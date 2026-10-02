#include "view/simulator/StartupPage.h"

#include <QtCore/QEvent>
#include <QtCore/QSignalBlocker>
#include <QtCore/QStringList>
#include <QtGui/QShortcut>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "support/PathText.h"
#include "view/PresetsPage.h"
#include "view/delegates/RowDelegate.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ScrollBarCap.h"
#include "view/simulator/StartupDraftDialog.h"
#include "view/simulator/UndoButton.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kSwitch = 0;
    constexpr int kPath = 1;
    constexpr int kState = 2;
    constexpr int kProgramWidth = 280;

    QString StateOf(const StartupLine& line)
    {
        switch (line.condition)
        {
        case StartupCondition::Broken: return QObject::tr("the program is missing");
        case StartupCondition::BehindADisabledAddon: return QObject::tr("its addon is disabled");
        case StartupCondition::Unavailable: return QObject::tr("its drive is not connected");
        case StartupCondition::Reachable: break;
        }

        return line.reach == StartupReach::InsideAnAddon ? QObject::tr("inside an addon")
                                                         : QObject::tr("outside your addons");
    }

    bool ItsLaunchWouldFailNow(const StartupLine& line)
    {
        return line.enabled && line.condition != StartupCondition::Reachable;
    }

    void Dress(QTreeWidgetItem* row, const StartupLine& line)
    {
        row->setText(kSwitch, QString::fromStdString(line.label));
        row->setCheckState(kSwitch, line.enabled ? Qt::Checked : Qt::Unchecked);
        row->setData(kSwitch, Qt::UserRole, AsText(line.path));
        row->setText(kPath, AsText(line.path));
        row->setToolTip(kPath, AsText(line.path));

        const bool tagged = line.condition == StartupCondition::Broken;
        const bool alarming = ItsLaunchWouldFailNow(line);

        row->setText(kState, tagged ? QString() : StateOf(line));
        row->setData(kState, TagTextRole, tagged ? StateOf(line) : QString());
        row->setData(kState, TagToneRole, static_cast<int>(TagTone::Outlined));

        row->setData(kPath, QuietRole, true);
        row->setData(kState, QuietRole, !alarming);

        for (int column = kSwitch; column <= kState; ++column)
        {
            row->setData(column, AlarmingRole, alarming);
        }
    }

    void DressTheRemoved(QTreeWidgetItem* row, const StartupRemovedEntry& removed)
    {
        row->setText(kSwitch, QString::fromStdString(removed.entry.label));
        row->setData(kSwitch, Qt::UserRole, AsText(removed.entry.path));
        row->setText(kPath, AsText(removed.entry.path));
        row->setToolTip(kPath, AsText(removed.entry.path));
        row->setText(kState, AsMoment(removed.removedAt));
        row->setData(kPath, QuietRole, true);
        row->setData(kState, QuietRole, true);
    }

    QString EffectSaid(const StartupUndoEffect effect)
    {
        switch (effect)
        {
        case StartupUndoEffect::RemovesTheAddedEntry: return QObject::tr("Undo: %1 added");
        case StartupUndoEffect::EditsTheEntryBack: return QObject::tr("Undo: %1 edited");
        case StartupUndoEffect::RestoresTheRemovedEntry: break;
        }

        return QObject::tr("Undo: %1 removed");
    }

    QToolButton* NewChip(QWidget* parent)
    {
        auto* chip = new QToolButton(parent);
        chip->setObjectName(QStringLiteral("FilterChip"));
        chip->setCheckable(true);
        chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        return chip;
    }

    void ShowTheCount(QToolButton* chip, const QString& said, const int count)
    {
        chip->ensurePolished();
        chip->setProperty("population", count == 0 ? "none" : "some");
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);
        chip->setText(said);
    }

    QTreeWidget* NewThreeColumnTable(QWidget* parent)
    {
        auto* table = new QTreeWidget(parent);
        table->setRootIsDecorated(false);
        table->setUniformRowHeights(true);
        table->setColumnCount(3);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setTextElideMode(Qt::ElideMiddle);
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(kSwitch, QHeaderView::Interactive);
        table->header()->setSectionResizeMode(kPath, QHeaderView::Stretch);
        table->header()->setSectionResizeMode(kState, QHeaderView::ResizeToContents);
        table->setColumnWidth(kSwitch, kProgramWidth);
        DressTheHeaderOf(table->header());
        CapTheScrollBarOf(table, table->header());

        auto* rows = new RowDelegate(table);
        rows->AlignTheCheckWithTheText();
        table->setItemDelegate(rows);

        return table;
    }

    QString PathOfTheSelectedRow(const QTreeWidget* table)
    {
        const QList<QTreeWidgetItem*> selected = table->selectedItems();

        return selected.isEmpty() ? QString() : selected.front()->data(kSwitch, Qt::UserRole).toString();
    }

    void SelectTheRowAt(QTreeWidget* table, const QString& path)
    {
        if (path.isEmpty())
        {
            return;
        }

        for (int at = 0; at < table->topLevelItemCount(); ++at)
        {
            QTreeWidgetItem* row = table->topLevelItem(at);

            if (row->data(kSwitch, Qt::UserRole).toString() == path)
            {
                row->setSelected(true);
                table->setCurrentItem(row);

                return;
            }
        }
    }

    template<typename Item, typename Dressing>
    void FillTheRows(QTreeWidget* table, const std::vector<Item>& items, const Dressing& dress)
    {
        const QString selected = PathOfTheSelectedRow(table);
        const QSignalBlocker quiet(table);
        const int wanted = static_cast<int>(items.size());

        table->clearSelection();

        while (table->topLevelItemCount() > wanted)
        {
            delete table->takeTopLevelItem(table->topLevelItemCount() - 1);
        }

        while (table->topLevelItemCount() < wanted)
        {
            table->addTopLevelItem(new QTreeWidgetItem);
        }

        for (int at = 0; at < wanted; ++at)
        {
            dress(table->topLevelItem(at), items[static_cast<std::size_t>(at)]);
        }

        SelectTheRowAt(table, selected);
    }

    QString TheNoteOnThePresetsThatFollowed(const StartupGestureOutcome& outcome)
    {
        const auto followed =
            static_cast<int>(outcome.presetsThatFollowed.size()) + (outcome.returnPresetFollowed ? 1 : 0);

        return followed == 0 ? QString() : QObject::tr("%n preset followed the new path.", nullptr, followed);
    }

    QString TheNoteOnThePresetsThatCouldNot(const StartupGestureOutcome& outcome)
    {
        QStringList names;

        for (const std::string& name : outcome.presetsThatCouldNotBeWritten)
        {
            names << QString::fromStdString(name);
        }

        if (outcome.returnPresetCouldNotBeWritten)
        {
            names << TheWayBackIsCalled();
        }

        if (names.isEmpty())
        {
            return {};
        }

        return QObject::tr("%n preset could not follow the new path and still names the old one: %1.", nullptr,
                           static_cast<int>(names.size()))
            .arg(names.join(QStringLiteral(", ")));
    }

    QString TheNoteOnPresets(const StartupGestureOutcome& outcome)
    {
        QStringList notes;

        for (const QString& note : {TheNoteOnThePresetsThatFollowed(outcome), TheNoteOnThePresetsThatCouldNot(outcome)})
        {
            if (!note.isEmpty())
            {
                notes << note;
            }
        }

        return notes.join(QLatin1Char(' '));
    }
}

