#include "view/simulator/PackageListPage.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "view/delegates/RowDelegate.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    constexpr int kAirport = 0;
    constexpr int kCovered = 1;
    constexpr int kAndBy = 2;
    constexpr int kAirportWidth = 90;
    constexpr int kNameWidth = 300;

    void Dress(QTreeWidgetItem* row, const CoverageLine& line)
    {
        row->setText(kAirport, line.code);
        row->setText(kCovered, line.covered);
        row->setToolTip(kCovered, line.covered);
        row->setText(kAndBy, line.andBy);
        row->setToolTip(kAndBy, line.andBy);
        row->setData(kAndBy, QuietRole, !line.againstTheSimulator);
        row->setData(kAndBy, TagTextRole,
                     line.againstTheSimulator ? QObject::tr("the simulator") : QObject::tr("your library"));
        row->setData(kAndBy, TagToneRole, static_cast<int>(TagTone::Muted));
    }

    void Dress(QTreeWidgetItem* row, const TurnedOffLine& line)
    {
        row->setText(kAirport, line.code);
        row->setText(kCovered, line.name);
        row->setToolTip(kCovered, line.name);
        row->setData(kCovered, QuietRole, true);
    }
}

PackageListPage::PackageListPage(CoverageViewModel& viewModel, QWidget* parent) : QWidget(parent), viewModel_(viewModel)
{
    leftAlone_ = new EmptyState(this);
    turnOn_ = leftAlone_->OfferTheOnlyAction();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateToolbar());
    layout->addWidget(CreateHalves(), 1);

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
    connect(conflicts_, &QTreeWidget::itemSelectionChanged, this, &PackageListPage::DressTheActions);
    connect(turnedOff_, &QTreeWidget::itemSelectionChanged, this, &PackageListPage::DressTheActions);
    connect(turnOff_, &QPushButton::clicked, this, &PackageListPage::TurnTheSimulatorsOneOff);
    connect(coexist_, &QPushButton::clicked, this, &PackageListPage::LetThemCoexist);
    connect(turnBackOn_, &QPushButton::clicked, this, &PackageListPage::TurnItBackOn);
    connect(&viewModel_, &CoverageViewModel::Changed, this, &PackageListPage::ShowWhatTheListSays);
    connect(&viewModel_, &CoverageViewModel::SettingsCouldNotBeSaved, this,
            [this]
            {
                emit StatusChanged(tr("The app could not write the choice down, so it stays as it was."));
            });

    RetranslateUi();
    ShowWhatTheListSays();
}

void PackageListPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        ShowWhatTheListSays();
    }

    QWidget::changeEvent(event);
}

QWidget* PackageListPage::CreateToolbar()
{
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    turnOff_ = new QPushButton(toolbar);
    turnOff_->setProperty("role", "primary");
    coexist_ = new QPushButton(toolbar);
    turnBackOn_ = new QPushButton(toolbar);
    leaveAlone_ = new QPushButton(toolbar);

    readAt_ = new QLabel(toolbar);
    readAt_->setObjectName(QStringLiteral("PanelPromise"));

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(turnOff_);
    bar->addWidget(coexist_);
    bar->addWidget(turnBackOn_);
    bar->addWidget(readAt_);
    bar->addStretch();
    bar->addWidget(leaveAlone_);

    return toolbar;
}

