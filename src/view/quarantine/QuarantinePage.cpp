#include "view/quarantine/QuarantinePage.h"

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/quarantine/RestoreDialog.h"
#include "view/delegates/RowDelegate.h"
#include "view/TableColumns.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ModelRowDetail.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"

QuarantinePage::QuarantinePage(QuarantineViewModel& viewModel, QuarantineModel& model, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    table_ = new QTableView(this);
    table_->setModel(&model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setItemDelegate(new RowDelegate(table_));
    table_->setShowGrid(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(table_);
    table_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(table_->horizontalHeader());

    restore_ = new QPushButton(tr("Restaurar selecionados"), this);
    discard_ = new QPushButton(tr("Descartar selecionados"), this);
    empty_ = new QPushButton(tr("Esvaziar a quarentena"), this);
    empty_->setProperty("role", "destructive");

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(restore_);
    bar->addWidget(discard_);
    bar->addStretch();
    bar->addWidget(empty_);

    panel_ = new ContextPanel(tr("Item retido"), 400, this);
    panel_->setObjectName(QStringLiteral("QuarantineItemPanel"));
    detail_ = new ModelRowDetail(panel_);
    restoreFromPanel_ = new QPushButton(tr("Restaurar para a biblioteca"), panel_);
    restoreFromPanel_->setProperty("role", "primary");
    openFolder_ = new QPushButton(tr("Abrir a pasta"), panel_);
    auto* promise = new QLabel(tr("Nada sai da quarentena sem você mandar."), panel_);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);
    panel_->Add(detail_);
    panel_->Add(restoreFromPanel_);
    panel_->Add(openFolder_);
    panel_->Add(promise);
    panel_->RestoreCollapsedState();
    panel_->Summon(false);

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(toolbar);
    column->addWidget(table_, 1);

    auto* held = new QWidget(this);
    auto* heldLayout = new QHBoxLayout(held);
    heldLayout->setContentsMargins(0, 0, 0, 0);
    heldLayout->setSpacing(0);
    heldLayout->addLayout(column, 1);
    heldLayout->addWidget(panel_);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(held);
    pages_->addWidget(new EmptyState(tr("A quarentena está vazia."),
                                     tr("Quando duas cópias do mesmo addon disputam o mesmo nome, a perdedora "
                                        "vem para cá em vez de ser apagada. Nada foi retido até agora."),
                                     this));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pages_);

    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &QuarantinePage::ShowTheSelectedItem);
    connect(&model_, &QAbstractItemModel::modelReset, this, &QuarantinePage::ShowTheSelectedItem);
    connect(panel_, &ContextPanel::CloseRequested, table_->selectionModel(), &QItemSelectionModel::clearSelection);
    connect(openFolder_, &QPushButton::clicked, this, &QuarantinePage::OpenTheSelectedFolder);
    connect(restoreFromPanel_, &QPushButton::clicked, this, &QuarantinePage::RestoreSelected);
    connect(restore_, &QPushButton::clicked, this, &QuarantinePage::RestoreSelected);
    connect(discard_, &QPushButton::clicked, this, &QuarantinePage::DiscardSelected);
    connect(empty_, &QPushButton::clicked, this, &QuarantinePage::EmptyTheQuarantine);
    connect(&model_, &QAbstractItemModel::modelReset, this, &QuarantinePage::UpdateSummary);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]
            {
                UpdateSummary();
            });

    connect(&viewModel_, &QuarantineViewModel::Restored, this,
            [this](const std::vector<FileOperationResult>& results)
            {
                Report(tr("Restauração"), results);
            });
    connect(&viewModel_, &QuarantineViewModel::Discarded, this,
            [this](const std::vector<FileOperationResult>& results)
            {
                Report(tr("Descarte"), results);
            });

    UpdateSummary();
}

void QuarantinePage::ShowTheSelectedItem()
{
    const QModelIndexList rows = table_->selectionModel()->selectedRows();

    panel_->Summon(!rows.isEmpty());
    ShowWhatTheActionsWillTouch(rows);

    if (rows.size() > 1)
    {
        ShowTheSelectedBatch(rows);
        return;
    }

    if (!rows.isEmpty())
    {
        detail_->Show(rows.front());
        panel_->ShowTitle(model_.data(rows.front(), Qt::DisplayRole).toString());
    }
}

