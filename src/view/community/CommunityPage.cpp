#include "view/community/CommunityPage.h"

#include <algorithm>

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/community/ConflictDialog.h"
#include "view/community/ImportDialog.h"
#include "view/community/RepairDialog.h"
#include "view/delegates/RowDelegate.h"
#include "view/TableColumns.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/ModelRowDetail.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kConflictFilter = -1;
    constexpr int kEveryFilter = -2;
    constexpr int kChipsPerRow = 4;
    constexpr int kLeadColumn = 0;
    constexpr int kGroupGapColumn = 1;
    constexpr int kGroupGap = 8;

    struct ImportableSelection
    {
        std::vector<std::filesystem::path> folders;
        int conflicted = 0;
        int selected = 0;
    };

    ImportableSelection
    ChosenForImport(const QTableView& table, const QSortFilterProxyModel& filter, const CommunityModel& model)
    {
        const QModelIndexList rows = table.selectionModel()->selectedRows();

        ImportableSelection chosen;
        chosen.selected = static_cast<int>(rows.size());

        for (const QModelIndex& position : rows)
        {
            const QModelIndex source = filter.mapToSource(position);
            const DestinationEntry* entry = model.EntryAt(source);

            if (model.ConflictAt(source) != nullptr)
            {
                ++chosen.conflicted;
                continue;
            }

            if (entry != nullptr && entry->classification == EntryClassification::Unmanaged)
            {
                chosen.folders.push_back(entry->path);
            }
        }

        return chosen;
    }

    void Emphasise(QPushButton& button, const bool primary)
    {
        button.setProperty("role", primary ? "primary" : "secondary");
        button.style()->unpolish(&button);
        button.style()->polish(&button);
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
    table_->setItemDelegate(new RowDelegate(table_));
    table_->setShowGrid(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(table_);
    table_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(table_->horizontalHeader());

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(CreateFilters());
    column->addWidget(CreateActions());
    column->addWidget(table_, 1);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(column, 1);
    layout->addWidget(CreatePanel());

    connect(&model_, &QAbstractItemModel::modelReset, this, &CommunityPage::UpdateSummary);
    connect(&model_, &QAbstractItemModel::modelReset, this, &CommunityPage::ShowTheSelectedEntry);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]
            {
                ShowTheSelectedEntry();
                UpdateSummary();
            });
    connect(&viewModel_, &CommunityViewModel::RepairFinished, this, &CommunityPage::OnRepairFinished);
    connect(&model_, &QAbstractItemModel::modelReset, &importViewModel_, &ImportViewModel::ForgetMeasuredSizes);
    connect(&importViewModel_, &ImportViewModel::SizeMeasuring, this,
            [this]
            {
                if (batch_)
                {
                    ShowTheBatchFields(tr("medindo…"));
                }
            });
    connect(&importViewModel_, &ImportViewModel::SizeMeasured, this,
            [this](const qulonglong bytes)
            {
                if (batch_)
                {
                    ShowTheBatchFields(AsSize(bytes));
                }
            });
    connect(&importViewModel_, &ImportViewModel::Started, this, &CommunityPage::OnImportStarted);
    connect(&importViewModel_, &ImportViewModel::Progressed, this, &CommunityPage::OnImportProgressed);
    connect(&importViewModel_, &ImportViewModel::StepChanged, this, &CommunityPage::OnImportStep);
    connect(&importViewModel_, &ImportViewModel::Finished, this, &CommunityPage::OnImportFinished);

    UpdateSummary();
}

QWidget* CommunityPage::CreateFilters()
{
    auto* bar = new QWidget(this);

    const struct
    {
        QString label;
        int filter;
    } wanted[] = {
        {tr("Todas"), kEveryFilter},
        {tr("Gerenciada"), static_cast<int>(EntryClassification::Managed)},
        {tr("Externa"), static_cast<int>(EntryClassification::External)},
        {tr("Quebrada"), static_cast<int>(EntryClassification::Broken)},
        {tr("Não gerenciada"), static_cast<int>(EntryClassification::Unmanaged)},
        {tr("Indisponível"), static_cast<int>(EntryClassification::Unavailable)},
        {tr("Duplicada"), static_cast<int>(EntryClassification::Duplicated)},
        {tr("Em conflito"), kConflictFilter},
    };

    auto* group = new QButtonGroup(bar);
    auto* grid = new QGridLayout(bar);
    grid->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, 0);
    grid->setSpacing(6);

    int column = 0;
    int row = 0;

    for (const auto& [label, filter] : wanted)
    {
        auto* chip = new QToolButton(bar);
        chip->setObjectName(QStringLiteral("FilterChip"));
        chip->setText(label);
        chip->setProperty("label", label);
        chip->setProperty("filter", filter);
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);

        group->addButton(chip);
        grid->addWidget(chip, row, column == kLeadColumn ? column : column + 1);
        chips_.append(chip);

        if (++column == kChipsPerRow)
        {
            column = 0;
            ++row;
        }
    }

    grid->setColumnMinimumWidth(kGroupGapColumn, kGroupGap);
    grid->setColumnStretch(kChipsPerRow + 1, 1);
    chips_.front()->setChecked(true);

    connect(group, &QButtonGroup::buttonClicked, this,
            [this](const QAbstractButton* chip)
            {
                ApplyFilter(chip->property("filter").toInt());
            });

    return bar;
}

