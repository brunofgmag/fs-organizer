#include "view/PresetsPage.h"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include <QtWidgets/QStackedWidget>

#include "view/delegates/RowDelegate.h"
#include "view/TableColumns.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

namespace
{
    constexpr int kAddonColumn = 0;
    constexpr int kActionColumn = 2;
    constexpr int kNameColumn = 0;
    constexpr int kContentColumn = 1;
    constexpr int kUpdatedColumn = 2;
    constexpr int kNameTableWidth = 380;

    class CenteredCheckDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem cell = option;
            initStyleOption(&cell, index);

            QStyle* style = cell.widget != nullptr ? cell.widget->style() : QApplication::style();

            QStyleOptionViewItem background = cell;
            background.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            style->drawControl(QStyle::CE_ItemViewItem, &background, painter, cell.widget);

            QStyleOptionViewItem check = cell;
            check.rect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &cell, cell.widget);
            check.rect.moveCenter(cell.rect.center());
            check.state &= ~QStyle::State_HasFocus;
            check.state |= cell.checkState == Qt::Checked ? QStyle::State_On : QStyle::State_Off;
            style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, cell.widget);
        }

    protected:
        bool editorEvent(QEvent* event,
                         QAbstractItemModel* model,
                         const QStyleOptionViewItem& option,
                         const QModelIndex& index) override
        {
            if (!index.flags().testFlag(Qt::ItemIsUserCheckable) || !index.flags().testFlag(Qt::ItemIsEnabled))
            {
                return false;
            }

            if (event->type() == QEvent::MouseButtonRelease)
            {
                const auto* mouse = static_cast<QMouseEvent*>(event);

                if (!option.rect.contains(mouse->position().toPoint()))
                {
                    return false;
                }
            }
            else if (event->type() == QEvent::KeyPress)
            {
                const auto* keys = static_cast<QKeyEvent*>(event);

                if (keys->key() != Qt::Key_Space && keys->key() != Qt::Key_Select)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            const Qt::CheckState flipped =
                index.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked;

            return model->setData(index, flipped, Qt::CheckStateRole);
        }
    };

    QString CountsOf(const PresetPreview& preview)
    {
        QString counted = QObject::tr("Enables %1, disables %2. %3 already match what the preset asks, %4 were not "
                                      "found, and %5 destination entries stay as they are.")
                              .arg(preview.toEnable)
                              .arg(preview.toDisable)
                              .arg(preview.alreadyInPlace)
                              .arg(preview.unresolved)
                              .arg(preview.leftAlone);

        if (preview.notNamedByThePreset > 0)
        {
            counted += QObject::tr(" Of the ones it disables, %1 entered the library after the preset was saved.")
                           .arg(preview.notNamedByThePreset);
        }

        return counted;
    }
}

