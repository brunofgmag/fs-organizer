#include "view/diagnostics/BisectionPanel.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    [[nodiscard]] QLabel* Quiet(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("PanelPromise"));
        label->setWordWrap(true);

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

        return tree;
    }

    [[nodiscard]] QVBoxLayout* AColumnInside(QWidget* pane)
    {
        auto* column = new QVBoxLayout(pane);
        column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
        column->setSpacing(8);

        return column;
    }

    [[nodiscard]] QString HowTheyAreCoupled(const Coupling coupling)
    {
        switch (coupling)
        {
        case Coupling::Merge: return QObject::tr("they share a model folder and no file twice");
        case Coupling::Shadowing: return QObject::tr("one of them writes over a file of another");
        case Coupling::OnlyTheSharedModelFolder:
            return QObject::tr("held together only by the shared model folder name");
        case Coupling::Alone:
        case Coupling::NotYetMeasured: break;
        }

        return QString();
    }

    [[nodiscard]] QString WhatMoved(const DriftKind kind)
    {
        switch (kind)
        {
        case DriftKind::ALinkWeLeftIsGone: return QObject::tr("a link this program had put is gone");
        case DriftKind::AnEntryWeDidNotLeaveIsThere: return QObject::tr("an entry this program did not put is there");
        case DriftKind::AnEntryPointsSomewhereElse: return QObject::tr("an entry points somewhere else now");
        case DriftKind::AnAddonLeftTheLibrary: return QObject::tr("an addon left the library");
        case DriftKind::AnAddonJoinedTheLibrary: return QObject::tr("an addon joined the library");
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

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, 0);
    column->setSpacing(8);
    column->addWidget(headline_);
    column->addWidget(body_, 1);

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
    toBeSearched_ = UnitTable(QStringLiteral("BisectionUnits"), pane);
    start_ = new QPushButton(pane);
    start_->setObjectName(QStringLiteral("PrimaryButton"));

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(start_);
    buttons->addStretch();

    QVBoxLayout* column = AColumnInside(pane);
    column->addWidget(announced_);
    column->addWidget(outOfReach_);
    column->addWidget(promise_);
    column->addLayout(buttons);
    column->addWidget(toBeSearched_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateTheRound()
{
    auto* pane = new QWidget(this);

    standing_ = Quiet(pane);
    ask_ = Loud(pane);
    hint_ = Quiet(pane);
    turnedOn_ = UnitTable(QStringLiteral("BisectionTurnedOn"), pane);
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
    column->addWidget(standing_);
    column->addWidget(ask_);
    column->addWidget(hint_);
    column->addLayout(buttons);
    column->addWidget(turnedOn_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateTheDrift()
{
    auto* pane = new QWidget(this);

    drifted_ = Loud(pane);
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
    column->addWidget(drifted_);
    column->addWidget(notInTheJournal_);
    column->addLayout(buttons);
    column->addWidget(divergences_, 1);

    return pane;
}

QWidget* BisectionPanel::CreateTheOutcome()
{
    auto* pane = new QWidget(this);

    outcome_ = Loud(pane);
    aboutTheSecondPass_ = Quiet(pane);
    singleCulprit_ = Quiet(pane);
    whatIsLeft_ = UnitTable(QStringLiteral("BisectionWhatIsLeft"), pane);
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
    column->addWidget(outcome_);
    column->addWidget(aboutTheSecondPass_);
    column->addWidget(singleCulprit_);
    column->addLayout(buttons);
    column->addWidget(whatIsLeft_, 1);

    return pane;
}

void BisectionPanel::RetranslateUi()
{
    headline_->setText(tr("Find the addon that brings the simulator down"));
    start_->setText(tr("Start the search"));
    crashed_->setText(tr("It came down"));
    ranFine_->setText(tr("It ran fine"));
    stop_->setText(tr("Stop and put everything back"));
    startOver_->setText(tr("Start over from what is on the disk now"));
    giveUp_->setText(tr("Stop and put everything back"));
    refine_->setText(tr("Split this group"));
    bringThemIn_->setText(tr("Bring them into the library"));
    finish_->setText(tr("Put everything back and finish"));
    notInTheJournal_->setText(tr("What this program can say is that the change is not in its journal. Who made it, it "
                                 "has no way of knowing."));
    singleCulprit_->setText(tr("This method assumes one culprit. Two addons that only bring the simulator down when "
                               "both are on would converge on an innocent one."));
    promise_->setText(tr("The first round turns every one of them off, which is what separates a cause among your "
                         "addons from one outside them. Your setup is written down before that and goes back when "
                         "this ends, however it ends, including when you stop it halfway."));

    const QStringList unitColumns{tr("Addon"), tr("Addons"), tr("Why they move together")};

    toBeSearched_->setHeaderLabels(unitColumns);
    turnedOn_->setHeaderLabels(unitColumns);
    whatIsLeft_->setHeaderLabels(unitColumns);
    divergences_->setHeaderLabels({tr("What moved"), QString(), tr("Where")});

    ShowWhereItStands();
}

void BisectionPanel::ShowWhereItStands()
{
    switch (viewModel_.Stage())
    {
    case BisectionStage::ItDrifted:
        body_->setCurrentIndex(ItDrifted);
        ShowWhatMoved();

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

    if (report.refusal == BisectionRefusal::NothingIsEnabledToSearch)
    {
        announced_->setText(tr("Nothing of this profile is turned on, so there is nothing to search. Turn the addons "
                               "you fly with back on and open this again."));
        outOfReach_->clear();
        toBeSearched_->clear();
        start_->setEnabled(false);

        return;
    }

    start_->setEnabled(true);
    announced_->setText(tr("%n unit will be searched, and that takes about %1 rounds. A unit is one addon, or a group "
                           "that has to move together.",
                           nullptr, static_cast<int>(report.units))
                            .arg(report.roundsInTheWorstCase));

    outOfReach_->setText(tr("%n entry in the destinations carries on outside the reach of this search, and stays on "
                            "through every round.",
                            nullptr, static_cast<int>(report.outOfReach)));

    ListTheUnitsOf(toBeSearched_, viewModel_.WhatIsLeft());
}

void BisectionPanel::ShowTheRound() const
{
    const BisectionReport& report = viewModel_.Report();

    standing_->setText(
        tr("Round %1, and %n at most left after it.", nullptr, static_cast<int>(viewModel_.RoundsLeftInTheWorstCase()))
            .arg(report.round));

    const std::vector<UnitOnScreen> on = viewModel_.WhatToTurnOn();

    if (report.round == 0)
    {
        ask_->setText(tr("Nothing of yours is on. Launch the simulator now: this first round is what separates a cause "
                         "among your addons from one outside them."));
    }
    else
    {
        ask_->setText(tr("%n addon is on now. Launch the simulator and come back with what happened.", nullptr,
                         static_cast<int>(report.addonsTurnedOn.size())));
    }

    hint_->setText(tr("Nothing else is written until you answer. %n unit is still under suspicion.", nullptr,
                      static_cast<int>(report.unitsUnderSuspicion.size())));

    ListTheUnitsOf(turnedOn_, on);
}

void BisectionPanel::ShowWhatMoved() const
{
    const BisectionReport& report = viewModel_.Report();

    drifted_->setText(tr("The disk moved between one round and the next, so the split this search had made is about "
                         "another set of addons than the one that is there now."));

    divergences_->clear();

    for (const Divergence& divergence : report.drift)
    {
        auto* row = new QTreeWidgetItem(divergences_);
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
        outcome_->setText(tr("With nothing of yours on, the simulator still came down. The cause is not among the "
                             "addons this program manages."));
        aboutTheSecondPass_->setText(tr("%n entry carries on outside the reach of this search. Bringing them into the "
                                        "library is what puts them under it.",
                                        nullptr, static_cast<int>(report.outOfReach)));
    }
    else if (report.outcome == BisectionOutcome::OneAddonLeft)
    {
        outcome_->setText(tr("What the search was left with is this one."));
        aboutTheSecondPass_->clear();
    }
    else
    {
        outcome_->setText(tr("The answers stopped separating, and this is the whole set the search was left with."));
        aboutTheSecondPass_->setText(
            report.aSecondPassIsPossible
                ? tr("Splitting the group runs more rounds than the number announced at the start, which counted "
                     "units and not the addons inside them.")
                : tr("This group has no aircraft that the others extend, so splitting it would leave a state nobody "
                     "knows how to read. It is not offered."));
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
        row->setText(2, HowTheyAreCoupled(unit.coupling));
        row->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        row->setData(1, QuietRole, true);
        row->setData(2, QuietRole, true);
    }
}