StartupPage::StartupPage(StartupViewModel& viewModel, QWidget* parent) : QWidget(parent), viewModel_(viewModel)
{
    nothingToShow_ = new EmptyState(this);
    addTheFirst_ = nothingToShow_->OfferTheOnlyAction();
    nothingRemoved_ = new EmptyState(this);
    leftAlone_ = new EmptyState(this);
    turnOn_ = leftAlone_->OfferTheOnlyAction();

    panes_ = new QStackedWidget(this);
    panes_->addWidget(CreateEntriesPane());
    panes_->addWidget(nothingToShow_);
    panes_->addWidget(leftAlone_);
    panes_->addWidget(CreateRemovedPane());
    panes_->addWidget(nothingRemoved_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateToolbar());
    layout->addWidget(panes_, 1);

    CreateShortcuts();

    connect(readAgain_, &QPushButton::clicked, &viewModel_, &StartupViewModel::Show);
    connect(add_, &QPushButton::clicked, this, &StartupPage::Add);
    connect(addTheFirst_, &QPushButton::clicked, this, &StartupPage::Add);
    connect(edit_, &QPushButton::clicked, this, &StartupPage::EditSelected);
    connect(remove_, &QPushButton::clicked, this, &StartupPage::RemoveSelected);
    connect(restore_, &QPushButton::clicked, this, &StartupPage::RestoreSelected);
    connect(discard_, &QPushButton::clicked, this, &StartupPage::DiscardSelected);
    connect(undo_, &QPushButton::clicked, this, &StartupPage::UndoLast);
    connect(entries_, &QTreeWidget::itemChanged, this, &StartupPage::Toggle);
    connect(entries_, &QTreeWidget::itemSelectionChanged, this, &StartupPage::DressTheToolbar);
    connect(entries_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* row)
            {
                EditTheRowDoubleClicked(row);
            });
    connect(entries_, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& at)
            {
                ShowTheMenu(entries_, at);
            });
    connect(removed_, &QTreeWidget::itemSelectionChanged, this, &StartupPage::DressTheToolbar);
    connect(removed_, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& at)
            {
                ShowTheMenu(removed_, at);
            });
    connect(inTheFile_, &QToolButton::clicked, this,
            [this]
            {
                showingTheRemoved_ = false;
                ShowWhatTheFileSays();
            });
    connect(removedChip_, &QToolButton::clicked, this,
            [this]
            {
                showingTheRemoved_ = true;
                ShowWhatTheFileSays();
            });
    connect(turnOn_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.Manage(true);
            });
    connect(leaveAlone_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.Manage(false);
            });
    connect(&viewModel_, &StartupViewModel::Changed, this, &StartupPage::ShowWhatTheFileSays);
    connect(&viewModel_, &StartupViewModel::SettingsCouldNotBeSaved, this,
            [this]
            {
                emit StatusChanged(tr("Could not save the choice, so nothing changed."));
            });

    RetranslateUi();
    ShowWhatTheFileSays();
}

void StartupPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        ShowWhatTheFileSays();
    }

    QWidget::changeEvent(event);
}

QWidget* StartupPage::CreateToolbar()
{
    auto* toolbar = new QWidget(this);
    toolbar_ = toolbar;
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    readAgain_ = new QPushButton(toolbar);
    readAgain_->setProperty("role", "primary");

    add_ = new QPushButton(toolbar);
    edit_ = new QPushButton(toolbar);
    remove_ = new QPushButton(toolbar);
    restore_ = new QPushButton(toolbar);
    discard_ = new QPushButton(toolbar);
    undo_ = new UndoButton(toolbar);
    leaveAlone_ = new QPushButton(toolbar);

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(kToolbarGap);
    bar->addWidget(readAgain_);
    bar->addWidget(add_);
    bar->addWidget(edit_);
    bar->addWidget(remove_);
    bar->addWidget(restore_);
    bar->addWidget(discard_);
    bar->addWidget(undo_);
    bar->addSpacing(kSpringAtLeast - kToolbarGap);
    bar->addStretch();
    bar->addWidget(CreateTheViews(toolbar));
    bar->addWidget(leaveAlone_);

    return toolbar;
}

QWidget* StartupPage::CreateTheViews(QWidget* bar)
{
    auto* holder = new QWidget(bar);
    holder->setObjectName(QStringLiteral("StartupViews"));
    holder->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    inTheFile_ = NewChip(holder);
    removedChip_ = NewChip(holder);

    auto* group = new QButtonGroup(holder);
    group->addButton(inTheFile_);
    group->addButton(removedChip_);

    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, kFilterGroupGap - kToolbarGap, 0);
    line->setSpacing(kChipGap);
    line->addWidget(inTheFile_);
    line->addWidget(removedChip_);

    return holder;
}

