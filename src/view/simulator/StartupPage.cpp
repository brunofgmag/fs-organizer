#include "view/simulator/StartupPage.h"

#include <QtCore/QEvent>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "support/PathText.h"
#include "view/delegates/RowDelegate.h"
#include "view/TreeColumns.h"
#include "view/panels/EmptyState.h"
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
        switch (line.alarm)
        {
        case StartupAlarm::TheExecutableIsMissing: return QObject::tr("the program is not there");
        case StartupAlarm::TheAddonHoldingItIsOff: return QObject::tr("the addon that holds it is off");
        case StartupAlarm::None: break;
        }

        return line.reach == StartupReach::InsideAnAddon ? QObject::tr("Inside an addon")
                                                         : QObject::tr("outside your addons");
    }

    bool ItIsATag(const StartupLine& line)
    {
        return line.alarm == StartupAlarm::None && line.reach == StartupReach::InsideAnAddon;
    }

    void Dress(QTreeWidgetItem* row, const StartupLine& line)
    {
        row->setText(kSwitch, QString::fromStdString(line.label));
        row->setCheckState(kSwitch, line.enabled ? Qt::Checked : Qt::Unchecked);
        row->setData(kSwitch, Qt::UserRole, AsText(line.path));
        row->setText(kPath, AsText(line.path));
        row->setToolTip(kPath, AsText(line.path));

        const bool tagged = ItIsATag(line);
        row->setText(kState, tagged ? QString() : StateOf(line));
        row->setData(kState, TagTextRole, tagged ? StateOf(line) : QString());
        row->setData(kState, TagToneRole, static_cast<int>(TagTone::Muted));

        row->setData(kPath, QuietRole, true);
        row->setData(kState, QuietRole, line.alarm == StartupAlarm::None);

        for (int column = kSwitch; column <= kState; ++column)
        {
            row->setData(column, AlarmingRole, line.alarm != StartupAlarm::None);
        }
    }
}

StartupPage::StartupPage(StartupViewModel& viewModel, QWidget* parent) : QWidget(parent), viewModel_(viewModel)
{
    nothingToShow_ = new EmptyState(this);
    leftAlone_ = new EmptyState(this);
    turnOn_ = leftAlone_->OfferTheOnlyAction();

    panes_ = new QStackedWidget(this);
    panes_->addWidget(CreateEntriesPane());
    panes_->addWidget(nothingToShow_);
    panes_->addWidget(leftAlone_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateToolbar());
    layout->addWidget(panes_, 1);

    connect(readAgain_, &QPushButton::clicked, &viewModel_, &StartupViewModel::Show);
    connect(entries_, &QTreeWidget::itemChanged, this, &StartupPage::Toggle);
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
                emit StatusChanged(tr("The app could not write the choice down, so it stays as it was."));
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

    readAt_ = new QLabel(toolbar);
    readAt_->setObjectName(QStringLiteral("PanelPromise"));

    leaveAlone_ = new QPushButton(toolbar);

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(readAgain_);
    bar->addWidget(readAt_);
    bar->addStretch();
    bar->addWidget(leaveAlone_);

    return toolbar;
}

QWidget* StartupPage::CreateEntriesPane()
{
    auto* pane = new QWidget(this);

    entries_ = new QTreeWidget(pane);
    entries_->setRootIsDecorated(false);
    entries_->setUniformRowHeights(true);
    entries_->setColumnCount(3);
    entries_->header()->setStretchLastSection(false);
    entries_->header()->setSectionResizeMode(kSwitch, QHeaderView::Interactive);
    entries_->header()->setSectionResizeMode(kPath, QHeaderView::Stretch);
    LetTheseColumnsBeDragged(entries_, {kState});
    entries_->setColumnWidth(kSwitch, kProgramWidth);
    DressTheHeaderOf(entries_->header());

    entries_->setItemDelegate(new RowDelegate(entries_));

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(entries_, 1);

    return pane;
}

void StartupPage::RetranslateUi() const
{
    readAgain_->setText(tr("Read it again"));
    leaveAlone_->setText(tr("Stop managing these"));
    entries_->setHeaderLabels({tr("Program"), tr("Path"), tr("State")});

    nothingToShow_->Retell(tr("No startup entry to show"),
                           tr("The startup file of this profile was not found beside its UserCfg.opt, or it carries no "
                              "program. Nothing was written."));
    leftAlone_->Retell(tr("The startup entries are not managed"),
                       tr("Manage these and FS Organizer reads the startup file of the simulator, lists the programs "
                          "it launches with itself, and lets you switch one off without editing XML. It changes one "
                          "thing only: the switch of an entry that is already there."));
    turnOn_->setText(tr("Manage these"));
}

void StartupPage::ShowWhatTheFileSays()
{
    FillTheTable();
    DressTheToolbar();

    if (!viewModel_.Managing())
    {
        panes_->setCurrentIndex(LeftAlone);
        emit SummaryChanged(tr("The startup entries of the simulator are not managed."));

        return;
    }

    const std::vector<StartupLine>& lines = viewModel_.Lines();
    panes_->setCurrentIndex(lines.empty() ? NothingToShow : TheEntries);

    emit SummaryChanged(tr("%n program the simulator launches with itself.", nullptr, static_cast<int>(lines.size())));
}

void StartupPage::FillTheTable() const
{
    const QSignalBlocker quiet(entries_);
    const std::vector<StartupLine>& lines = viewModel_.Lines();
    const int wanted = static_cast<int>(lines.size());

    while (entries_->topLevelItemCount() > wanted)
    {
        delete entries_->takeTopLevelItem(entries_->topLevelItemCount() - 1);
    }

    while (entries_->topLevelItemCount() < wanted)
    {
        entries_->addTopLevelItem(new QTreeWidgetItem);
    }

    for (int at = 0; at < wanted; ++at)
    {
        Dress(entries_->topLevelItem(at), lines[static_cast<std::size_t>(at)]);
    }

    WidenTheseColumnsToTheirRows(entries_, {kState});
}

void StartupPage::DressTheToolbar() const
{
    const std::optional<std::chrono::system_clock::time_point> read = viewModel_.ReadAt();

    readAt_->setText(read.has_value() ? tr("startup file · read %1").arg(AsMoment(*read))
                                      : tr("startup file · not read"));
    toolbar_->setVisible(viewModel_.Managing());
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
        emit StatusChanged(tr("The switch was not changed: %1.").arg(Explain(result)));
        ShowWhatTheFileSays();

        return;
    }

    emit StatusChanged(asked.enabled ? tr("%1 will start with the simulator.").arg(asked.label)
                                     : tr("%1 will not start with the simulator.").arg(asked.label));
}

bool StartupPage::TheSimulatorIsInTheWay()
{
    while (const std::optional<std::string> running = viewModel_.RunningSimulator())
    {
        QMessageBox blocked(QMessageBox::Warning, tr("Simulator open"),
                            tr("The startup file stays untouched while the simulator runs."), QMessageBox::Cancel,
                            this);
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