QWidget* CommunityPage::CreateActions()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("PageToolbar"));

    auto* selectAll = new QPushButton(tr("Selecionar tudo que o filtro mostra"), bar);

    auto* search = new QLineEdit(bar);
    search->setPlaceholderText(tr("Filtrar entradas"));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(180);
    search->setMaximumWidth(240);

    connect(selectAll, &QPushButton::clicked, table_, &QTableView::selectAll);
    connect(search, &QLineEdit::textChanged, filter_, &QSortFilterProxyModel::setFilterFixedString);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(8);
    layout->addWidget(selectAll);
    layout->addStretch();
    layout->addWidget(search);

    return bar;
}

QWidget* CommunityPage::CreatePanel()
{
    panel_ = new ContextPanel(tr("Entrada selecionada"), 380, this);
    panel_->setObjectName(QStringLiteral("CommunityEntryPanel"));

    detail_ = new ModelRowDetail(panel_);

    importOne_ = new QPushButton(tr("Importar esta pasta…"), panel_);
    importOne_->setObjectName(QStringLiteral("ImportChosen"));
    importOne_->setProperty("role", "primary");

    resolveChosen_ = new QPushButton(tr("Resolver o conflito…"), panel_);
    resolveChosen_->setObjectName(QStringLiteral("ResolveChosen"));

    openFolder_ = new QPushButton(tr("Abrir a pasta"), panel_);

    auto* promise = new QLabel(tr("Importar copia para a biblioteca e deixa um link no lugar. A pasta "
                                  "original só é removida depois da verificação."),
                               panel_);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    panel_->Add(detail_);
    panel_->Add(resolveChosen_);
    panel_->Add(importOne_);
    panel_->Add(openFolder_);
    panel_->Add(promise);

    panel_->RestoreCollapsedState();
    panel_->Summon(false);
    ShowWhatTheActionsWillTouch({});

    connect(importOne_, &QPushButton::clicked, this, &CommunityPage::StartImport);
    connect(resolveChosen_, &QPushButton::clicked, this, &CommunityPage::ResolveTheSelectedConflict);
    connect(openFolder_, &QPushButton::clicked, this, &CommunityPage::OpenTheSelectedFolder);
    connect(panel_, &ContextPanel::CloseRequested, table_->selectionModel(), &QItemSelectionModel::clearSelection);

    return panel_;
}

void CommunityPage::ShowTheSelectedEntry()
{
    const QModelIndexList rows = table_->selectionModel()->selectedRows();

    panel_->Summon(!rows.isEmpty());
    ShowWhatTheActionsWillTouch(rows);

    if (rows.isEmpty())
    {
        return;
    }

    if (rows.size() > 1)
    {
        ShowTheSelectedBatch(rows);
        return;
    }

    batch_ = false;

    const QModelIndex source = filter_->mapToSource(rows.front());
    const DestinationEntry* entry = model_.EntryAt(source);

    if (entry == nullptr)
    {
        return;
    }

    QList<ModelRowDetail::Field> fields;
    fields.append({tr("Classificação"), CommunityModel::ClassificationName(entry->classification)});
    fields.append({tr("Destino"), AsText(entry->path.parent_path().filename())});
    fields.append({tr("Caminho"), AsText(entry->path)});
    fields.append({tr("É link?"), entry->target.empty() ? tr("não, pasta física") : AsText(entry->target)});

    if (const CopyConflict* conflict = model_.ConflictAt(source); conflict != nullptr)
    {
        fields.append({tr("Na biblioteca"), AsText(conflict->libraryPath)});
    }

    panel_->ShowTitle(AsText(entry->path.filename()), model_.data(source, AlarmingRole).toBool());
    detail_->ShowFields(fields);
}