QWidget* StartupPage::CreateEntriesPane()
{
    entries_ = NewThreeColumnTable(this);
    entries_->setObjectName(QStringLiteral("StartupEntries"));

    return entries_;
}

QWidget* StartupPage::CreateRemovedPane()
{
    removed_ = NewThreeColumnTable(this);
    removed_->setObjectName(QStringLiteral("RemovedEntries"));

    return removed_;
}

void StartupPage::CreateShortcuts()
{
    const auto when =
        [this](const QKeySequence& keys, QWidget* where, const Qt::ShortcutContext context, QPushButton* button)
    {
        auto* shortcut = new QShortcut(keys, where, nullptr, nullptr, context);
        connect(shortcut, &QShortcut::activated, button,
                [button]
                {
                    if (button->isEnabled() && button->isVisibleTo(button->window()))
                    {
                        button->click();
                    }
                });
    };

    when(QKeySequence(Qt::Key_F2), entries_, Qt::WidgetShortcut, edit_);
    when(QKeySequence(Qt::Key_Delete), entries_, Qt::WidgetShortcut, remove_);
    when(QKeySequence::Undo, this, Qt::WidgetWithChildrenShortcut, undo_);
}

void StartupPage::RetranslateUi() const
{
    readAgain_->setText(tr("Refresh"));
    add_->setText(tr("Add…"));
    edit_->setText(tr("Edit…"));
    remove_->setText(tr("Remove…"));
    restore_->setText(tr("Restore entry"));
    discard_->setText(tr("Discard…"));
    leaveAlone_->setText(tr("Stop managing"));
    leaveAlone_->setToolTip(tr("Stop managing startup entries"));
    entries_->setHeaderLabels({tr("Program"), tr("Path"), tr("State")});
    removed_->setHeaderLabels({tr("Program"), tr("Path"), tr("Removed")});

    nothingToShow_->Retell(tr("No startup entries"),
                           tr("The simulator's startup file (EXE.xml) lists no programs, or it does not exist yet "
                              "next to this profile's UserCfg.opt. Adding a program creates it."));
    addTheFirst_->setText(tr("Add a program…"));
    nothingRemoved_->Retell(tr("No removed entries"),
                            tr("Entries you remove from the startup file stay here, with their name and command "
                               "line, until you restore or discard them."));
    leftAlone_->Retell(
        tr("The startup entries are not managed"),
        tr("When managed, FS Organizer reads the simulator's startup file, lists the programs it launches with itself, "
           "and lets you add, edit, disable and remove them without editing XML."));
    turnOn_->setText(tr("Manage startup entries"));
}

void StartupPage::ShowWhatTheFileSays()
{
    FillTheTable();
    FillTheRemoved();
    DressTheToolbar();
    ChooseThePane();

    if (!viewModel_.Managing())
    {
        emit SummaryChanged(tr("The simulator's startup entries are not managed."));

        return;
    }

    if (showingTheRemoved_)
    {
        emit SummaryChanged(tr("%n removed program, kept so you can restore it.", nullptr,
                               static_cast<int>(viewModel_.Removed().size())));

        return;
    }

    emit SummaryChanged(
        tr("%n program the simulator launches with itself.", nullptr, static_cast<int>(viewModel_.Lines().size())));
}

void StartupPage::ChooseThePane()
{
    if (!viewModel_.Managing())
    {
        panes_->setCurrentIndex(LeftAlone);

        return;
    }

    if (showingTheRemoved_)
    {
        panes_->setCurrentIndex(viewModel_.Removed().empty() ? NothingRemoved : TheRemoved);

        return;
    }

    panes_->setCurrentIndex(viewModel_.Lines().empty() ? NothingToShow : TheEntries);
}

