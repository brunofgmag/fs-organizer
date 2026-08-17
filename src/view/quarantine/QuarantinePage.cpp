#include "view/quarantine/QuarantinePage.h"

#include <QtCore/QEvent>
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
#include "view/quarantine/CollisionDialog.h"
#include "view/quarantine/DiscardProgressDialog.h"
#include "view/quarantine/RestoreDialog.h"
#include "view/delegates/RowDelegate.h"
#include "view/TableColumns.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ModelRowDetail.h"
#include "viewmodel/SizeSummary.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/ModelRetranslation.h"

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
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    DressTheHeaderOf(table_->horizontalHeader());

    restore_ = new QPushButton(this);
    restore_->setObjectName(QStringLiteral("RestoreChosen"));
    discard_ = new QPushButton(this);
    empty_ = new QPushButton(this);
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

    panel_ = new ContextPanel(tr("Item held"), 400, this);
    panel_->setObjectName(QStringLiteral("QuarantineItemPanel"));
    detail_ = new ModelRowDetail(panel_);
    restoreFromPanel_ = new QPushButton(panel_);
    restoreFromPanel_->setProperty("role", "primary");
    openFolder_ = new QPushButton(panel_);
    panel_->Add(detail_);
    panel_->Add(restoreFromPanel_);
    panel_->Add(openFolder_);
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
    nothingHeld_ = new EmptyState(this);
    pages_->addWidget(nothingHeld_);

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
                Report(tr("Restore", "the title of a report"), results);
            });
    connect(&viewModel_, &QuarantineViewModel::Swapped, this, &QuarantinePage::ReportTheSwaps);
    connect(&viewModel_, &QuarantineViewModel::DiscardStarted, this, &QuarantinePage::OpenTheProgress);
    connect(&viewModel_, &QuarantineViewModel::DiscardProgressed, this,
            [this](const int discarded, const int outOf)
            {
                if (progress_ != nullptr)
                {
                    progress_->ShowTheItem(discarded, outOf);
                }
            });
    connect(&viewModel_, &QuarantineViewModel::Discarded, this,
            [this](const std::vector<FileOperationResult>& results)
            {
                CloseTheProgress();
                Report(tr("Discard"), results);
            });

    RetranslateUi();
    UpdateSummary();
}

void QuarantinePage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        SayTheModelWasRetranslated(model_);
        UpdateSummary();
        ShowTheSelectedItem();
    }

    QWidget::changeEvent(event);
}

void QuarantinePage::RetranslateUi()
{
    restore_->setText(tr("Restore the selected ones"));
    discard_->setText(tr("Discard the selected ones"));
    empty_->setText(tr("Empty the quarantine"));
    openFolder_->setText(tr("Open the folder"));
    panel_->RenameTheFallback(tr("Item held"));
    nothingHeld_->Retell(tr("The quarantine is empty."),
                         tr("When two copies of the same addon fight over the same name, the losing one comes here "
                            "instead of being deleted. Nothing has been held so far."));
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

    if (rows.isEmpty())
    {
        return;
    }

    detail_->Show(rows.front(), WhatTheTableDoesNotShow(rows.front()));
    panel_->ShowTitle(model_.data(rows.front(), Qt::DisplayRole).toString(),
                      model_.data(rows.front(), QuarantineModel::ReplacedRole).toBool());
}

QList<ModelRowDetail::Field> QuarantinePage::WhatTheTableDoesNotShow(const QModelIndex& position) const
{
    const QuarantinedItem* item = model_.ItemAt(position);
    if (item == nullptr)
    {
        return {};
    }

    QList<ModelRowDetail::Field> fields;

    if (const QString size = model_.SizeOf(*item); !size.isEmpty())
    {
        fields.append({tr("Size on disk"), size});
    }

    if (const QString when = model_.WhenItWasQuarantined(*item); !when.isEmpty())
    {
        fields.append({tr("Quarantined on"), when});
    }

    fields.append({tr("Kept in"), AsText(item->path.parent_path())});
    fields.append(WhereEachSourcePoints(*item));
    fields.append(TheComparisonFor(position));

    return fields;
}

QList<ModelRowDetail::Field> QuarantinePage::WhereEachSourcePoints(const QuarantinedItem& item)
{
    if (!item.TheSourcesDisagree())
    {
        return {};
    }

    return {{tr("The record says"), AsText(item.origin)}, {tr("The Journal says"), AsText(item.theOtherSourceSays)}};
}

