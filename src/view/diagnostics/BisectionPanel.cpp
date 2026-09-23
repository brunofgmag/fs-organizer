#include "view/diagnostics/BisectionPanel.h"

#include <algorithm>

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "support/PathText.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kHistoryWidth = 250;
    constexpr int kBetweenEntries = 11;
    constexpr int kUnderTheTitle = 4;

    [[nodiscard]] QLabel* Quiet(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("PanelPromise"));
        label->setWordWrap(true);

        return label;
    }

    [[nodiscard]] QLabel* Stressed(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setWordWrap(true);

        QFont bold = label->font();
        bold.setWeight(QFont::DemiBold);
        label->setFont(bold);

        return label;
    }

    [[nodiscard]] QLabel* Loud(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setWordWrap(true);

        return label;
    }

    [[nodiscard]] QTreeWidget* UnitTable(const QString& name, QWidget* parent)
    {
        auto* tree = new QTreeWidget(parent);
        tree->setObjectName(name);
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setColumnCount(3);
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
        DressTheHeaderOf(tree->header());
        tree->setItemDelegate(new RowDelegate(tree));
        tree->setTextElideMode(Qt::ElideMiddle);

        return tree;
    }

    [[nodiscard]] QTreeWidget* ThatOpensIntoItsMembers(QTreeWidget* tree)
    {
        tree->setRootIsDecorated(true);

        return tree;
    }

    [[nodiscard]] QVBoxLayout* AColumnInside(QWidget* pane)
    {
        auto* column = new QVBoxLayout(pane);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);

        return column;
    }

    [[nodiscard]] QVBoxLayout* TheProseAtTheTopOf(QVBoxLayout* column)
    {
        auto* prose = new QVBoxLayout;
        prose->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
        prose->setSpacing(8);
        column->addLayout(prose);

        return prose;
    }

    [[nodiscard]] std::size_t WritingApartIn(const UnitOnScreen& unit)
    {
        return static_cast<std::size_t>(std::ranges::count_if(unit.members,
                                                              [](const MemberOnScreen& member)
                                                              {
                                                                  return member.writesWith == 0;
                                                              }));
    }

    [[nodiscard]] QString HeldByTheModelFolderName(const UnitOnScreen& unit)
    {
        const std::size_t apart = WritingApartIn(unit);

        if (apart == 0 || apart == unit.members.size())
        {
            return QObject::tr("grouped only by a shared model folder name");
        }

        return QObject::tr(
                   "%n of them grouped only by the model folder name, the other %1 by a shared folder inside the model",
                   nullptr, static_cast<int>(apart))
            .arg(unit.members.size() - apart);
    }

    [[nodiscard]] QString HowTheyAreCoupled(const UnitOnScreen& unit)
    {
        switch (unit.coupling)
        {
        case Coupling::Merge: return QObject::tr("they share a model folder without overlapping files");
        case Coupling::Shadowing: return QObject::tr("one of them overwrites a file of another");
        case Coupling::OnlyTheSharedModelFolder: return HeldByTheModelFolderName(unit);
        case Coupling::Alone:
        case Coupling::NotYetMeasured: break;
        }

        return QString();
    }

    [[nodiscard]] QString HowItIsHeld(const MemberOnScreen& member)
    {
        if (member.writesWith == 0)
        {
            return QObject::tr("only the model folder name in common");
        }

        return QObject::tr("shares a folder inside the model with %n other", nullptr,
                           static_cast<int>(member.writesWith));
    }

    [[nodiscard]] QString AtWhatTime(const std::chrono::system_clock::time_point at)
    {
        return AsLocalTime(at).toString(QLatin1String("HH:mm:ss"));
    }

    [[nodiscard]] QString TheNameOf(const AnsweredRound& answered)
    {
        if (ItIsTheReferenceRound(answered))
        {
            return QObject::tr("Reference");
        }

        if (answered.pass == BisectionPass::InsideTheGroup)
        {
            return QObject::tr("Round %1, inside the group").arg(answered.number);
        }

        return QObject::tr("Round %1").arg(answered.number);
    }

    [[nodiscard]] QString WhatItSettled(const AnsweredRound& answered)
    {
        if (ItIsTheReferenceRound(answered))
        {
            return answered.answer == BisectionAnswer::ItCrashed ? QObject::tr("No addons enabled: it crashed.")
                                                                 : QObject::tr("No addons enabled: it worked.");
        }

        const QString told = answered.answer == BisectionAnswer::ItCrashed
            ? QObject::tr("%n unit enabled: it crashed.", nullptr, static_cast<int>(answered.unitsOn))
            : QObject::tr("%n unit enabled: it worked.", nullptr, static_cast<int>(answered.unitsOn));

        return QObject::tr("%1 %2 ruled out, %3 left.").arg(told).arg(answered.unitsCleared).arg(answered.unitsLeft);
    }

    [[nodiscard]] QString WhatMoved(const DriftKind kind)
    {
        switch (kind)
        {
        case DriftKind::ALinkWeLeftIsGone: return QObject::tr("a link made by this app is gone");
        case DriftKind::AnEntryWeDidNotLeaveIsThere: return QObject::tr("an entry this app did not make appeared");
        case DriftKind::AnEntryPointsSomewhereElse: return QObject::tr("an entry points somewhere else now");
        case DriftKind::AnAddonLeftTheLibrary: return QObject::tr("an addon was removed from the library");
        case DriftKind::AnAddonJoinedTheLibrary: return QObject::tr("an addon was added to the library");
        }

        return QString();
    }
}