void StartupPage::FillTheTable() const
{
    FillTheRows(entries_, viewModel_.Lines(), Dress);
}

void StartupPage::FillTheRemoved() const
{
    FillTheRows(removed_, viewModel_.Removed(), DressTheRemoved);
}

void StartupPage::DressTheToolbar() const
{
    const std::optional<std::chrono::system_clock::time_point> read = viewModel_.ReadAt();
    const bool managing = viewModel_.Managing();
    const bool inTheRemoved = showingTheRemoved_;

    readAgain_->setToolTip(read.has_value() ? tr("startup file · read %1").arg(AsMoment(*read))
                                            : tr("startup file · not read"));

    add_->setVisible(!inTheRemoved);
    edit_->setVisible(!inTheRemoved);
    remove_->setVisible(!inTheRemoved);
    restore_->setVisible(inTheRemoved);
    discard_->setVisible(inTheRemoved);

    const bool oneEntry = !entries_->selectedItems().isEmpty();
    const bool oneRemoved = !removed_->selectedItems().isEmpty();

    edit_->setEnabled(oneEntry);
    remove_->setEnabled(oneEntry);
    restore_->setEnabled(oneRemoved);
    discard_->setEnabled(oneRemoved);

    DressTheViews();
    DressTheUndo();
    toolbar_->setVisible(managing);
}

void StartupPage::DressTheViews() const
{
    const auto inTheFile = static_cast<int>(viewModel_.Lines().size());
    const auto removed = static_cast<int>(viewModel_.Removed().size());

    ShowTheCount(inTheFile_, tr("In the file %1").arg(inTheFile), inTheFile);
    ShowTheCount(removedChip_, tr("Removed %1").arg(removed), removed);

    inTheFile_->setChecked(!showingTheRemoved_);
    removedChip_->setChecked(showingTheRemoved_);
}

void StartupPage::DressTheUndo() const
{
    const std::optional<StartupUndoPlan> plan = viewModel_.UndoPlan();

    if (!plan.has_value())
    {
        undo_->NothingToUndo(tr("Undo"));

        return;
    }

    undo_->Name(EffectSaid(plan->effect), QString::fromStdString(plan->label));
}

void StartupPage::Toggle(QTreeWidgetItem* row, const int column)
{
    if (row == nullptr || column != kSwitch)
    {
        return;
    }

    Apply(WhatTheRowAsks{.path = AsPath(row->data(kSwitch, Qt::UserRole).toString()),
                         .enabled = row->checkState(kSwitch) == Qt::Checked,
                         .label = row->text(kSwitch)});
}

void StartupPage::Apply(const WhatTheRowAsks& asked)
{
    if (TheSimulatorIsInTheWay())
    {
        ShowWhatTheFileSays();
        return;
    }

    const FileResult result = viewModel_.Switch(asked.path, asked.enabled);
    if (!Succeeded(result))
    {
        emit StatusChanged(tr("Nothing changed: %1.").arg(Explain(result)));
        ShowWhatTheFileSays();

        return;
    }

    emit StatusChanged(asked.enabled ? tr("%1 will start with the simulator.").arg(asked.label)
                                     : tr("%1 will not start with the simulator.").arg(asked.label));
}

