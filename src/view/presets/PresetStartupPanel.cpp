#include "view/presets/PresetStartupPanel.h"

#include <QtCore/QEvent>
#include <QtGui/QAction>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MenuText.h"
#include "view/TableColumns.h"
#include "view/delegates/CenteredCheckDelegate.h"
#include "view/delegates/RowDelegate.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ScrollBarCap.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kEntryColumn = 0;
    constexpr int kTargetColumn = 1;
    constexpr int kActionColumn = 2;
    constexpr int kPromiseIsAbove = 0;
    constexpr int kEntriesAreAbove = 1;
} // namespace

PresetStartupPanel::PresetStartupPanel(QWidget* parent) : QWidget(parent)
{
    governs_ = new QCheckBox(this);
    governs_->setObjectName(QStringLiteral("PresetGovernsStartup"));

    empty_ = new EmptyState(this);

    body_ = new QStackedWidget(this);
    body_->addWidget(empty_);
    body_->addWidget(CreateTheLiveHalf());

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    column->setSpacing(12);
    column->addWidget(governs_);
    column->addWidget(body_, 1);

    connect(governs_, &QCheckBox::clicked, this, &PresetStartupPanel::GovernToggled);
    connect(update_, &QPushButton::clicked, this, &PresetStartupPanel::RecaptureRequested);
    connect(add_, &QPushButton::clicked, this, &PresetStartupPanel::OfferTheCandidates);
    connect(takeOut_, &QPushButton::clicked, this, &PresetStartupPanel::TakeTheSelectedRowOut);
    connect(takeOutAction_, &QAction::triggered, this, &PresetStartupPanel::TakeTheSelectedRowOut);
    connect(candidatesMenu_, &QMenu::triggered, this, &PresetStartupPanel::ChooseACandidate);
    connect(entries_, &QTableWidget::customContextMenuRequested, this, &PresetStartupPanel::OfferToTakeTheRowOut);
    connect(entries_, &QTableWidget::itemSelectionChanged, this, &PresetStartupPanel::ShowWhatTheGesturesCanDo);
    connect(entries_, &QTableWidget::itemChanged, this, &PresetStartupPanel::RowChanged);

    RetranslateUi();
}

QWidget* PresetStartupPanel::CreateTheLiveHalf()
{
    entries_ = new QTableWidget(this);
    entries_->setObjectName(QStringLiteral("PresetStartupEntries"));
    entries_->setColumnCount(3);
    entries_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entries_->setSelectionMode(QAbstractItemView::SingleSelection);
    entries_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entries_->setContextMenuPolicy(Qt::CustomContextMenu);
    entries_->setItemDelegate(new RowDelegate(entries_));
    entries_->setItemDelegateForColumn(kActionColumn, new CenteredCheckDelegate(entries_));
    entries_->setShowGrid(false);
    entries_->setTextElideMode(Qt::ElideMiddle);
    LetTheColumnsBeDraggedAndStillFillTheTable(entries_, kTargetColumn);
    entries_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(entries_->horizontalHeader());
    CapTheScrollBarOf(entries_, entries_->horizontalHeader());

    candidatesMenu_ = new QMenu(this);
    candidatesMenu_->setObjectName(QStringLiteral("PresetAddStartupMenu"));

    rowMenu_ = new QMenu(this);
    rowMenu_->setObjectName(QStringLiteral("PresetStartupRowMenu"));
    takeOutAction_ = rowMenu_->addAction(QString());

    auto* live = new QWidget(this);

    auto* liveColumn = new QVBoxLayout(live);
    liveColumn->setContentsMargins(0, 0, 0, 0);
    liveColumn->setSpacing(8);
    liveColumn->addWidget(entries_, 1);
    liveColumn->addLayout(CreateTheGestures());

    return live;
}

QHBoxLayout* PresetStartupPanel::CreateTheGestures()
{
    add_ = new QPushButton(this);
    add_->setObjectName(QStringLiteral("PresetAddStartupEntry"));

    takeOut_ = new QPushButton(this);
    takeOut_->setObjectName(QStringLiteral("PresetTakeStartupEntryOut"));

    update_ = new QPushButton(this);
    update_->setObjectName(QStringLiteral("PresetUpdateStartup"));

    auto* gestures = new QHBoxLayout;
    gestures->setContentsMargins(0, 0, 0, 0);
    gestures->setSpacing(kToolbarGap);
    gestures->addWidget(add_);
    gestures->addWidget(takeOut_);
    gestures->addSpacing(kSpringAtLeast - kToolbarGap);
    gestures->addStretch();
    gestures->addWidget(update_);

    return gestures;
}

