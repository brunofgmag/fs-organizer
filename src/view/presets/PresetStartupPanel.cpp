#include "view/presets/PresetStartupPanel.h"

#include <QtCore/QEvent>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include "view/TableColumns.h"
#include "view/delegates/CenteredCheckDelegate.h"
#include "view/delegates/RowDelegate.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

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
    connect(entries_, &QTableWidget::itemChanged, this, &PresetStartupPanel::RowChanged);

    RetranslateUi();
}

QWidget* PresetStartupPanel::CreateTheLiveHalf()
{
    entries_ = new QTableWidget(this);
    entries_->setObjectName(QStringLiteral("PresetStartupEntries"));
    entries_->setColumnCount(3);
    entries_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entries_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entries_->setItemDelegate(new RowDelegate(entries_));
    entries_->setItemDelegateForColumn(kActionColumn, new CenteredCheckDelegate(entries_));
    entries_->setShowGrid(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(entries_, kTargetColumn);
    entries_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(entries_->horizontalHeader());

    update_ = new QPushButton(this);
    update_->setObjectName(QStringLiteral("PresetUpdateStartup"));

    auto* live = new QWidget(this);

    auto* liveColumn = new QVBoxLayout(live);
    liveColumn->setContentsMargins(0, 0, 0, 0);
    liveColumn->setSpacing(8);
    liveColumn->addWidget(entries_, 1);

    auto* updateRow = new QHBoxLayout;
    updateRow->setContentsMargins(0, 0, 0, 0);
    updateRow->addWidget(update_);
    updateRow->addStretch();
    liveColumn->addLayout(updateRow);

    return live;
}

void PresetStartupPanel::Show(const PresetStartupState& state)
{
    populating_ = true;

    governs_->setEnabled(state.holdsOne && !state.readOnly);
    governs_->setChecked(state.governs);
    update_->setEnabled(!state.readOnly);
    body_->setCurrentIndex(state.governs ? kEntriesAreAbove : kPromiseIsAbove);

    entries_->setRowCount(static_cast<int>(state.rows.size()));

    for (int row = 0; row < state.rows.size(); ++row)
    {
        const PresetStartupRow& entry = state.rows[row];
        entries_->setItem(row, kEntryColumn, new QTableWidgetItem(entry.label));
        entries_->setItem(row, kTargetColumn, new QTableWidgetItem(entry.target));
        entries_->item(row, kTargetColumn)->setData(QuietRole, true);

        auto* action = new QTableWidgetItem;
        action->setFlags(state.readOnly ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                        : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        action->setCheckState(entry.action == PresetAction::Disable ? Qt::Unchecked : Qt::Checked);

        entries_->setItem(row, kActionColumn, action);
    }

    populating_ = false;
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
    governs_->setText(tr("This preset also governs startup entries"));
    update_->setText(tr("Update with the enabled ones"));
    empty_->Retell(tr("This preset does not govern startup entries"),
                   tr("Check the box above and it captures the ones enabled right now. You can then turn "
                      "each on or off here."));
}
