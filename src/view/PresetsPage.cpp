#include "view/PresetsPage.h"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
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
#include "view/panels/EmptyState.h"
#include "view/presets/OmittedDialog.h"
#include "view/presets/PresetPlanPanel.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kAddonColumn = 0;
    constexpr int kLibraryColumn = 1;
    constexpr int kActionColumn = 2;
    constexpr int kNameColumn = 0;
    constexpr int kContentColumn = 1;
    constexpr int kUpdatedColumn = 2;
    constexpr int kChangesColumn = 3;
    constexpr int kNameTableWidth = 440;

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

    QString TheWayBackIsCalled()
    {
        return QObject::tr("Back to the previous set");
    }
}

PresetsPage::PresetsPage(PresetViewModel& viewModel, const SessionNotifier& notifier, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel)
{
    names_ = CreateNameTable();

    return_ = CreateReturnTable();

    returnRule_ = new QFrame(this);
    returnRule_->setObjectName(QStringLiteral("TriageSeparator"));
    returnRule_->setFixedHeight(1);

    auto* leftColumn = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftLayout->addWidget(names_, 1);
    leftLayout->addWidget(returnRule_);
    leftLayout->addWidget(return_);
    leftColumn->setFixedWidth(kNameTableWidth);

    create_ = new QPushButton(this);
    create_->setProperty("role", "primary");
    update_ = new QPushButton(this);
    rename_ = new QPushButton(this);
    remove_ = new QPushButton(this);
    goBack_ = new QPushButton(this);
    goBack_->setObjectName(QStringLiteral("PresetGoBack"));

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
    bar->addWidget(goBack_);
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

    planPanel_ = new PresetPlanPanel(this);

    auto* tables = new QHBoxLayout;
    tables->setContentsMargins(0, 0, 0, 0);
    tables->setSpacing(0);
    tables->addWidget(leftColumn);
    tables->addWidget(CreateTheTwoHalves(), 1);

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
    connect(goBack_, &QPushButton::clicked, this, &PresetsPage::GoBack);
    connect(planPanel_, &PresetPlanPanel::ApplyRequested, this, &PresetsPage::ApplySelected);
    connect(planPanel_, &PresetPlanPanel::OmittedRequested, this, &PresetsPage::ListTheOmitted);
    connect(planPanel_, &PresetPlanPanel::GovernStartupToggled, this, &PresetsPage::GovernStartupToggled);
    connect(planPanel_, &PresetPlanPanel::ModeChanged, this, &PresetsPage::ReloadNames);
    connect(names_, &QTableWidget::currentCellChanged, this,
            [this](const int row, int, const int previous, int)
            {
                if (row != previous)
                {
                    ShowSelected();
                }
            });
    connect(return_, &QTableWidget::currentCellChanged, this,
            [this](const int row, int, const int previous, int)
            {
                if (row >= 0 && row != previous)
                {
                    ShowTheReturnPreset();
                }
            });
    connect(filter_, &QLineEdit::textChanged, this, &PresetsPage::ShowOnlyTheNamesThatMatch);

    connect(entries_, &QTableWidget::itemChanged, this, &PresetsPage::ActionToggled);

    connect(&viewModel_, &PresetViewModel::Changed, this, &PresetsPage::ReloadNames);
    connect(&notifier, &SessionNotifier::Refreshed, this, &PresetsPage::ReloadNames);
    connect(&notifier, &SessionNotifier::ScanFinished, this, &PresetsPage::ReloadNames);
    connect(&viewModel_, &PresetViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::information(this, tr("Nothing changed"), explanation);
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
            [this](const QStringList& unresolved, const QString& startupLeftUndone)
            {
                QStringList said;

                if (!unresolved.isEmpty())
                {
                    said.append(tr("These addons of the preset are no longer in the library:\n\n%1")
                                    .arg(unresolved.join(QStringLiteral("\n"))));
                }

                if (!startupLeftUndone.isEmpty())
                {
                    said.append(startupLeftUndone);
                }

                if (!said.isEmpty())
                {
                    QMessageBox::warning(this, tr("Not everything the preset asked for happened"),
                                         said.join(QStringLiteral("\n\n")));
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
    names_->setHorizontalHeaderLabels({tr("Preset"), tr("Content"), tr("Changed"), tr("Would change")});
    plan_->setText(tr("Plan"));
    nothing_->Retell(tr("No preset in this profile yet."),
                     tr("A preset keeps which addons stay enabled. Enable what you want to fly and keep that "
                        "combination under a name. Applying it later is a single batch, with a whole undo."));
    nothingAction_->setText(tr("New from the enabled ones…"));
}

QTableWidget* PresetsPage::CreateNameTable()
{
    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("PresetNames"));
    table->setColumnCount(4);
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

QTableWidget* PresetsPage::CreateReturnTable()
{
    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("PresetReturn"));
    table->setColumnCount(4);
    table->setRowCount(1);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setItemDelegate(new RowDelegate(table));
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setVisible(false);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setFrameShape(QFrame::NoFrame);
    table->hide();
    LetTheColumnsBeDraggedAndStillFillTheTable(table, kNameColumn);

    return table;
}

QWidget* PresetsPage::CreateTheTwoHalves()
{
    auto* right = new QWidget(this);

    content_ = new QPushButton(right);
    content_->setObjectName(QStringLiteral("PresetContentTab"));
    content_->setCheckable(true);
    content_->setChecked(true);
    plan_ = new QPushButton(right);
    plan_->setObjectName(QStringLiteral("PresetPlanTab"));
    plan_->setCheckable(true);

    auto* halves = new QButtonGroup(right);
    halves->addButton(content_, 0);
    halves->addButton(plan_, 1);

    auto* strip = new QWidget(right);
    strip->setObjectName(QStringLiteral("PresetHalves"));

    auto* stripRow = new QHBoxLayout(strip);
    stripRow->setContentsMargins(0, 0, 0, 0);
    stripRow->setSpacing(0);
    stripRow->addWidget(content_);
    stripRow->addWidget(plan_);
    stripRow->addStretch();

    auto* shown = new QStackedWidget(right);
    shown->addWidget(entries_);
    shown->addWidget(planPanel_);

    auto* column = new QVBoxLayout(right);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(strip);
    column->addWidget(shown, 1);

    connect(halves, &QButtonGroup::idClicked, shown, &QStackedWidget::setCurrentIndex);

    return right;
}

int PresetsPage::HideTheNamesThatDoNotMatch() const
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

    return firstStanding;
}

void PresetsPage::ShowOnlyTheNamesThatMatch() const
{
    const int firstStanding = HideTheNamesThatDoNotMatch();

    if (showingReturn_)
    {
        return;
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
    return planPanel_->Mode();
}

namespace
{
    QTableWidgetItem* ChangeCell(const PresetRow& row)
    {
        auto* item = new QTableWidgetItem(QObject::tr("%n change", nullptr, static_cast<int>(row.changes)));
        item->setData(QuietRole, row.changes == 0);

        if (row.satisfied)
        {
            item->setData(TagTextRole, QObject::tr("Satisfied"));
            item->setData(TagToneRole, static_cast<int>(TagTone::Muted));
        }

        return item;
    }
}

void PresetsPage::ReloadNames()
{
    const QString wanted = SelectedName();
    const QList<PresetRow> rows = viewModel_.Rows(Mode());

    populating_ = true;
    names_->clearContents();
    names_->setRowCount(static_cast<int>(rows.size()));

    int landOn = rows.isEmpty() ? -1 : 0;

    for (int row = 0; row < rows.size(); ++row)
    {
        names_->setItem(row, kNameColumn, new QTableWidgetItem(rows[row].name));
        names_->setItem(row, kContentColumn, new QTableWidgetItem(rows[row].content));
        names_->setItem(row, kUpdatedColumn, new QTableWidgetItem(rows[row].updated));
        names_->setItem(row, kChangesColumn, ChangeCell(rows[row]));

        names_->item(row, kContentColumn)->setData(QuietRole, true);
        names_->item(row, kUpdatedColumn)->setData(QuietRole, true);

        if (rows[row].name == wanted)
        {
            landOn = row;
        }
    }

    if (!showingReturn_)
    {
        names_->setCurrentCell(landOn, kNameColumn);
    }

    populating_ = false;

    ShowOnlyTheNamesThatMatch();

    pages_->setCurrentIndex(rows.isEmpty() ? 1 : 0);

    emit SummaryChanged(rows.isEmpty() ? tr("No preset in this profile yet.")
                                       : tr("%n preset in this profile.", nullptr, static_cast<int>(rows.size())));

    ShowTheWayBack();

    if (showingReturn_)
    {
        ShowTheReturnPreset();
        return;
    }

    ShowSelected();
}

void PresetsPage::ShowTheWayBack()
{
    const std::optional<PresetRow> back = viewModel_.ReturnRow(Mode());
    const bool undoable = viewModel_.CanUndo();

    populating_ = true;
    return_->setVisible(back.has_value());
    returnRule_->setVisible(back.has_value());

    if (back.has_value())
    {
        return_->setItem(0, kNameColumn, new QTableWidgetItem(TheWayBackIsCalled()));
        return_->setItem(0, kContentColumn, new QTableWidgetItem(back->content));
        return_->setItem(0, kUpdatedColumn, new QTableWidgetItem);
        return_->setItem(0, kChangesColumn, ChangeCell(*back));
        return_->item(0, kContentColumn)->setData(QuietRole, true);
        return_->resizeRowsToContents();
        return_->setFixedHeight(return_->rowHeight(0) + 2);
    }

    populating_ = false;

    goBack_->setEnabled(undoable || back.has_value());
    goBack_->setText(TheWayBackIsCalled());
    goBack_->setToolTip(undoable ? tr("Undoes the batch you just applied.")
                                 : tr("Applies the return preset, written down before the last application."));
}

void PresetsPage::LetGoOf(QTableWidget* table)
{
    const bool was = populating_;
    populating_ = true;
    table->setCurrentItem(nullptr);
    populating_ = was;
}

void PresetsPage::ShowSelected()
{
    if (populating_)
    {
        return;
    }

    showingReturn_ = false;
    LetGoOf(return_);

    const QString name = SelectedName();
    selected_ = name.isEmpty() ? std::nullopt : viewModel_.Load(name);

    const bool holdsOne = selected_.has_value();
    update_->setEnabled(holdsOne);
    rename_->setEnabled(holdsOne);
    remove_->setEnabled(holdsOne);

    ShowEntries();

    RefreshPreview();
}

void PresetsPage::ShowTheReturnPreset()
{
    if (populating_)
    {
        return;
    }

    showingReturn_ = true;
    LetGoOf(names_);
    selected_ = viewModel_.ReturnPreset();

    update_->setEnabled(false);
    rename_->setEnabled(false);
    remove_->setEnabled(false);

    ShowEntries();

    RefreshPreview();
}

void PresetsPage::ShowEntries()
{
    const int rows = selected_.has_value() ? static_cast<int>(selected_->entries.size()) : 0;

    content_->setText(tr("Content · %1").arg(rows));

    populating_ = true;
    entries_->setRowCount(rows);

    for (int row = 0; row < rows; ++row)
    {
        const PresetEntry& entry = selected_->entries[static_cast<std::size_t>(row)];
        entries_->setItem(row, kAddonColumn, new QTableWidgetItem(QString::fromStdString(entry.addonId.folderName)));
        entries_->setItem(row, kLibraryColumn, new QTableWidgetItem(viewModel_.LibraryLabel(entry.addonId.libraryId)));
        entries_->item(row, kLibraryColumn)->setData(QuietRole, true);

        auto* action = new QTableWidgetItem;
        action->setFlags(showingReturn_ ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                        : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
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
    const QString planFor = showingReturn_ ? TheWayBackIsCalled() : SelectedName();
    const bool holdsOne = selected_.has_value();
    const bool canGovern = holdsOne && !showingReturn_;
    const bool governsStartup = holdsOne && selected_->governsStartup;
    const PresetPreview preview = holdsOne ? viewModel_.Preview(*selected_, planPanel_->Mode()) : PresetPreview{};

    planPanel_->Show({.planFor = planFor,
                      .preview = preview,
                      .holdsOne = holdsOne,
                      .governsStartup = governsStartup,
                      .canGovern = canGovern});
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

void PresetsPage::GovernStartupToggled(const bool governs)
{
    const QString name = SelectedName();

    if (name.isEmpty() || !viewModel_.GovernStartup(name, governs))
    {
        RefreshPreview();
        return;
    }

    selected_ = viewModel_.Load(name);

    RefreshPreview();
}

void PresetsPage::ListTheOmitted()
{
    if (!selected_.has_value())
    {
        return;
    }

    OmittedDialog dialog(viewModel_.Omitted(*selected_, Mode()), this);
    dialog.exec();
}

void PresetsPage::GoBack()
{
    if (viewModel_.CanUndo())
    {
        viewModel_.UndoLastBatch();
        return;
    }

    if (const std::optional<Preset> back = viewModel_.ReturnPreset(); back.has_value())
    {
        viewModel_.Apply(*back, ApplyMode::Replace);
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
        const QString label = showingReturn_ ? TheWayBackIsCalled() : SelectedName();
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Replace what is enabled"), tr("%1\n\nApply \"%2\"?").arg(CountsSentenceFor(preview), label));

        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    viewModel_.Apply(*selected_, mode);
}