QTableWidgetItem* PresetStartupPanel::TargetCellOf(const PresetStartupRow& row) const
{
    auto* target = new QTableWidgetItem(row.target);
    target->setData(QuietRole, true);

    if (row.hasNoEntry)
    {
        target->setData(TagTextRole, tr("not in the file"));
        target->setData(TagToneRole, static_cast<int>(TagTone::Outlined));
        target->setData(TagAtTheEndRole, true);
        target->setToolTip(tr("This path has no entry in the startup file, so applying the preset leaves it out.")
                           + QLatin1Char('\n') + row.target);
    }

    return target;
}

void PresetStartupPanel::Show(const PresetStartupState& state)
{
    populating_ = true;

    readOnly_ = state.readOnly;
    fileHoldsEntries_ = state.fileHoldsEntries;
    candidates_ = state.candidates;

    governs_->setEnabled(state.holdsOne && !state.readOnly);
    governs_->setChecked(state.governs);
    update_->setEnabled(!state.readOnly);
    body_->setCurrentIndex(state.governs ? kEntriesAreAbove : kPromiseIsAbove);

    FillTheRows(state.rows);
    FillTheCandidateMenu();

    populating_ = false;

    ShowWhatTheGesturesCanDo();
}

void PresetStartupPanel::FillTheRows(const QList<PresetStartupRow>& rows)
{
    entries_->setRowCount(static_cast<int>(rows.size()));

    for (int row = 0; row < rows.size(); ++row)
    {
        const PresetStartupRow& entry = rows[row];
        entries_->setItem(row, kEntryColumn, new QTableWidgetItem(entry.label));
        entries_->setItem(row, kTargetColumn, TargetCellOf(entry));

        auto* action = new QTableWidgetItem;
        action->setFlags(readOnly_ ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                   : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        action->setCheckState(entry.action == PresetAction::Disable ? Qt::Unchecked : Qt::Checked);

        entries_->setItem(row, kActionColumn, action);
    }
}

void PresetStartupPanel::FillTheCandidateMenu()
{
    candidatesMenu_->clear();

    for (int index = 0; index < candidates_.size(); ++index)
    {
        candidatesMenu_->addAction(AsMenuText(candidates_[index].label))->setData(index);
    }
}

void PresetStartupPanel::ShowWhatTheGesturesCanDo()
{
    if (populating_)
    {
        return;
    }

    const bool editable = !readOnly_;
    const bool everyEntryIsIn = editable && fileHoldsEntries_ && candidates_.isEmpty();

    add_->setEnabled(editable && !candidates_.isEmpty());
    add_->setToolTip(everyEntryIsIn ? tr("Every entry in the startup file is already in this preset.") : QString());
    takeOut_->setEnabled(editable && SelectedRow() >= 0);
}

int PresetStartupPanel::SelectedRow() const
{
    const QModelIndexList rows = entries_->selectionModel()->selectedRows();

    return rows.isEmpty() ? -1 : rows.front().row();
}

void PresetStartupPanel::OfferTheCandidates()
{
    candidatesMenu_->popup(add_->mapToGlobal(QPoint(0, add_->height())));
}

void PresetStartupPanel::ChooseACandidate(const QAction* chosen)
{
    const int index = chosen->data().toInt();

    if (index >= 0 && index < candidates_.size())
    {
        const std::filesystem::path path = candidates_[index].path;

        emit EntryAddRequested(path);
    }
}

void PresetStartupPanel::OfferToTakeTheRowOut(const QPoint& where)
{
    const QTableWidgetItem* item = entries_->itemAt(where);

    if (readOnly_ || item == nullptr)
    {
        return;
    }

    entries_->selectRow(item->row());
    rowMenu_->popup(entries_->viewport()->mapToGlobal(where));
}

void PresetStartupPanel::TakeTheSelectedRowOut()
{
    const int row = SelectedRow();

    if (readOnly_ || row < 0)
    {
        return;
    }

    entries_->clearSelection();

    emit RowTakeOutRequested(row);
}

void PresetStartupPanel::RowChanged(const QTableWidgetItem* item)
{
    if (populating_ || item->column() != kActionColumn)
    {
        return;
    }

    emit ActionToggled(item->row(), item->checkState() == Qt::Checked ? PresetAction::Enable : PresetAction::Disable);
}

void PresetStartupPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void PresetStartupPanel::RetranslateUi()
{
    entries_->setHorizontalHeaderLabels({tr("Entry"), tr("Target"), tr("Enables")});
    governs_->setText(tr("This preset also controls startup entries"));
    add_->setText(tr("Add entry…"));
    takeOut_->setText(tr("Remove from preset"));
    takeOutAction_->setText(tr("Remove from preset"));
    update_->setText(tr("Update from enabled entries"));
    empty_->Retell(
        tr("This preset does not control startup entries"),
        tr("Check the box above to save the entries enabled right now. You can then enable or disable each one here."));
    ShowWhatTheGesturesCanDo();
}