BisectionPanel::BisectionPanel(BisectionViewModel& viewModel, QWidget* parent) : QWidget(parent), viewModel_(viewModel)
{
    headline_ = Loud(this);
    headline_->setObjectName(QStringLiteral("SectionHeadline"));

    body_ = new QStackedWidget(this);
    body_->addWidget(CreateTheOpening());
    body_->addWidget(CreateTheRound());
    body_->addWidget(CreateTheDrift());
    body_->addWidget(CreateTheOutcome());
    body_->addWidget(CreateWhatJoinedTheLibrary());

    auto* split = new QHBoxLayout;
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(0);
    split->addWidget(body_, 1);
    split->addWidget(CreateWhatHappenedSoFar());

    auto* title = new QVBoxLayout;
    title->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, 0);
    title->addWidget(headline_);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(title);
    column->addLayout(split, 1);

    connect(start_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Begin);
    connect(crashed_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.Answer(BisectionAnswer::ItCrashed);
            });
    connect(ranFine_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.Answer(BisectionAnswer::ItRanFine);
            });
    connect(stop_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Stop);
    connect(giveUp_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Stop);
    connect(giveUpInstead_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Stop);
    connect(carryOn_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::CarryOn);
    connect(finish_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Stop);
    connect(refine_, &QPushButton::clicked, &viewModel_, &BisectionViewModel::Refine);
    connect(startOver_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.Stop();
                viewModel_.Begin();
            });
    connect(bringThemIn_, &QPushButton::clicked, this, &BisectionPanel::ImportRequested);
    connect(&viewModel_, &BisectionViewModel::Changed, this, &BisectionPanel::ShowWhereItStands);

    RetranslateUi();
}

void BisectionPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

