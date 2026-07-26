#include "view/CommunityPage.h"

#include <algorithm>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/ConflictDialog.h"
#include "view/ImportDialog.h"
#include "view/RepairDialog.h"
#include "view/TableColumns.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr int kConflictFilter = -1;

    QString ReasonImportIsOff(const int importable, const int conflicted, const int selected)
    {
        if (importable > 0)
        {
            return QObject::tr("Copia as pastas selecionadas para a biblioteca e deixa um link no lugar.");
        }

        if (selected == 0)
        {
            return QObject::tr("Selecione uma entrada não gerenciada para importar.");
        }

        if (conflicted > 0)
        {
            return QObject::tr("A seleção está em conflito: já existe um addon de mesmo nome na "
                "biblioteca. Resolva o conflito antes de importar.");
        }

        return QObject::tr("Só entrada não gerenciada pode ser importada. Link já gerenciado, "
            "quebrado ou de outro programa não entra.");
    }
}

CommunityPage::CommunityPage(CommunityViewModel& viewModel,
                             ImportViewModel& importViewModel,
                             CommunityModel& model,
                             QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), importViewModel_(importViewModel), model_(model)
{
    filter_ = new CommunityFilterModel(this);
    filter_->setSourceModel(&model_);

    table_ = new QTableView(this);
    table_->setModel(filter_);
    table_->setSortingEnabled(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    LetTheColumnsBeDraggedAndStillFillTheTable(table_);
    table_->verticalHeader()->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(CreateActions());
    layout->addWidget(table_, 1);

    connect(classes_, &QComboBox::activated, this, &CommunityPage::OnFilterChanged);
    connect(repair_, &QPushButton::clicked, this, &CommunityPage::StartRepair);
    connect(import_, &QPushButton::clicked, this, &CommunityPage::StartImport);
    connect(resolve_, &QPushButton::clicked, this, &CommunityPage::ResolveTheSelectedConflict);
    connect(&model_, &QAbstractItemModel::modelReset, this, &CommunityPage::UpdateSummary);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { UpdateSummary(); });
    connect(&viewModel_, &CommunityViewModel::RepairFinished, this,
            &CommunityPage::OnRepairFinished);
    connect(&importViewModel_, &ImportViewModel::Started, this, &CommunityPage::OnImportStarted);
    connect(&importViewModel_, &ImportViewModel::Progressed, this, &CommunityPage::OnImportProgressed);
    connect(&importViewModel_, &ImportViewModel::StepChanged, this, &CommunityPage::OnImportStep);
    connect(&importViewModel_, &ImportViewModel::Finished, this, &CommunityPage::OnImportFinished);

    UpdateSummary();
}

QWidget* CommunityPage::CreateActions()
{
    auto* bar = new QWidget(this);

    summary_ = new QLabel(bar);

    classes_ = new QComboBox(bar);
    classes_->addItem(tr("Todas as classes"), QVariant());
    classes_->addItem(tr("Gerenciada"), static_cast<int>(EntryClassification::Managed));
    classes_->addItem(tr("Externa"), static_cast<int>(EntryClassification::External));
    classes_->addItem(tr("Quebrada"), static_cast<int>(EntryClassification::Broken));
    classes_->addItem(tr("Indisponível"), static_cast<int>(EntryClassification::Unavailable));
    classes_->addItem(tr("Não gerenciada"), static_cast<int>(EntryClassification::Unmanaged));
    classes_->addItem(tr("Duplicada"), static_cast<int>(EntryClassification::Duplicated));
    classes_->addItem(tr("Em conflito"), kConflictFilter);

    import_ = new QPushButton(tr("Importar selecionados..."), bar);
    resolve_ = new QPushButton(tr("Resolver conflito..."), bar);
    repair_ = new QPushButton(tr("Reparar quebrados..."), bar);

    import_->setEnabled(false);
    resolve_->setEnabled(false);
    repair_->setEnabled(false);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(summary_, 1);
    layout->addWidget(classes_);
    layout->addWidget(import_);
    layout->addWidget(resolve_);
    layout->addWidget(repair_);

    return bar;
}

void CommunityPage::OnFilterChanged(const int index) const
{
    const QVariant chosen = classes_->itemData(index);

    if (chosen.isValid() && chosen.toInt() == kConflictFilter)
    {
        filter_->ShowOnlyTheConflicted(true);
        return;
    }

    filter_->ShowOnly(chosen.isValid()
                          ? std::optional(static_cast<EntryClassification>(chosen.toInt()))
                          : std::nullopt);
}