void CommunityPage::ShowTheSelectedBatch(const QModelIndexList& rows)
{
    batch_ = true;

    QHash<int, int> counted;
    QSet<QString> destinations;
    bool alarming = false;

    for (const QModelIndex& position : rows)
    {
        const QModelIndex source = filter_->mapToSource(position);

        ++counted[model_.data(source, CommunityModel::ClassificationRole).toInt()];
        counted[kConflictFilter] += model_.data(source, CommunityModel::ConflictRole).toBool() ? 1 : 0;
        alarming = alarming || model_.data(source, AlarmingRole).toBool();

        if (const DestinationEntry* entry = model_.EntryAt(source); entry != nullptr)
        {
            destinations.insert(AsText(entry->path.parent_path().filename()));
        }
    }

    counted_.clear();

    for (const QToolButton* chip : chips_)
    {
        const int filter = chip->property("filter").toInt();
        const int population = counted.value(filter);

        if (filter == kEveryFilter || population == 0)
        {
            continue;
        }

        counted_.append({chip->property("label").toString(), QString::number(population)});
    }

    counted_.append({tr("Destinos"), QString::number(destinations.size())});

    panel_->ShowTitle(tr("%n entrada(s) selecionada(s)", nullptr, static_cast<int>(rows.size())), alarming);

    const std::vector<std::filesystem::path> importable = ChosenForImport(*table_, *filter_, model_).folders;

    ShowTheBatchFields(importable.empty() ? QString() : tr("medindo…"));

    if (!importable.empty())
    {
        importViewModel_.MeasureTotalSize(importable);
    }
}

void CommunityPage::ShowTheBatchFields(const QString& size) const
{
    QList<ModelRowDetail::Field> fields = counted_;

    if (!size.isEmpty())
    {
        fields.append({tr("Tamanho em disco"), size});
    }

    detail_->ShowFields(fields);
}

void CommunityPage::ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const
{
    const ImportableSelection chosen = ChosenForImport(*table_, *filter_, model_);
    const auto importable = static_cast<int>(chosen.folders.size());
    const int blocked = chosen.conflicted;

    importOne_->setEnabled(importable > 0);
    importOne_->setText(importable > 1 ? tr("Importar as %n pastas…", nullptr, importable)
                                       : tr("Importar esta pasta…"));

    resolveChosen_->setVisible(blocked > 0);
    resolveChosen_->setText(blocked > 1 ? tr("Resolver os %n conflitos…", nullptr, blocked)
                                        : tr("Resolver o conflito…"));

    Emphasise(*resolveChosen_, blocked > 0);
    Emphasise(*importOne_, blocked == 0);

    openFolder_->setEnabled(rows.size() == 1);
}

void CommunityPage::OpenTheSelectedFolder() const
{
    const QModelIndexList rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty())
    {
        return;
    }

    if (const DestinationEntry* entry = model_.EntryAt(filter_->mapToSource(rows.front())); entry != nullptr)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(entry->path)));
    }
}

void CommunityPage::FilterBy(const EntryClassification classification) const
{
    ShowFilter(static_cast<int>(classification));
}

void CommunityPage::FilterByConflicted() const
{
    ShowFilter(kConflictFilter);
}

void CommunityPage::SelectEverythingShown() const
{
    table_->selectAll();
}

void CommunityPage::ShowFilter(const int filter) const
{
    for (QToolButton* chip : chips_)
    {
        if (chip->property("filter").toInt() == filter)
        {
            chip->setChecked(true);
            ApplyFilter(filter);
            return;
        }
    }
}

void CommunityPage::ApplyFilter(const int filter) const
{
    if (filter == kConflictFilter)
    {
        filter_->ShowOnlyTheConflicted(true);
        return;
    }

    filter_->ShowOnly(filter == kEveryFilter ? std::nullopt : std::optional(static_cast<EntryClassification>(filter)));
}