void StartupPage::Add()
{
    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    StartupDraftDialog dialog(
        [this](const std::filesystem::path& file)
        {
            return viewModel_.Check(file, std::nullopt);
        },
        std::nullopt, this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const StartupDraft draft = dialog.Draft();

    Settle(viewModel_.Add(draft), tr("%1 added to the startup file.").arg(QString::fromStdString(draft.label)));
}

void StartupPage::EditSelected()
{
    const StartupLine* selected = SelectedLine();

    if (selected != nullptr)
    {
        EditTheLine(*selected);
    }
}

void StartupPage::EditTheRowDoubleClicked(QTreeWidgetItem* row)
{
    const auto* rows = qobject_cast<const RowDelegate*>(entries_->itemDelegate());

    if (row == nullptr || (rows != nullptr && rows->DoubleClickedTheCheck()))
    {
        return;
    }

    const StartupLine* clicked = LineAt(row->data(kSwitch, Qt::UserRole).toString());

    if (clicked != nullptr)
    {
        EditTheLine(*clicked);
    }
}

void StartupPage::EditTheLine(const StartupLine& wanted)
{
    const StartupLine line = wanted;

    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    const std::filesystem::path entryPath = line.path;

    StartupDraftDialog dialog(
        [this, entryPath](const std::filesystem::path& file)
        {
            return viewModel_.Check(file, entryPath);
        },
        StartupDraft{.label = line.label, .file = line.path, .commandLine = line.commandLine}, this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const StartupDraft draft = dialog.Draft();
    const StartupGestureOutcome outcome = viewModel_.Edit(entryPath, draft);

    if (Succeeded(outcome.result) && outcome.changedNothing)
    {
        emit StatusChanged(tr("Nothing to save: %1 is already like that.").arg(QString::fromStdString(draft.label)));

        return;
    }

    Settle(outcome, tr("%1 saved to the startup file.").arg(QString::fromStdString(draft.label)));
}

void StartupPage::RemoveSelected()
{
    const StartupLine* selected = SelectedLine();

    if (selected == nullptr)
    {
        return;
    }

    const StartupLine line = *selected;

    if (TheSimulatorIsInTheWay() || !ConfirmTheRemoval(line))
    {
        return;
    }

    const QString done = tr("%1 removed from the startup file.").arg(QString::fromStdString(line.label));

    Settle(viewModel_.Remove(line.path), done);
}

void StartupPage::RestoreSelected()
{
    const StartupRemovedEntry* selected = SelectedRemoved();

    if (selected == nullptr)
    {
        return;
    }

    const StartupEntry entry = selected->entry;

    if (TheSimulatorIsInTheWay())
    {
        return;
    }

    const QString done = tr("%1 is back in the startup file.").arg(QString::fromStdString(entry.label));

    Settle(viewModel_.Restore(entry.path), done);
}

void StartupPage::DiscardSelected()
{
    const StartupRemovedEntry* selected = SelectedRemoved();

    if (selected == nullptr)
    {
        return;
    }

    const StartupRemovedEntry removed = *selected;

    if (!ConfirmTheDiscard(removed))
    {
        return;
    }

    const QString done = tr("%1 discarded.").arg(QString::fromStdString(removed.entry.label));

    Settle(viewModel_.Discard(removed.entry.path), done);
}

void StartupPage::UndoLast()
{
    const std::optional<StartupUndoPlan> plan = viewModel_.UndoPlan();

    if (!plan.has_value() || TheSimulatorIsInTheWay())
    {
        return;
    }

    const std::optional<StartupGestureOutcome> outcome = viewModel_.Undo();

    if (outcome.has_value())
    {
        Settle(*outcome, tr("Undid the last change to %1.").arg(QString::fromStdString(plan->label)));
    }
}

void StartupPage::ShowTheMenu(QTreeWidget* table, const QPoint& at)
{
    QTreeWidgetItem* row = table->itemAt(at);

    if (row == nullptr)
    {
        return;
    }

    table->setCurrentItem(row);
    row->setSelected(true);

    QMenu menu(table);

    if (table == entries_)
    {
        menu.addAction(tr("Edit…"), this, &StartupPage::EditSelected);
        menu.addAction(tr("Remove…"), this, &StartupPage::RemoveSelected);
    }
    else
    {
        menu.addAction(tr("Restore entry"), this, &StartupPage::RestoreSelected);
        menu.addAction(tr("Discard…"), this, &StartupPage::DiscardSelected);
    }

    menu.exec(table->viewport()->mapToGlobal(at));
}

void StartupPage::Settle(const StartupGestureOutcome& outcome, const QString& done)
{
    if (!Succeeded(outcome.result))
    {
        emit StatusChanged(tr("Nothing changed: %1.").arg(Explain(outcome.result)));
        ShowWhatTheFileSays();

        return;
    }

    const QString note = TheNoteOnPresets(outcome);

    emit StatusChanged(note.isEmpty() ? done : done + QLatin1Char(' ') + note);
}

bool StartupPage::ConfirmTheRemoval(const StartupLine& line)
{
    QStringList sentences;

    const QString takesOut = tr("Removing takes its name and command line out of the startup file.");
    const QString keeps = tr("FS Organizer keeps the entry in Removed, where you can restore it.");

    switch (line.condition)
    {
    case StartupCondition::Reachable:
        if (line.enabled)
        {
            sentences << tr("Disabling this entry already stops the simulator from launching the program.");
        }

        sentences << takesOut << keeps;
        break;
    case StartupCondition::BehindADisabledAddon:
        sentences << tr("Its program is inside a disabled addon.")
                  << tr("The entry works again when you enable the addon.") << takesOut << keeps;
        break;
    case StartupCondition::Broken: sentences << tr("Its program no longer exists."); break;
    case StartupCondition::Unavailable:
        sentences << tr("Its drive is not connected right now, so the program may come back.") << takesOut << keeps;
        break;
    }

    return Confirmed(tr("Remove startup entry"),
                     tr("Remove %1 from the startup file?").arg(QString::fromStdString(line.label)),
                     sentences.join(QLatin1Char('\n')), tr("Remove"));
}

bool StartupPage::ConfirmTheDiscard(const StartupRemovedEntry& removed)
{
    const QStringList sentences{tr("FS Organizer deletes the name, path and command line it kept."),
                                tr("This cannot be undone.")};

    return Confirmed(tr("Discard removed entry"),
                     tr("Discard %1 for good?").arg(QString::fromStdString(removed.entry.label)),
                     sentences.join(QLatin1Char('\n')), tr("Discard", "forgets a removed startup entry for good"));
}

bool StartupPage::Confirmed(const QString& title, const QString& asked, const QString& explained, const QString& go)
{
    QMessageBox question(QMessageBox::Question, title, asked, QMessageBox::NoButton, this);
    question.setInformativeText(explained);

    QPushButton* proceed = question.addButton(go, QMessageBox::AcceptRole);
    question.addButton(tr("Cancel"), QMessageBox::RejectRole);
    question.setDefaultButton(proceed);

    if (auto* asking = question.findChild<QLabel*>(QStringLiteral("qt_msgbox_label")))
    {
        asking->setMinimumWidth(kReadableWidth);
    }

    question.exec();

    return question.clickedButton() == proceed;
}

const StartupLine* StartupPage::SelectedLine() const
{
    return LineAt(PathOfTheSelectedRow(entries_));
}

const StartupLine* StartupPage::LineAt(const QString& path) const
{
    for (const StartupLine& line : viewModel_.Lines())
    {
        if (AsText(line.path) == path)
        {
            return &line;
        }
    }

    return nullptr;
}

const StartupRemovedEntry* StartupPage::SelectedRemoved() const
{
    const QString selected = PathOfTheSelectedRow(removed_);

    for (const StartupRemovedEntry& removed : viewModel_.Removed())
    {
        if (AsText(removed.entry.path) == selected)
        {
            return &removed;
        }
    }

    return nullptr;
}

bool StartupPage::TheSimulatorIsInTheWay()
{
    while (const std::optional<std::string> running = viewModel_.RunningSimulator())
    {
        QMessageBox blocked(QMessageBox::Warning, tr("Simulator open"),
                            tr("The startup file cannot be changed while the simulator is running."),
                            QMessageBox::Cancel, this);
        blocked.setInformativeText(tr("Close %1 and check again.").arg(QString::fromStdString(*running)));

        const QPushButton* again = blocked.addButton(tr("Check again"), QMessageBox::AcceptRole);
        blocked.exec();

        if (blocked.clickedButton() != again)
        {
            return true;
        }
    }

    return false;
}