bool CommunityPage::TheSimulatorIsInTheWay()
{
    while (const std::optional<std::string> running = importViewModel_.RunningSimulator())
    {
        QMessageBox blocked(QMessageBox::Warning, tr("Simulador aberto"),
                            tr("Operações de arquivo ficam bloqueadas enquanto o simulador roda."),
                            QMessageBox::Cancel, this);
        blocked.setInformativeText(tr("Feche %1 e verifique de novo.")
                                   .arg(QString::fromStdString(*running)));

        const QPushButton* again = blocked.addButton(tr("Verificar de novo"), QMessageBox::AcceptRole);
        blocked.exec();

        if (blocked.clickedButton() != again)
        {
            return true;
        }
    }

    return false;
}

void CommunityPage::StartImport()
{
    std::vector<std::filesystem::path> folders;
    int conflicted = 0;

    for (const QModelIndex& position : table_->selectionModel()->selectedRows())
    {
        const QModelIndex source = filter_->mapToSource(position);
        const DestinationEntry* entry = model_.EntryAt(source);

        if (entry == nullptr || entry->classification != EntryClassification::Unmanaged)
        {
            continue;
        }

        if (model_.ConflictAt(source) != nullptr)
        {
            ++conflicted;
            continue;
        }

        folders.push_back(entry->path);
    }

    if (folders.empty())
    {
        emit StatusChanged(conflicted > 0
                               ? tr("Resolva o conflito antes de importar: a biblioteca já tem um addon "
                                   "com esse nome.")
                               : tr("Selecione ao menos uma pasta não gerenciada."));
        return;
    }

    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    ImportDialog dialog(folders, viewModel_.Snapshot().libraries, importViewModel_.Profile(),
                        importViewModel_.TotalSizeOf(folders), this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    importViewModel_.Import(dialog.ChosenRequests());
}

void CommunityPage::ResolveTheSelectedConflict()
{
    std::vector<CopyConflict> conflicts;

    for (const QModelIndex& position : table_->selectionModel()->selectedRows())
    {
        if (const CopyConflict* conflict = model_.ConflictAt(filter_->mapToSource(position)))
        {
            conflicts.push_back(*conflict);
        }
    }

    if (conflicts.empty())
    {
        emit StatusChanged(tr("Selecione uma entrada marcada como em conflito."));
        return;
    }

    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    int resolved = 0;
    std::size_t asked = 0;

    for (const CopyConflict& conflict : conflicts)
    {
        const std::optional<ImportResult> result = ResolveOneConflict(conflict, ++asked, conflicts.size());
        if (!result.has_value())
        {
            --asked;
            break;
        }

        resolved += *result == ImportResult::Completed ? 1 : 0;
    }

    viewModel_.Show();

    emit StatusChanged(asked == conflicts.size()
                           ? tr("%n conflito(s) resolvido(s).", nullptr, resolved)
                           : tr("%n conflito(s) resolvido(s), e os outros continuam abertos.",
                                nullptr, resolved));
}

void CommunityPage::ResolveConflict(const CopyConflict& conflict)
{
    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    if (ResolveOneConflict(conflict, 1, 1).has_value())
    {
        viewModel_.Show();
    }
}

std::optional<ImportResult> CommunityPage::ResolveOneConflict(const CopyConflict& conflict,
                                                              const std::size_t position,
                                                              const std::size_t total)
{
    ConflictDialog dialog(importViewModel_.DetailsOf(conflict), this);
    if (total > 1)
    {
        dialog.setWindowTitle(tr("Duas cópias do mesmo addon (%1 de %2)")
                              .arg(position)
                              .arg(total));
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const ImportResult result = importViewModel_.ResolveConflict(conflict, dialog.Choice());

    if (result != ImportResult::Completed)
    {
        QMessageBox::warning(this, tr("O conflito continua"),
                             result == ImportResult::CouldNotRemoveTheLink
                                 ? tr("Nenhuma pasta foi movida: %1.").arg(Explain(result))
                                 : tr("Nada foi apagado: %1.").arg(Explain(result)));
    }

    return result;
}

void CommunityPage::StartRepair()
{
    const std::vector<RepairCandidate> candidates = viewModel_.PlanRepairs();

    if (candidates.empty())
    {
        emit StatusChanged(tr("Nenhum link quebrado para reparar."));
        return;
    }

    RepairDialog dialog(candidates, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    viewModel_.Repair(dialog.ChosenRequests());
}

void CommunityPage::OnImportStarted(const int folders)
{
    folders_ = folders;
    folder_ = 0;
    step_ = NameOfImportStep(OperationKind::ImportCopyToStaging);

    progress_ = new QProgressDialog(step_, tr("Cancelar"), 0, 100, this);
    progress_->setWindowModality(Qt::ApplicationModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    progress_->setValue(0);

    connect(progress_, &QProgressDialog::canceled, &importViewModel_, &ImportViewModel::Cancel);
}

void CommunityPage::OnImportProgressed(const qulonglong copiedBytes, const qulonglong totalBytes,
                                       const int folder)
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->setRange(0, 100);
    progress_->setLabelText(tr("%1 · %2 · %3 de %4")
                            .arg(tr("Pasta %1 de %2").arg(folder).arg(folders_), step_,
                                 AsSize(copiedBytes), AsSize(totalBytes)));

    progress_->setValue(totalBytes == 0 ? 0 : static_cast<int>(copiedBytes * 100 / totalBytes));
}

void CommunityPage::OnImportStep(const QString& step)
{
    const bool copying = step == NameOfImportStep(OperationKind::ImportCopyToStaging);

    step_ = step;
    folder_ += copying ? 1 : 0;

    if (progress_ == nullptr)
    {
        return;
    }

    progress_->setLabelText(tr("Pasta %1 de %2 · %3").arg(folder_).arg(folders_).arg(step_));

    if (!copying)
    {
        progress_->setRange(0, 0);
    }
}

void CommunityPage::OnImportFinished(const std::vector<ImportOperationResult>& results)
{
    if (progress_ != nullptr)
    {
        progress_->close();
        progress_->deleteLater();
        progress_ = nullptr;
    }

    QStringList failed;
    for (const ImportOperationResult& result : results)
    {
        if (result.result != ImportResult::Completed)
        {
            failed.append(QStringLiteral("%1 — %2").arg(AsText(result.request.source.filename()),
                                                        Explain(result.result)));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (!failed.isEmpty())
    {
        QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi importado"),
                           tr("%n importação(ões) não terminou(aram).", nullptr,
                              static_cast<int>(failed.size())),
                           QMessageBox::Ok, this);
        report.setInformativeText(tr("%n addon(s) agora mora(m) na biblioteca.", nullptr, done));
        report.setDetailedText(failed.join('\n'));
        report.exec();
    }

    emit StatusChanged(tr("%n addon(s) importado(s) para a biblioteca.", nullptr, done));
}

void CommunityPage::OnRepairFinished(const std::vector<LinkOperationResult>& results)
{
    QStringList failed;
    for (const LinkOperationResult& result : results)
    {
        if (!result.outcome.Succeeded())
        {
            failed.append(Describe(result));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (failed.isEmpty())
    {
        emit StatusChanged(tr("%n reparo(s) concluído(s).", nullptr, done));
        return;
    }

    QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi reparado"),
                       tr("%n reparo(s) falhou(aram).", nullptr,
                          static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n reparo(s) concluído(s).", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();

    emit StatusChanged(tr("%1 · %2")
        .arg(tr("%n reparo(s) concluído(s)", nullptr, done),
             tr("%n falhou(aram)", nullptr, static_cast<int>(failed.size()))));
}

void CommunityPage::UpdateSummary()
{
    const int rows = model_.rowCount({});
    int broken = 0;
    int managed = 0;
    int conflicted = 0;

    for (int row = 0; row < rows; ++row)
    {
        const QModelIndex position = model_.index(row, 0);
        const auto classification = static_cast<EntryClassification>(
            model_.data(position, CommunityModel::ClassificationRole).toInt());

        broken += classification == EntryClassification::Broken ? 1 : 0;
        managed += classification == EntryClassification::Managed ? 1 : 0;
        conflicted += model_.data(position, CommunityModel::ConflictRole).toBool() ? 1 : 0;
    }

    summary_->setText(tr("%1 · %2 · %3 · %4")
        .arg(tr("%n entrada(s)", nullptr, rows),
             tr("%n gerenciada(s)", nullptr, managed),
             tr("%n quebrada(s)", nullptr, broken),
             tr("%n em conflito", nullptr, conflicted)));

    repair_->setEnabled(broken > 0);

    int importable = 0;
    int selectedConflicts = 0;
    for (const QModelIndex& position : table_->selectionModel()->selectedRows())
    {
        const QModelIndex source = filter_->mapToSource(position);
        const DestinationEntry* entry = model_.EntryAt(source);
        const bool inConflict = model_.ConflictAt(source) != nullptr;

        selectedConflicts += inConflict ? 1 : 0;
        importable += entry != nullptr && entry->classification == EntryClassification::Unmanaged
                      && !inConflict
                          ? 1
                          : 0;
    }

    import_->setEnabled(importable > 0);
    import_->setToolTip(ReasonImportIsOff(importable, selectedConflicts,
                                          table_->selectionModel()->selectedRows().size()));
    resolve_->setEnabled(selectedConflicts > 0);
}