bool CommunityPage::TheSimulatorIsInTheWay()
{
    while (const std::optional<std::string> running = importViewModel_.RunningSimulator())
    {
        QMessageBox blocked(QMessageBox::Warning, tr("Simulador aberto"),
                            tr("Operações de arquivo ficam bloqueadas enquanto o simulador roda."), QMessageBox::Cancel,
                            this);
        blocked.setInformativeText(tr("Feche %1 e verifique de novo.").arg(QString::fromStdString(*running)));

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
    const ImportableSelection chosen = ChosenForImport(*table_, *filter_, model_);

    if (chosen.folders.empty())
    {
        emit StatusChanged(chosen.conflicted > 0
                               ? tr("Resolva o conflito antes de importar: a biblioteca já tem um addon "
                                    "com esse nome.")
                               : tr("Selecione ao menos uma pasta não gerenciada."));
        return;
    }

    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    ImportDialog dialog(chosen.folders, viewModel_.Snapshot().libraries, importViewModel_.Profile(),
                        importViewModel_.TotalSizeOf(chosen.folders), this);

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
        const std::optional<FileResult> result = ResolveOneConflict(conflict, ++asked, conflicts.size());
        if (!result.has_value())
        {
            --asked;
            break;
        }

        resolved += Succeeded(*result) ? 1 : 0;
    }

    viewModel_.Show();

    emit StatusChanged(asked == conflicts.size()
                           ? tr("%n conflito(s) resolvido(s).", nullptr, resolved)
                           : tr("%n conflito(s) resolvido(s), e os outros continuam abertos.", nullptr, resolved));
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

std::optional<FileResult>
CommunityPage::ResolveOneConflict(const CopyConflict& conflict, const std::size_t position, const std::size_t total)
{
    ConflictDialog dialog(importViewModel_.DetailsOf(conflict), this);
    if (total > 1)
    {
        dialog.setWindowTitle(tr("Duas cópias do mesmo addon (%1 de %2)").arg(position).arg(total));
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const FileResult result = importViewModel_.ResolveConflict(conflict, dialog.Choice());

    if (!Succeeded(result))
    {
        QMessageBox::warning(this, tr("O conflito continua"),
                             result == FileResult::CouldNotRemoveTheLink
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

void CommunityPage::OnImportProgressed(const qulonglong copiedBytes, const qulonglong totalBytes, const int folder)
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->setRange(0, 100);
    progress_->setLabelText(
        tr("%1 · %2 · %3 de %4")
            .arg(tr("Pasta %1 de %2").arg(folder).arg(folders_), step_, AsSize(copiedBytes), AsSize(totalBytes)));

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
        if (!Succeeded(result.result))
        {
            failed.append(Describe(result));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (!failed.isEmpty())
    {
        QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi importado"),
                           tr("%n importação(ões) não terminou(aram).", nullptr, static_cast<int>(failed.size())),
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
                       tr("%n reparo(s) falhou(aram).", nullptr, static_cast<int>(failed.size())), QMessageBox::Ok,
                       this);
    report.setInformativeText(tr("%n reparo(s) concluído(s).", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();

    emit StatusChanged(tr("%1 · %2").arg(tr("%n reparo(s) concluído(s)", nullptr, done),
                                         tr("%n falhou(aram)", nullptr, static_cast<int>(failed.size()))));
}

void CommunityPage::FitTheChips()
{
    int lead = 0;
    int rest = 0;

    for (int position = 0; position < chips_.size(); ++position)
    {
        const int wanted = chips_[position]->sizeHint().width();
        int& widest = position % kChipsPerRow == kLeadColumn ? lead : rest;

        widest = std::max(widest, wanted);
    }

    for (int position = 0; position < chips_.size(); ++position)
    {
        chips_[position]->setFixedWidth(position % kChipsPerRow == kLeadColumn ? lead : rest);
    }
}

void CommunityPage::UpdateSummary()
{
    const int rows = model_.rowCount({});
    QHash<int, int> counted;

    for (int row = 0; row < rows; ++row)
    {
        const QModelIndex position = model_.index(row, 0);

        ++counted[model_.data(position, CommunityModel::ClassificationRole).toInt()];
        counted[kConflictFilter] += model_.data(position, CommunityModel::ConflictRole).toBool() ? 1 : 0;
    }

    counted[kEveryFilter] = rows;

    for (QToolButton* chip : chips_)
    {
        const int filter = chip->property("filter").toInt();
        const int population = counted.value(filter);

        chip->setText(QStringLiteral("%1 %2").arg(chip->property("label").toString()).arg(population));
        chip->setProperty("population", population == 0 ? "none" : "some");
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
    }

    FitTheChips();

    const int broken = counted.value(static_cast<int>(EntryClassification::Broken));
    const int managed = counted.value(static_cast<int>(EntryClassification::Managed));

    emit SummaryChanged(tr("%1 · %2 · %3 · %4")
                            .arg(tr("%n entrada(s)", nullptr, rows), tr("%n gerenciada(s)", nullptr, managed),
                                 tr("%n quebrada(s)", nullptr, broken),
                                 tr("%n em conflito", nullptr, counted.value(kConflictFilter))));

    emit AsideChanged(tr("%n destino(s)", nullptr, static_cast<int>(importViewModel_.Profile().destinations.size())));
}
