#include "view/PresetsPage.h"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
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
#include "view/panels/EmptyState.h"
#include "view/presets/OmittedDialog.h"
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
    constexpr int kBetweenTheTwoColumnsOfCounts = 44;

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

    QLabel* FieldName(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("DetailFieldName"));
        label->setWordWrap(true);

        return label;
    }

    QLabel* LoudFieldName(QWidget* parent)
    {
        QLabel* label = FieldName(parent);
        label->setObjectName(QStringLiteral("PresetPlanFor"));

        return label;
    }

    QLabel* FieldValue(const QString& name, QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(name);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        return label;
    }

    void Say(QLabel* label, const std::size_t count)
    {
        label->setText(QString::number(count));
    }

    void ShowBoth(QLabel* name, QLabel* value, const bool showing)
    {
        name->setVisible(showing);
        value->setVisible(showing);
    }

    QString TheWayBackIsCalled()
    {
        return QObject::tr("Back to the previous set");
    }

    QString TheStartupSentenceOf(const PresetPreview& preview)
    {
        if (preview.notApplied > 0)
        {
            return QObject::tr("This preset asks for %n startup entry, and none will be applied, because startup "
                               "management is off in Options.",
                               nullptr, static_cast<int>(preview.notApplied));
        }

        if (preview.startupUnresolved > 0)
        {
            return QObject::tr("This preset asks for %1 startup entries. %2 of them are no longer in the simulator "
                               "file, and %3 will be switched.")
                .arg(preview.startupAsked)
                .arg(preview.startupUnresolved)
                .arg(preview.startupToApply);
        }

        return QObject::tr("This preset asks for %n startup entry, and all of them will be applied.", nullptr,
                           static_cast<int>(preview.startupAsked));
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
    connect(apply_, &QPushButton::clicked, this, &PresetsPage::ApplySelected);
    connect(goBack_, &QPushButton::clicked, this, &PresetsPage::GoBack);
    connect(showOmitted_, &QPushButton::clicked, this, &PresetsPage::ListTheOmitted);
    connect(governsStartup_, &QCheckBox::clicked, this, &PresetsPage::GovernStartupToggled);
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
    connect(modes_, &QButtonGroup::idClicked, this,
            [this]
            {
                ReloadNames();
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
    showOmitted_->setText(tr("Show them…"));
    governsStartup_->setText(tr("This preset also governs startup entries"));
    filter_->setPlaceholderText(tr("Filter presets"));
    entries_->setHorizontalHeaderLabels({tr("Addon"), tr("Library"), tr("Enables")});
    names_->setHorizontalHeaderLabels({tr("Preset"), tr("Content"), tr("Changed"), tr("Would change")});
    toEnableName_->setText(tr("Turn on"));
    toDisableName_->setText(tr("Turn off"));
    alreadyName_->setText(tr("Already as the preset asks"));
    unresolvedName_->setText(tr("Named, but no addon found"));
    notNamedName_->setText(tr("Off because Replace omits them"));
    notAppliedName_->setText(tr("Asked for, but not applied"));
    applyAs_->setText(tr("Apply as"));
    plan_->setText(tr("Plan"));
    modes_->button(static_cast<int>(ApplyMode::Replace))->setText(tr("Replace"));
    modes_->button(static_cast<int>(ApplyMode::Cumulative))->setText(tr("Accumulate"));
    modes_->button(static_cast<int>(ApplyMode::Disable))->setText(tr("Disable"));
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

QWidget* PresetsPage::CreateThePlanFields()
{
    auto* fields = new QWidget(this);

    auto* grid = new QGridLayout(fields);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(2, kBetweenTheTwoColumnsOfCounts);
    grid->setColumnStretch(3, 1);

    toEnableName_ = LoudFieldName(fields);
    toEnable_ = FieldValue(QStringLiteral("PlanToEnable"), fields);
    toDisableName_ = LoudFieldName(fields);
    toDisable_ = FieldValue(QStringLiteral("PlanToDisable"), fields);
    alreadyName_ = FieldName(fields);
    already_ = FieldValue(QStringLiteral("PlanAlreadyInPlace"), fields);
    unresolvedName_ = FieldName(fields);
    unresolved_ = FieldValue(QStringLiteral("PlanUnresolved"), fields);
    notNamedName_ = FieldName(fields);
    notNamed_ = FieldValue(QStringLiteral("PlanNotNamed"), fields);
    notAppliedName_ = FieldName(fields);
    notApplied_ = FieldValue(QStringLiteral("PlanNotApplied"), fields);

    QLabel* const names[] = {toEnableName_,   toDisableName_, alreadyName_,
                             unresolvedName_, notNamedName_,  notAppliedName_};
    QLabel* const values[] = {toEnable_, toDisable_, already_, unresolved_, notNamed_, notApplied_};

    for (int field = 0; field < 6; ++field)
    {
        const int column = field < 3 ? 0 : 3;

        grid->addWidget(names[field], field % 3, column);
        grid->addWidget(values[field], field % 3, column + 1);
    }

    return fields;
}

QWidget* PresetsPage::CreateTheModeRow()
{
    auto* row = new QWidget(this);

    applyAs_ = new QLabel(row);
    applyAs_->setObjectName(QStringLiteral("DetailFieldName"));

    modes_ = new QButtonGroup(row);
    auto* replace = new QRadioButton(row);
    replace->setObjectName(QStringLiteral("ModeReplace"));
    auto* cumulative = new QRadioButton(row);
    cumulative->setObjectName(QStringLiteral("ModeCumulative"));
    auto* disable = new QRadioButton(row);
    disable->setObjectName(QStringLiteral("ModeDisable"));
    modes_->addButton(replace, static_cast<int>(ApplyMode::Replace));
    modes_->addButton(cumulative, static_cast<int>(ApplyMode::Cumulative));
    modes_->addButton(disable, static_cast<int>(ApplyMode::Disable));
    replace->setChecked(true);

    modeExplained_ = new QLabel(row);
    modeExplained_->setObjectName(QStringLiteral("ModeExplained"));

    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(14);
    line->addWidget(applyAs_);
    line->addWidget(replace);
    line->addWidget(cumulative);
    line->addWidget(disable);
    line->addSpacing(6);
    line->addWidget(modeExplained_);
    line->addStretch();

    return row;
}

QWidget* PresetsPage::CreateThePlanHalf()
{
    auto* half = new QWidget(this);

    planFor_ = new QLabel(half);
    planFor_->setObjectName(QStringLiteral("PresetPlanFor"));

    planTitle_ = new QLabel(half);
    planTitle_->setObjectName(QStringLiteral("PanelSubHeading"));

    omittedNote_ = new QLabel(half);
    omittedNote_->setObjectName(QStringLiteral("PanelPromise"));
    omittedNote_->setWordWrap(true);

    showOmitted_ = new QPushButton(half);
    showOmitted_->setObjectName(QStringLiteral("PresetShowOmitted"));

    auto* omittedRow = new QHBoxLayout;
    omittedRow->setContentsMargins(0, 0, 0, 0);
    omittedRow->setSpacing(12);
    omittedRow->addWidget(omittedNote_, 1);
    omittedRow->addWidget(showOmitted_);

    apply_ = new QPushButton(half);
    apply_->setObjectName(QStringLiteral("PresetApply"));
    apply_->setProperty("role", "primary");
    apply_->setDefault(true);

    auto* applyRow = new QHBoxLayout;
    applyRow->setContentsMargins(0, 0, 0, 0);
    applyRow->addWidget(apply_);
    applyRow->addStretch();

    auto* column = new QVBoxLayout(half);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    column->setSpacing(12);
    column->addWidget(planFor_);
    column->addWidget(CreateTheModeRow());
    column->addWidget(planTitle_);
    column->addWidget(CreateThePlanFields());
    column->addLayout(omittedRow);
    column->addWidget(CreateTheStartupSection());
    column->addSpacing(6);
    column->addLayout(applyRow);
    column->addStretch();

    return half;
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
    shown->addWidget(CreateThePlanHalf());

    auto* column = new QVBoxLayout(right);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(strip);
    column->addWidget(shown, 1);

    connect(halves, &QButtonGroup::idClicked, shown, &QStackedWidget::setCurrentIndex);

    return right;
}

QWidget* PresetsPage::CreateTheStartupSection()
{
    auto* block = new QWidget(this);

    governsStartup_ = new QCheckBox(block);
    governsStartup_->setObjectName(QStringLiteral("PresetGovernsStartup"));

    startupSection_ = new QWidget(block);
    startupSection_->setObjectName(QStringLiteral("PresetStartupSection"));

    startupSaid_ = new QLabel(startupSection_);
    startupSaid_->setObjectName(QStringLiteral("PanelPromise"));
    startupSaid_->setWordWrap(true);

    auto* inside = new QVBoxLayout(startupSection_);
    inside->setContentsMargins(0, 0, 0, 0);
    inside->addWidget(startupSaid_);

    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(5);
    column->addWidget(governsStartup_);
    column->addWidget(startupSection_);

    startupSection_->hide();

    return block;
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
    return static_cast<ApplyMode>(modes_->checkedId());
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
    apply_->setEnabled(holdsOne);
    governsStartup_->setEnabled(holdsOne);
    planFor_->setText(name);

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
    governsStartup_->setEnabled(false);
    apply_->setEnabled(selected_.has_value());
    planFor_->setText(TheWayBackIsCalled());

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
    switch (Mode())
    {
    case ApplyMode::Replace:
        modeExplained_->setText(tr("Leaves only what the preset enables."));
        planTitle_->setText(tr("The plan, as Replace"));
        break;
    case ApplyMode::Cumulative:
        modeExplained_->setText(tr("Enables what the preset names, without touching the rest."));
        planTitle_->setText(tr("The plan, as Accumulate"));
        break;
    case ApplyMode::Disable:
        modeExplained_->setText(tr("Disables what the preset enables."));
        planTitle_->setText(tr("The plan, as Disable"));
        break;
    }

    governsStartup_->setChecked(selected_.has_value() && selected_->governsStartup);
    startupSection_->setVisible(governsStartup_->isChecked());

    const PresetPreview preview = selected_.has_value() ? viewModel_.Preview(*selected_, Mode()) : PresetPreview{};

    if (!selected_.has_value())
    {
        apply_->setText(tr("Apply"));
        apply_->setToolTip({});
    }
    else
    {
        apply_->setToolTip(CountsOf(preview));
        apply_->setText(tr("Apply: enables %1, disables %2").arg(preview.toEnable).arg(preview.toDisable));
    }

    Say(toEnable_, preview.toEnable);
    Say(toDisable_, preview.toDisable);
    Say(already_, preview.alreadyInPlace);
    Say(unresolved_, preview.unresolved);
    Say(notNamed_, preview.notNamedByThePreset);
    Say(notApplied_, preview.notApplied);

    const bool replacing = Mode() == ApplyMode::Replace;

    ShowBoth(notNamedName_, notNamed_, replacing);
    showOmitted_->setVisible(replacing);
    showOmitted_->setEnabled(preview.notNamedByThePreset > 0);
    omittedNote_->setVisible(replacing && preview.notNamedByThePreset > 0);
    omittedNote_->setText(tr("The %1 omitted are part of the %2 being turned off, and not a pile on top of them.")
                              .arg(preview.notNamedByThePreset)
                              .arg(preview.toDisable));

    startupSaid_->setText(TheStartupSentenceOf(preview));
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
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Replace what is enabled"), tr("%1\n\nApply \"%2\"?").arg(CountsOf(preview), planFor_->text()));

        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    viewModel_.Apply(*selected_, mode);
}