QWidget* PackageListPage::CreateHalves()
{
    auto* pane = new QWidget(this);

    conflictsHeading_ = new QLabel(pane);
    conflictsHeading_->setObjectName(QStringLiteral("PanelSubHeading"));

    conflictsPromise_ = new QLabel(pane);
    conflictsPromise_->setObjectName(QStringLiteral("PanelPromise"));
    conflictsPromise_->setWordWrap(true);

    conflicts_ = new QTreeWidget(pane);
    turnedOff_ = new QTreeWidget(pane);

    turnedOffHeading_ = new QLabel(pane);
    turnedOffHeading_->setObjectName(QStringLiteral("PanelSubHeading"));

    for (QTreeWidget* table : {conflicts_, turnedOff_})
    {
        table->setRootIsDecorated(false);
        table->setUniformRowHeights(true);
        table->setColumnCount(3);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->header()->setStretchLastSection(true);
        table->header()->setSectionResizeMode(kAirport, QHeaderView::Fixed);
        table->header()->setSectionResizeMode(kCovered, QHeaderView::Interactive);
        table->setColumnWidth(kAirport, kAirportWidth);
        table->setColumnWidth(kCovered, kNameWidth);
        DressTheHeaderOf(table->header());
        table->setItemDelegate(new RowDelegate(table));
    }

    const auto insetLikeAToolbar = [](QWidget* label)
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
        row->addWidget(label);

        return row;
    };

    auto* packages = new QWidget(pane);
    auto* half = new QVBoxLayout(packages);
    half->setContentsMargins(0, 0, 0, 0);
    half->setSpacing(0);
    half->addLayout(insetLikeAToolbar(turnedOffHeading_));
    half->addWidget(turnedOff_, 1);

    secondHalf_ = new QStackedWidget(pane);
    secondHalf_->addWidget(packages);
    secondHalf_->addWidget(leftAlone_);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(insetLikeAToolbar(conflictsHeading_));
    layout->addWidget(conflicts_, 1);
    layout->addLayout(insetLikeAToolbar(conflictsPromise_));
    layout->addWidget(secondHalf_, 1);

    return pane;
}

void PackageListPage::RetranslateUi() const
{
    turnOff_->setText(tr("Turn the simulator's one off"));
    coexist_->setText(tr("They can coexist"));
    turnBackOn_->setText(tr("Turn it back on"));
    leaveAlone_->setText(tr("Stop managing this"));

    conflicts_->setHeaderLabels({tr("Airport"), tr("Covered by"), tr("And by")});
    turnedOff_->setHeaderLabels({tr("Airport"), tr("Package you turned off"), QString()});

    leftAlone_->Retell(tr("The package list is not managed"),
                       tr("Manage it and FS Organizer reads the package list of the simulator, tells you when one of "
                          "your airports covers the same place as one the simulator ships, and lets you switch that "
                          "one off without editing XML. It writes nothing until you accept a warning. The warning "
                          "between two addons of your own does not need this and keeps working."));
    turnOn_->setText(tr("Manage it"));
}

void PackageListPage::ShowWhatTheListSays()
{
    FillTheConflicts();
    FillTheTurnedOff();
    DressTheToolbar();
    DressTheActions();

    secondHalf_->setCurrentIndex(viewModel_.Managing() ? ThePackagesYouTurnedOff : LeftAlone);

    const QString covered = tr("%n airport covered twice.", nullptr, static_cast<int>(viewModel_.Conflicts().size()));

    if (viewModel_.Managing())
    {
        emit SummaryChanged(covered);

        return;
    }

    emit SummaryChanged(covered + QLatin1Char(' ') + tr("The package list of the simulator is not managed."));
}

void PackageListPage::FillTheConflicts() const
{
    const std::vector<CoverageLine>& lines = viewModel_.Conflicts();

    conflicts_->clear();

    for (const CoverageLine& line : lines)
    {
        auto* row = new QTreeWidgetItem(conflicts_);
        Dress(row, line);
    }

    conflictsHeading_->setText(tr("Airports covered twice") + QStringLiteral("  ") + QString::number(lines.size()));

    const std::size_t read = viewModel_.AddonsWhoseSceneryWasRead();

    conflictsPromise_->setText(
        read == 0 ? tr("No scenery has been read yet, so this half has nothing to say. The Diagnostics screen reads "
                       "them all in one go, and enabling an airport reads that one.")
                  : tr("Read from the scenery of %n addon. The app never turns anything off by itself: between two "
                       "addons of your own it only shows the pair, because turning one off is enabling and disabling, "
                       "which you already do.",
                       nullptr, static_cast<int>(read)));
}