PresetsPage::PresetsPage(PresetViewModel& viewModel, const SessionNotifier& notifier, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel)
{
    names_ = CreateNameTable();

    create_ = new QPushButton(this);
    create_->setProperty("role", "primary");
    update_ = new QPushButton(this);
    rename_ = new QPushButton(this);
    remove_ = new QPushButton(this);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    filter_ = new QLineEdit(this);
    filter_->setClearButtonEnabled(true);
    filter_->setMinimumWidth(220);
    filter_->setMaximumWidth(280);

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(create_);
    bar->addWidget(update_);
    bar->addWidget(rename_);
    bar->addWidget(remove_);
    bar->addStretch();
    bar->addWidget(filter_);

    entries_ = new QTableWidget(this);
    entries_->setObjectName(QStringLiteral("PresetEntries"));
    entries_->setColumnCount(3);
    entries_->setHorizontalHeaderLabels({tr("Addon"), tr("Library"), tr("Enables")});
    entries_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entries_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entries_->setItemDelegate(new RowDelegate(entries_));
    entries_->setItemDelegateForColumn(kActionColumn, new CenteredCheckDelegate(entries_));
    entries_->setShowGrid(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(entries_, kAddonColumn);
    entries_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(entries_->horizontalHeader());

    auto* panel = new ContextPanel({}, 440, this);
    panel->setObjectName(QStringLiteral("PresetApplyPanel"));
    panel_ = panel;

    applyAs_ = new QLabel(panel);
    applyAs_->setObjectName(QStringLiteral("PanelSubHeading"));

    modes_ = new QButtonGroup(panel);
    auto* replace = new QRadioButton(panel);
    replace->setObjectName(QStringLiteral("ModeReplace"));
    auto* cumulative = new QRadioButton(panel);
    cumulative->setObjectName(QStringLiteral("ModeCumulative"));
    auto* disable = new QRadioButton(panel);
    disable->setObjectName(QStringLiteral("ModeDisable"));
    modes_->addButton(replace, static_cast<int>(ApplyMode::Replace));
    modes_->addButton(cumulative, static_cast<int>(ApplyMode::Cumulative));
    modes_->addButton(disable, static_cast<int>(ApplyMode::Disable));
    replace->setChecked(true);

    modeExplained_ = new QLabel(panel);
    modeExplained_->setObjectName(QStringLiteral("ModeExplained"));
    modeExplained_->setWordWrap(true);

    preview_ = new QLabel(panel);
    preview_->setWordWrap(true);

    promise_ = new QLabel(panel);
    promise_->setObjectName(QStringLiteral("PanelPromise"));
    promise_->setWordWrap(true);

    apply_ = new QPushButton(panel);
    apply_->setObjectName(QStringLiteral("PresetApply"));
    apply_->setProperty("role", "primary");
    apply_->setDefault(true);

    panel->Add(applyAs_);
    panel->Add(replace);
    panel->Add(cumulative);
    panel->Add(disable);
    panel->Add(modeExplained_);
    panel->Add(preview_);
    panel->Add(apply_);
    panel->Add(promise_);

    panel->RestoreCollapsedState();

    auto* tables = new QHBoxLayout;
    tables->setContentsMargins(0, 0, 0, 0);
    tables->setSpacing(0);
    tables->addWidget(names_);
    tables->addWidget(entries_, 1);
    tables->addWidget(panel);

    auto* kept = new QWidget(this);
    auto* keptLayout = new QVBoxLayout(kept);
    keptLayout->setContentsMargins(0, 0, 0, 0);
    keptLayout->setSpacing(0);
    keptLayout->addWidget(toolbar);
    keptLayout->addLayout(tables, 1);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(kept);

    nothing_ = new EmptyState(this);
    nothingAction_ = nothing_->OfferTheOnlyAction();
    connect(nothingAction_, &QPushButton::clicked, this, &PresetsPage::CreateFromWhatIsEnabled);
    pages_->addWidget(nothing_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pages_);

    connect(create_, &QPushButton::clicked, this, &PresetsPage::CreateFromWhatIsEnabled);
    connect(update_, &QPushButton::clicked, this, &PresetsPage::UpdateFromWhatIsEnabled);
    connect(rename_, &QPushButton::clicked, this, &PresetsPage::RenameSelected);
    connect(remove_, &QPushButton::clicked, this, &PresetsPage::RemoveSelected);
    connect(apply_, &QPushButton::clicked, this, &PresetsPage::ApplySelected);
    connect(names_, &QTableWidget::currentCellChanged, this,
            [this](const int row, int, const int previous, int)
            {
                if (row != previous)
                {
                    ShowSelected();
                }
            });
    connect(modes_, &QButtonGroup::idClicked, this,
            [this]
            {
                RefreshPreview();
            });

    connect(filter_, &QLineEdit::textChanged, this, &PresetsPage::ShowOnlyTheNamesThatMatch);

    connect(entries_, &QTableWidget::itemChanged, this, &PresetsPage::ActionToggled);

    connect(&viewModel_, &PresetViewModel::Changed, this, &PresetsPage::ReloadNames);
    connect(&notifier, &SessionNotifier::Refreshed, this, &PresetsPage::RefreshPreview);
    connect(&notifier, &SessionNotifier::ScanFinished, this, &PresetsPage::ReloadNames);
    connect(&viewModel_, &PresetViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::information(this, tr("Nothing changed"), explanation);
            });
    connect(&notifier, &SessionNotifier::SimulatorIsRunning, this,
            [this]
            {
                QMessageBox::information(this, tr("The simulator is open"),
                                         tr("The changes only count after the simulator is restarted."));
            });
    connect(&notifier, &SessionNotifier::RestartPendingChanged, this,
            [this](const bool pending)
            {
                if (pending)
                {
                    emit StatusChanged(tr("Restart the simulator to apply the changes."));
                }
            });
    connect(&viewModel_, &PresetViewModel::Applied, this,
            [this](const QStringList& unresolved)
            {
                if (!unresolved.isEmpty())
                {
                    QMessageBox::warning(this, tr("Entries not found"),
                                         tr("These addons of the preset are no longer in the library:\n\n%1")
                                             .arg(unresolved.join(QStringLiteral("\n"))));
                }
            });

    RetranslateUi();
    ReloadNames();
}

void PresetsPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        ReloadNames();
    }

    QWidget::changeEvent(event);
}

void PresetsPage::RetranslateUi()
{
    create_->setText(tr("New from the enabled ones…"));
    update_->setText(tr("Update with the enabled ones"));
    rename_->setText(tr("Rename…"));
    remove_->setText(tr("Delete"));
    filter_->setPlaceholderText(tr("Filter presets"));
    entries_->setHorizontalHeaderLabels({tr("Addon"), tr("Library"), tr("Enables")});
    names_->setHorizontalHeaderLabels({tr("Preset"), tr("Content"), tr("Changed")});
    applyAs_->setText(tr("Apply as"));
    panel_->RenameTheFallback(tr("Apply as"));
    modes_->button(static_cast<int>(ApplyMode::Replace))->setText(tr("Replace"));
    modes_->button(static_cast<int>(ApplyMode::Cumulative))->setText(tr("Accumulate"));
    modes_->button(static_cast<int>(ApplyMode::Disable))->setText(tr("Disable"));
    promise_->setText(tr("Applying is a single batch: \"Undo the last batch\" takes it all back at once."));
    nothing_->Retell(tr("No preset in this profile yet."),
                     tr("A preset keeps which addons stay enabled. Enable what you want to fly and keep that "
                        "combination under a name. Applying it later is a single batch, with a whole undo."));
    nothingAction_->setText(tr("New from the enabled ones…"));
}

QTableWidget* PresetsPage::CreateNameTable()
{
    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("PresetNames"));
    table->setFixedWidth(kNameTableWidth);
    table->setColumnCount(3);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setItemDelegate(new RowDelegate(table));
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    DressTheHeaderOf(table->horizontalHeader());
    LetTheColumnsBeDraggedAndStillFillTheTable(table, kNameColumn);

    return table;
}

void PresetsPage::ShowOnlyTheNamesThatMatch() const
{
    const QString wanted = filter_->text().trimmed();
    int firstStanding = -1;

    for (int row = 0; row < names_->rowCount(); ++row)
    {
        const QTableWidgetItem* item = names_->item(row, kNameColumn);
        const bool matches = item != nullptr && item->text().contains(wanted, Qt::CaseInsensitive);

        names_->setRowHidden(row, !matches);

        if (matches && firstStanding < 0)
        {
            firstStanding = row;
        }
    }

    if (names_->currentRow() >= 0 && !names_->isRowHidden(names_->currentRow()))
    {
        return;
    }

    if (firstStanding < 0)
    {
        names_->setCurrentItem(nullptr);
        return;
    }

    names_->setCurrentCell(firstStanding, kNameColumn);
}

QString PresetsPage::SelectedName() const
{
    const QTableWidgetItem* item = names_->item(names_->currentRow(), kNameColumn);

    return item == nullptr ? QString{} : item->text();
}

ApplyMode PresetsPage::Mode() const
{
    return static_cast<ApplyMode>(modes_->checkedId());
}

void PresetsPage::ReloadNames()
{
    const QString wanted = SelectedName();
    const QList<PresetRow> rows = viewModel_.Rows();

    populating_ = true;
    names_->clearContents();
    names_->setRowCount(static_cast<int>(rows.size()));

    int landOn = rows.isEmpty() ? -1 : 0;

    for (int row = 0; row < rows.size(); ++row)
    {
        names_->setItem(row, kNameColumn, new QTableWidgetItem(rows[row].name));
        names_->setItem(row, kContentColumn, new QTableWidgetItem(rows[row].content));
        names_->setItem(row, kUpdatedColumn, new QTableWidgetItem(rows[row].updated));

        if (rows[row].name == wanted)
        {
            landOn = row;
        }
    }

    names_->setCurrentCell(landOn, kNameColumn);
    populating_ = false;

    ShowOnlyTheNamesThatMatch();

    pages_->setCurrentIndex(rows.isEmpty() ? 1 : 0);

    emit SummaryChanged(rows.isEmpty() ? tr("No preset in this profile yet.")
                                       : tr("%n preset in this profile.", nullptr, static_cast<int>(rows.size())));

    ShowSelected();
}