QList<ModelRowDetail::Field> QuarantinePage::TheComparisonFor(const QModelIndex& position) const
{
    const QuarantineDetail* detail = model_.DetailAt(position);
    if (detail == nullptr || !detail->WasReplaced())
    {
        return {};
    }

    return {{tr("Already in place"), AsText(detail->replacedBy)},
            {tr("Version there"),
             detail->replacementVersion.empty() ? tr("the manifest does not say")
                                                : QString::fromStdString(detail->replacementVersion)}};
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
    fields.append({tr("Size on disk"), SizeOfTheSelection(model_.TallyOf(rows))});
    fields.append({tr("Items"), QString::number(held)});
    fields.append({tr("Know where they came from"), tr("%1 of %2").arg(known).arg(held)});
    fields.append({tr("Go back to"), tr("%n place", nullptr, static_cast<int>(origins.size()))});

    panel_->ShowTitle(tr("%n item selected", nullptr, held));
    detail_->ShowFields(fields);
}

void QuarantinePage::ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const
{
    const auto held = static_cast<int>(rows.size());

    restoreFromPanel_->setEnabled(held > 0);
    restoreFromPanel_->setText(held > 1 ? tr("Restore %n item", nullptr, held) : tr("Restore"));
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
        emit StatusChanged(tr("Select at least one item from the quarantine."));
        return;
    }

    RestoreDialog dialog(
        viewModel_.WhatRestoringWouldDo(items),
        [this](const RestoreCheck& check)
        {
            return AskAboutTheCollision(check);
        },
        this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const std::vector<QuarantinedItem> going = dialog.Restorable();
    const std::vector<QuarantinedItem> replacing = dialog.TheOnesReplacingWhatIsThere();

    if (going.empty() && replacing.empty())
    {
        emit StatusChanged(tr("Nothing was restored."));
        return;
    }

    if (!going.empty())
    {
        viewModel_.Restore(going);
    }

    if (!replacing.empty())
    {
        viewModel_.Swap(replacing);
    }
}

void QuarantinePage::DiscardSelected()
{
    const std::vector<QuarantinedItem> items = Selected();
    if (items.empty())
    {
        emit StatusChanged(tr("Select at least one item from the quarantine."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Discard from the quarantine"),
        tr("%n item will be deleted from the disk for good. Continue?", nullptr, static_cast<int>(items.size())),
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
        emit StatusChanged(tr("The quarantine is already empty."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Empty the quarantine"),
        tr("Everything in the quarantine, %n item, will be deleted from the disk for good. Continue?", nullptr,
           static_cast<int>(items.size())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Discard(items);
    }
}

void QuarantinePage::OpenTheProgress(const int items)
{
    CloseTheProgress();

    progress_ = new DiscardProgressDialog(items, this);
    progress_->show();
}

void QuarantinePage::CloseTheProgress()
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->close();
    progress_->deleteLater();
    progress_ = nullptr;
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
        emit StatusChanged(tr("%n item from the quarantine.", nullptr, done));
        return;
    }

    QMessageBox report(QMessageBox::Warning, title,
                       tr("%n item could not be handled.", nullptr, static_cast<int>(failed.size())), QMessageBox::Ok,
                       this);
    report.setInformativeText(tr("%n item finished.", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();
}

bool QuarantinePage::AskAboutTheCollision(const RestoreCheck& check)
{
    CollisionDialog dialog(check, this);

    viewModel_.WeighBothSidesOf(check,
                                [&dialog](const TwoSides& sides)
                                {
                                    dialog.ShowTheSizes(sides);
                                });

    return dialog.exec() == QDialog::Accepted;
}

void QuarantinePage::ReportTheSwaps(const std::vector<SwapResult>& results)
{
    QStringList told;
    int stopped = 0;

    for (const SwapResult& result : results)
    {
        told.append(Describe(result));
        stopped += result.Succeeded() ? 0 : 1;
    }

    if (stopped == 0)
    {
        emit StatusChanged(tr("%n item replaced what was in its place.", nullptr, static_cast<int>(results.size())));
        return;
    }

    QMessageBox report(QMessageBox::Warning, tr("Replace what's there"),
                       tr("%n replacement stopped part of the way.", nullptr, stopped), QMessageBox::Ok, this);
    report.setInformativeText(tr("Nothing was deleted. The detail says where each one stopped."));
    report.setDetailedText(told.join('\n'));
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

    emit SummaryChanged(rows == 0 ? tr("0 items in the quarantine") : tr("%n item in the quarantine.", nullptr, rows));
    emit AsideChanged(rows == 0 ? tr("0 bytes held") : QString());

    restore_->setEnabled(selected > 0);
    discard_->setEnabled(selected > 0);
    empty_->setEnabled(rows > 0);
}