void PackageListPage::FillTheTurnedOff() const
{
    const std::vector<TurnedOffLine>& lines = viewModel_.TurnedOff();

    turnedOff_->clear();

    for (const TurnedOffLine& line : lines)
    {
        auto* row = new QTreeWidgetItem(turnedOff_);
        Dress(row, line);
    }

    turnedOffHeading_->setText(tr("Packages you turned off") + QStringLiteral("  ") + QString::number(lines.size()));
}

void PackageListPage::DressTheToolbar() const
{
    const std::optional<std::chrono::system_clock::time_point> read = viewModel_.ReadAt();

    readAt_->setText(read.has_value() ? tr("package list · read %1").arg(AsMoment(*read))
                                      : tr("package list · not read"));

    for (QPushButton* onlyForTheSimulator : {turnOff_, turnBackOn_, leaveAlone_})
    {
        onlyForTheSimulator->setVisible(viewModel_.Managing());
    }
}

const CoverageLine* PackageListPage::TheChosenConflict() const
{
    const QList<QTreeWidgetItem*> chosen = conflicts_->selectedItems();
    if (chosen.isEmpty())
    {
        return nullptr;
    }

    const int at = conflicts_->indexOfTopLevelItem(chosen.first());
    const std::vector<CoverageLine>& lines = viewModel_.Conflicts();

    return at < 0 || static_cast<std::size_t>(at) >= lines.size() ? nullptr : &lines[static_cast<std::size_t>(at)];
}

void PackageListPage::DressTheActions() const
{
    const CoverageLine* chosen = TheChosenConflict();

    turnOff_->setEnabled(chosen != nullptr && chosen->againstTheSimulator);
    coexist_->setEnabled(chosen != nullptr && !chosen->againstTheSimulator);
    turnBackOn_->setEnabled(!turnedOff_->selectedItems().isEmpty());
}

void PackageListPage::TurnTheSimulatorsOneOff()
{
    const CoverageLine* chosen = TheChosenConflict();
    if (chosen == nullptr || !chosen->againstTheSimulator || TheSimulatorIsInTheWay())
    {
        return;
    }

    const QString name = chosen->andBy;

    Report(viewModel_.Switch(chosen->packageName, false), tr("%1 will not load with the simulator.").arg(name));
}

void PackageListPage::LetThemCoexist()
{
    const CoverageLine* chosen = TheChosenConflict();
    if (chosen == nullptr || chosen->againstTheSimulator)
    {
        return;
    }

    const QString code = chosen->code;
    const AddonId one = chosen->one;
    const AddonId other = chosen->other;

    viewModel_.TheyCanCoexist(one, other);

    emit StatusChanged(tr("The two addons of %1 will not be shown as covering each other again.").arg(code));
}

void PackageListPage::TurnItBackOn()
{
    const QList<QTreeWidgetItem*> chosen = turnedOff_->selectedItems();
    if (chosen.isEmpty() || TheSimulatorIsInTheWay())
    {
        return;
    }

    const int at = turnedOff_->indexOfTopLevelItem(chosen.first());
    const std::vector<TurnedOffLine>& lines = viewModel_.TurnedOff();
    if (at < 0 || static_cast<std::size_t>(at) >= lines.size())
    {
        return;
    }

    const QString name = lines[static_cast<std::size_t>(at)].name;

    Report(viewModel_.Switch(name.toStdString(), true), tr("%1 will load with the simulator again.").arg(name));
}

void PackageListPage::Report(const FileResult result, const QString& done)
{
    emit StatusChanged(Succeeded(result) ? done : tr("The package list was not changed: %1.").arg(Explain(result)));

    if (!Succeeded(result))
    {
        ShowWhatTheListSays();
    }
}

bool PackageListPage::TheSimulatorIsInTheWay()
{
    while (const std::optional<std::string> running = viewModel_.RunningSimulator())
    {
        QMessageBox blocked(QMessageBox::Warning, tr("Simulator open"),
                            tr("The package list stays untouched while the simulator runs."), QMessageBox::Cancel,
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