void QuarantinePage::ShowTheSelectedBatch(const QModelIndexList& rows) const
{
    int known = 0;
    QSet<QString> origins;

    for (const QModelIndex& position : rows)
    {
        const QuarantinedItem* item = model_.ItemAt(position);

        if (item == nullptr || !item->KnowsWhereItCameFrom())
        {
            continue;
        }

        ++known;
        origins.insert(AsText(item->origin.parent_path()));
    }

    const auto held = static_cast<int>(rows.size());

    QList<ModelRowDetail::Field> fields;
    fields.append({tr("Itens"), QString::number(held)});
    fields.append({tr("Sabem de onde vieram"), tr("%1 de %2").arg(known).arg(held)});
    fields.append({tr("Voltam para"), tr("%n lugar(es)", nullptr, origins.size())});

    panel_->ShowTitle(tr("%n item(ns) selecionado(s)", nullptr, held));
    detail_->ShowFields(fields);
}

void QuarantinePage::ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const
{
    const auto held = static_cast<int>(rows.size());

    restoreFromPanel_->setEnabled(held > 0);
    restoreFromPanel_->setText(held > 1 ? tr("Restaurar %n itens para a biblioteca", nullptr, held)
                                        : tr("Restaurar para a biblioteca"));
    openFolder_->setEnabled(held == 1);
}

std::vector<QuarantinedItem> QuarantinePage::Selected() const
{
    std::vector<QuarantinedItem> items;

    for (const QModelIndex& position : table_->selectionModel()->selectedRows())
    {
        if (const QuarantinedItem* item = model_.ItemAt(position))
        {
            items.push_back(*item);
        }
    }

    return items;
}

void QuarantinePage::RestoreSelected()
{
    const std::vector<QuarantinedItem> items = Selected();
    if (items.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um item da quarentena."));
        return;
    }

    RestoreDialog dialog(items, this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    viewModel_.Restore(dialog.Restorable());
}

void QuarantinePage::DiscardSelected()
{
    const std::vector<QuarantinedItem> items = Selected();
    if (items.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um item da quarentena."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Descartar da quarentena"),
        tr("%n item(ns) será(ão) apagado(s) do disco para sempre. Continuar?", nullptr, static_cast<int>(items.size())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Discard(items);
    }
}

void QuarantinePage::EmptyTheQuarantine()
{
    const std::vector<QuarantinedItem> items = model_.Items();
    if (items.empty())
    {
        emit StatusChanged(tr("A quarentena já está vazia."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Esvaziar a quarentena"),
        tr("Tudo que está na quarentena, %n item(ns), será apagado do disco para sempre. Continuar?", nullptr,
           static_cast<int>(items.size())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Discard(items);
    }
}

void QuarantinePage::Report(const QString& title, const std::vector<FileOperationResult>& results)
{
    QStringList failed;
    for (const FileOperationResult& result : results)
    {
        if (!Succeeded(result.result))
        {
            failed.append(Describe(result));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (failed.isEmpty())
    {
        emit StatusChanged(tr("%n item(ns) da quarentena.", nullptr, done));
        return;
    }

    QMessageBox report(QMessageBox::Warning, title,
                       tr("%n item(ns) não pôde(puderam) ser tratado(s).", nullptr, static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n item(ns) concluído(s).", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();
}

void QuarantinePage::OpenTheSelectedFolder() const
{
    const QModelIndexList rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty())
    {
        return;
    }

    if (const QuarantinedItem* item = model_.ItemAt(rows.front()); item != nullptr)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(item->path)));
    }
}

void QuarantinePage::UpdateSummary()
{
    const int rows = model_.rowCount({});
    const auto selected = static_cast<int>(Selected().size());

    pages_->setCurrentIndex(rows == 0 ? 1 : 0);

    emit SummaryChanged(rows == 0 ? tr("0 itens na quarentena") : tr("%n item(ns) na quarentena.", nullptr, rows));
    emit AsideChanged(rows == 0 ? tr("0 bytes retidos") : tr("nada sai daqui sem você mandar"));

    restore_->setEnabled(selected > 0);
    discard_->setEnabled(selected > 0);
    empty_->setEnabled(rows > 0);
}