void PresetsPage::ShowSelected()
{
    if (populating_)
    {
        return;
    }

    const QString name = SelectedName();
    selected_ = name.isEmpty() ? std::nullopt : viewModel_.Load(name);

    const bool holdsOne = selected_.has_value();
    update_->setEnabled(holdsOne);
    rename_->setEnabled(holdsOne);
    remove_->setEnabled(holdsOne);
    apply_->setEnabled(holdsOne);
    panel_->Summon(holdsOne);
    panel_->ShowTitle(name);

    ShowEntries();

    RefreshPreview();
}

void PresetsPage::ShowEntries()
{
    const int rows = selected_.has_value() ? static_cast<int>(selected_->entries.size()) : 0;

    populating_ = true;
    entries_->setRowCount(rows);

    for (int row = 0; row < rows; ++row)
    {
        const PresetEntry& entry = selected_->entries[static_cast<std::size_t>(row)];
        entries_->setItem(row, kAddonColumn, new QTableWidgetItem(QString::fromStdString(entry.addonId.folderName)));
        entries_->setItem(row, 1, new QTableWidgetItem(viewModel_.LibraryLabel(entry.addonId.libraryId)));

        auto* action = new QTableWidgetItem;
        action->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        action->setCheckState(entry.action == PresetAction::Disable ? Qt::Unchecked : Qt::Checked);

        entries_->setItem(row, kActionColumn, action);
    }

    populating_ = false;
}

void PresetsPage::ActionToggled(const QTableWidgetItem* item)
{
    if (populating_ || item->column() != kActionColumn || !selected_.has_value())
    {
        return;
    }

    const auto row = static_cast<std::size_t>(item->row());

    if (row >= selected_->entries.size())
    {
        return;
    }

    const PresetAction wanted = item->checkState() == Qt::Checked ? PresetAction::Enable : PresetAction::Disable;

    if (!viewModel_.SetAction(SelectedName(), row, selected_->entries[row].addonId, wanted))
    {
        QMetaObject::invokeMethod(this, &PresetsPage::ReloadNames, Qt::QueuedConnection);
        return;
    }

    selected_->entries[row].action = wanted;

    RefreshPreview();
}

void PresetsPage::RefreshPreview() const
{
    switch (Mode())
    {
    case ApplyMode::Replace: modeExplained_->setText(tr("Leaves only what the preset enables.")); break;
    case ApplyMode::Cumulative:
        modeExplained_->setText(tr("Enables what the preset names, without touching the rest."));
        break;
    case ApplyMode::Disable: modeExplained_->setText(tr("Disables what the preset enables.")); break;
    }

    if (!selected_.has_value())
    {
        preview_->clear();
        apply_->setText(tr("Apply"));
        return;
    }

    const PresetPreview preview = viewModel_.Preview(*selected_, Mode());

    preview_->setText(CountsOf(preview));
    apply_->setText(tr("Apply: enables %1, disables %2").arg(preview.toEnable).arg(preview.toDisable));
}

void PresetsPage::CreateFromWhatIsEnabled()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("New preset"), tr("Name:"), QLineEdit::Normal, {}, &accepted);

    if (accepted)
    {
        viewModel_.Create(name);
    }
}

void PresetsPage::UpdateFromWhatIsEnabled() const
{
    const QString name = SelectedName();

    if (!name.isEmpty())
    {
        viewModel_.Update(name);
    }
}

void PresetsPage::RenameSelected()
{
    const QString name = SelectedName();

    if (name.isEmpty())
    {
        return;
    }

    bool accepted = false;
    const QString wanted =
        QInputDialog::getText(this, tr("Rename preset"), tr("Name:"), QLineEdit::Normal, name, &accepted);

    if (accepted)
    {
        viewModel_.Rename(name, wanted);
    }
}

void PresetsPage::RemoveSelected()
{
    const QString name = SelectedName();

    if (name.isEmpty())
    {
        return;
    }

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Delete preset"), tr("Delete the preset \"%1\"?").arg(name));

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Remove(name);
    }
}

void PresetsPage::ApplySelected()
{
    if (!selected_.has_value())
    {
        return;
    }

    const ApplyMode mode = Mode();

    if (mode == ApplyMode::Replace)
    {
        const PresetPreview preview = viewModel_.Preview(*selected_, mode);
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Replace what is enabled"),
                                  tr("%1\n\nApply the preset \"%2\"?").arg(CountsOf(preview), selected_->name.c_str()));

        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    viewModel_.Apply(*selected_, mode);
}