QWidget* BisectionPanel::CreateTheOpening()
{
    auto* pane = new QWidget(this);

    announced_ = Loud(pane);
    outOfReach_ = Quiet(pane);
    promise_ = Quiet(pane);
    toBeSearched_ = ThatOpensIntoItsMembers(UnitTable(QStringLiteral("BisectionUnits"), pane));
    start_ = new QPushButton(pane);
    start_->setObjectName(QStringLiteral("PrimaryButton"));

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(start_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    QVBoxLayout* prose = TheProseAtTheTopOf(column);
    prose->addWidget(announced_);
    prose->addWidget(outOfReach_);
    prose->addWidget(promise_);
    prose->addLayout(buttons);
    column->addWidget(toBeSearched_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateTheRound()
{
    auto* pane = new QWidget(this);

    standing_ = Quiet(pane);
    ask_ = Loud(pane);
    hint_ = Quiet(pane);
    turnedOn_ = ThatOpensIntoItsMembers(UnitTable(QStringLiteral("BisectionTurnedOn"), pane));
    crashed_ = new QPushButton(pane);
    crashed_->setObjectName(QStringLiteral("PrimaryButton"));
    ranFine_ = new QPushButton(pane);
    stop_ = new QPushButton(pane);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    buttons->addWidget(crashed_);
    buttons->addWidget(ranFine_);
    buttons->addWidget(stop_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    QVBoxLayout* prose = TheProseAtTheTopOf(column);
    prose->addWidget(standing_);
    prose->addWidget(ask_);
    prose->addWidget(hint_);
    prose->addLayout(buttons);
    column->addWidget(turnedOn_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateWhatHappenedSoFar()
{
    aside_ = new QWidget(this);
    aside_->setFixedWidth(kHistoryWidth);

    soFar_ = Quiet(aside_);
    story_ = new QWidget(aside_);

    auto* entries = new QVBoxLayout(story_);
    entries->setContentsMargins(0, 0, 0, 0);
    entries->setSpacing(kBetweenEntries);

    scrolled_ = new QScrollArea(aside_);
    scrolled_->setObjectName(QStringLiteral("BisectionStory"));
    scrolled_->setWidget(story_);
    scrolled_->setWidgetResizable(true);
    scrolled_->setFrameShape(QFrame::NoFrame);
    scrolled_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrolled_->setFocusPolicy(Qt::NoFocus);
    story_->setAutoFillBackground(false);
    scrolled_->viewport()->setAutoFillBackground(false);

    auto* beside = new QVBoxLayout(aside_);
    beside->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    beside->setSpacing(8);
    beside->addWidget(soFar_);
    beside->addSpacing(kUnderTheTitle);
    beside->addWidget(scrolled_, 1);

    return aside_;
}

QWidget* BisectionPanel::CreateTheDrift()
{
    auto* pane = new QWidget(this);

    drifted_ = Loud(pane);
    whatStartingOverCosts_ = Loud(pane);
    notInTheJournal_ = Quiet(pane);
    divergences_ = UnitTable(QStringLiteral("BisectionDrift"), pane);
    startOver_ = new QPushButton(pane);
    startOver_->setObjectName(QStringLiteral("PrimaryButton"));
    giveUp_ = new QPushButton(pane);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    buttons->addWidget(startOver_);
    buttons->addWidget(giveUp_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    QVBoxLayout* prose = TheProseAtTheTopOf(column);
    prose->addWidget(drifted_);
    prose->addWidget(whatStartingOverCosts_);
    prose->addWidget(notInTheJournal_);
    prose->addLayout(buttons);
    column->addWidget(divergences_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateWhatJoinedTheLibrary()
{
    auto* pane = new QWidget(this);

    joined_ = Loud(pane);
    notInTheJournalEither_ = Quiet(pane);
    whatJoined_ = UnitTable(QStringLiteral("BisectionJoined"), pane);
    whatJoined_->setColumnHidden(0, true);
    whatJoined_->setColumnHidden(1, true);
    carryOn_ = new QPushButton(pane);
    carryOn_->setObjectName(QStringLiteral("PrimaryButton"));
    giveUpInstead_ = new QPushButton(pane);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    buttons->addWidget(carryOn_);
    buttons->addWidget(giveUpInstead_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    QVBoxLayout* prose = TheProseAtTheTopOf(column);
    prose->addWidget(joined_);
    prose->addWidget(notInTheJournalEither_);
    prose->addLayout(buttons);
    column->addWidget(whatJoined_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateTheOutcome()
{
    auto* pane = new QWidget(this);

    outcome_ = Loud(pane);
    aboutTheSecondPass_ = Quiet(pane);
    singleCulprit_ = Quiet(pane);
    whatIsLeft_ = ThatOpensIntoItsMembers(UnitTable(QStringLiteral("BisectionWhatIsLeft"), pane));
    refine_ = new QPushButton(pane);
    refine_->setObjectName(QStringLiteral("PrimaryButton"));
    bringThemIn_ = new QPushButton(pane);
    finish_ = new QPushButton(pane);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    buttons->addWidget(refine_);
    buttons->addWidget(bringThemIn_);
    buttons->addWidget(finish_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    QVBoxLayout* prose = TheProseAtTheTopOf(column);
    prose->addWidget(outcome_);
    prose->addWidget(aboutTheSecondPass_);
    prose->addWidget(singleCulprit_);
    prose->addLayout(buttons);
    column->addWidget(whatIsLeft_, 1);

    return pane;
}

void BisectionPanel::RetranslateUi()
{
    headline_->setText(tr("Find the addon that crashes the simulator"));
    start_->setText(tr("Start the search"));
    crashed_->setText(tr("It crashed"));
    ranFine_->setText(tr("It worked"));
    stop_->setText(tr("Stop and restore the setup"));
    startOver_->setText(tr("Start over with the current setup"));
    giveUp_->setText(tr("Stop and restore the setup"));
    carryOn_->setText(tr("Continue the search"));
    giveUpInstead_->setText(tr("Stop and restore the setup"));
    soFar_->setText(tr("Rounds so far"));
    refine_->setText(tr("Split this group"));
    bringThemIn_->setText(tr("Import them into the library"));
    finish_->setText(tr("Restore the setup and finish"));
    notInTheJournal_->setText(tr("This change was not made by FS Organizer."));
    notInTheJournalEither_->setText(notInTheJournal_->text());
    singleCulprit_->setText(tr("The search assumes a single culprit. If the crash only happens with two addons enabled "
                               "together, the result may point to the wrong one."));
    promise_->setText(tr("The first round disables all of them, to tell whether the cause is among your addons at all. "
                         "Your setup is saved first and restored when the search ends, however it ends."));

    const QStringList unitColumns{tr("Addon"), tr("Addons"), tr("Why they are grouped")};

    toBeSearched_->setHeaderLabels(unitColumns);
    turnedOn_->setHeaderLabels(unitColumns);
    whatIsLeft_->setHeaderLabels(unitColumns);

    for (QTreeWidget* tree : {toBeSearched_, turnedOn_, whatIsLeft_})
    {
        tree->headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    }

    const QStringList driftColumns{tr("What changed"), QString(), tr("Where")};

    divergences_->setHeaderLabels(driftColumns);
    whatJoined_->setHeaderLabels(driftColumns);

    ShowWhereItStands();
}

void BisectionPanel::ShowWhereItStands()
{
    ListWhatHappenedSoFar();
    aside_->setVisible(!viewModel_.Report().story.empty());

    switch (viewModel_.Stage())
    {
    case BisectionStage::ItDrifted:
        body_->setCurrentIndex(ItDrifted);
        ShowWhatMoved();

        return;
    case BisectionStage::TheLibraryGainedAnAddon:
        body_->setCurrentIndex(TheLibraryGainedAnAddon);
        ShowWhatJoinedTheLibrary();

        return;
    case BisectionStage::Finished:
        body_->setCurrentIndex(Finished);
        ShowTheOutcome();

        return;
    case BisectionStage::Asking:
        body_->setCurrentIndex(Asking);
        ShowTheRound();

        return;
    case BisectionStage::NotStarted: break;
    }

    body_->setCurrentIndex(NotStarted);
    ShowWhatWillBeSearched();
}

void BisectionPanel::ShowWhatWillBeSearched() const
{
    const BisectionReport& report = viewModel_.Report();

    if (viewModel_.ReadingWhatIsOn())
    {
        announced_->setText(tr("Reading the enabled addons and how they group…"));
        outOfReach_->clear();
        toBeSearched_->clear();
        start_->setEnabled(false);

        return;
    }

    if (report.refusal == BisectionRefusal::NothingIsEnabledToSearch)
    {
        announced_->setText(tr("No addon of this profile is enabled, so there is nothing to search. Enable the addons "
                               "you fly with and try again."));
        outOfReach_->clear();
        toBeSearched_->clear();
        start_->setEnabled(false);

        return;
    }

    start_->setEnabled(true);
    announced_->setText(
        tr("%n unit to search, in about %1 rounds. A unit is one addon, or a group of addons that must stay together.",
           nullptr, static_cast<int>(report.units))
            .arg(report.roundsInTheWorstCase));

    outOfReach_->setText(tr("%n destination entry is outside this search and stays enabled in every round.", nullptr,
                            static_cast<int>(report.outOfReach)));

    ListTheUnitsOf(toBeSearched_, viewModel_.WhatIsLeft());
}

void BisectionPanel::ShowTheRound() const
{
    const BisectionReport& report = viewModel_.Report();

    standing_->setText(
        tr("Round %1 · at most %n more", nullptr, static_cast<int>(viewModel_.RoundsLeftInTheWorstCase()))
            .arg(report.round));

    const std::vector<UnitOnScreen> on = viewModel_.WhatToTurnOn();

    if (report.round == 0)
    {
        ask_->setText(tr("All your addons are disabled. Launch the simulator now: this round tells whether the cause "
                         "is among your addons at all."));
    }
    else
    {
        ask_->setText(tr("%n addon is enabled now. Launch the simulator, then come back and say what happened.",
                         nullptr, static_cast<int>(report.addonsTurnedOn.size())));
    }

    hint_->setText(tr("Nothing changes until you answer. %n unit is still a suspect.", nullptr,
                      static_cast<int>(report.unitsUnderSuspicion.size())));

    ListTheUnitsOf(turnedOn_, on);
}

void BisectionPanel::ListWhatHappenedSoFar() const
{
    auto* entries = qobject_cast<QVBoxLayout*>(story_->layout());
    const bool wasAtTheEnd = ItIsShowingTheEndOfTheStory();

    while (QLayoutItem* old = entries->takeAt(0))
    {
        delete old->widget();
        delete old;
    }

    for (const AnsweredRound& answered : viewModel_.Report().story)
    {
        auto* head = Stressed(story_);
        head->setText(tr("%1 at %2").arg(TheNameOf(answered), AtWhatTime(answered.at)));

        auto* said = Quiet(story_);
        said->setText(WhatItSettled(answered));

        entries->addWidget(head);
        entries->addWidget(said);
    }

    entries->addStretch();

    if (wasAtTheEnd)
    {
        KeepShowingTheEndOfTheStory();
    }
}

bool BisectionPanel::ItIsShowingTheEndOfTheStory() const
{
    const QScrollBar* bar = scrolled_->verticalScrollBar();

    return bar->maximum() == 0 || bar->value() == bar->maximum();
}

void BisectionPanel::KeepShowingTheEndOfTheStory() const
{
    story_->adjustSize();

    QScrollBar* bar = scrolled_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void BisectionPanel::ShowWhatMoved() const
{
    drifted_->setText(
        tr("Your addons changed on the disk between rounds, so the search no longer matches what is installed."));

    whatStartingOverCosts_->setText(tr("Starting over discards the %n round you have already run, including the "
                                       "reference round, and searches every unit again.",
                                       nullptr, static_cast<int>(viewModel_.LaunchesAlreadyMade())));

    ListTheDriftIn(divergences_);
}

void BisectionPanel::ShowWhatJoinedTheLibrary() const
{
    joined_->setText(
        tr("%n addon was added to the library during the search. No round loaded it, so it stays out of this search.",
           nullptr, static_cast<int>(viewModel_.Report().drift.size())));

    ListTheDriftIn(whatJoined_);
}

void BisectionPanel::ListTheDriftIn(QTreeWidget* tree) const
{
    tree->clear();

    for (const Divergence& divergence : viewModel_.Report().drift)
    {
        auto* row = new QTreeWidgetItem(tree);
        row->setText(0, WhatMoved(divergence.kind));
        row->setText(2, AsText(divergence.path));
        row->setData(2, QuietRole, true);
    }
}

void BisectionPanel::ShowTheOutcome() const
{
    const BisectionReport& report = viewModel_.Report();

    if (report.outcome == BisectionOutcome::NotAmongTheManagedOnes)
    {
        outcome_->setText(tr("The simulator crashed with all your addons disabled. The cause is not among the addons "
                             "this app manages."));
        aboutTheSecondPass_->setText(tr("%n entry was outside this search. Import it into the library to include it.",
                                        nullptr, static_cast<int>(report.outOfReach)));
    }
    else if (report.outcome == BisectionOutcome::OneAddonLeft)
    {
        outcome_->setText(tr("The search points to this addon."));
        aboutTheSecondPass_->clear();
    }
    else
    {
        outcome_->setText(tr("The search could not narrow it down beyond this group."));
        aboutTheSecondPass_->setText(
            report.aSecondPassIsPossible
                ? tr("Splitting the group takes more rounds than estimated at the start, which counted units, not the "
                     "addons inside them.")
                : tr("This group cannot be split: it has no base aircraft that the others extend."));
    }

    refine_->setVisible(report.aSecondPassIsPossible);
    bringThemIn_->setVisible(report.outcome == BisectionOutcome::NotAmongTheManagedOnes);
    aboutTheSecondPass_->setVisible(!aboutTheSecondPass_->text().isEmpty());

    ListTheUnitsOf(whatIsLeft_, viewModel_.WhatIsLeft());
}

void BisectionPanel::ListTheUnitsOf(QTreeWidget* tree, const std::vector<UnitOnScreen>& units) const
{
    tree->clear();

    for (const UnitOnScreen& unit : units)
    {
        auto* row = new QTreeWidgetItem(tree);
        row->setText(0, unit.name);
        row->setText(1, unit.addons > 1 ? QString::number(unit.addons) : QString());
        row->setText(2, HowTheyAreCoupled(unit));
        row->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        row->setData(1, QuietRole, true);
        row->setData(2, QuietRole, true);

        for (const MemberOnScreen& member : unit.members)
        {
            auto* inside = new QTreeWidgetItem(row);
            inside->setText(0, member.name);
            inside->setText(2, HowItIsHeld(member));
            inside->setData(0, QuietRole, true);
            inside->setData(2, QuietRole, true);
        }
    }
}
