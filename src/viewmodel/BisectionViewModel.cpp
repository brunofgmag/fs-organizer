#include "viewmodel/BisectionViewModel.h"

#include <memory>
#include <utility>

#include "domain/linking/EntryClassifier.h"
#include "support/PathText.h"

namespace
{
    [[nodiscard]] std::vector<MemberOnScreen> MembersOf(const SearchUnit& unit)
    {
        std::vector<MemberOnScreen> members;

        for (const std::filesystem::path& folder : unit.writingApart)
        {
            members.push_back(MemberOnScreen{.name = AsText(folder.filename()), .writesWith = 0});
        }

        for (const std::vector<std::filesystem::path>& sharing : unit.writingTogether)
        {
            for (const std::filesystem::path& folder : sharing)
            {
                members.push_back(MemberOnScreen{.name = AsText(folder.filename()), .writesWith = sharing.size() - 1});
            }
        }

        return members;
    }

    [[nodiscard]] std::vector<UnitOnScreen> Showing(const std::vector<SearchUnit>& units)
    {
        std::vector<UnitOnScreen> shown;
        shown.reserve(units.size());

        for (const SearchUnit& unit : units)
        {
            shown.push_back(UnitOnScreen{.name = AsText(unit.addons.front().filename()),
                                         .addons = unit.addons.size(),
                                         .coupling = unit.coupling,
                                         .members = MembersOf(unit)});
        }

        return shown;
    }
}

BisectionViewModel::BisectionViewModel(BisectionService& bisection,
                                       Session& session,
                                       BackgroundRunner& runner,
                                       QObject* parent)
    : QObject(parent), bisection_(bisection), session_(session), reading_(runner), mutating_(runner)
{
}

void BisectionViewModel::Show()
{
    if (Working())
    {
        return;
    }

    const ProfileSnapshot snapshot = session_.Snapshot();
    std::vector<std::filesystem::path> enabled = EnabledAddonFolders(snapshot.entries);

    if (stage_ == BisectionStage::NotStarted && readFor_ == enabled)
    {
        emit Changed();

        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const auto found = std::make_shared<BisectionReport>();

    emit Changed();

    reading_.Run(
        [this, profile, snapshot, found]
        {
            *found = bisection_.WhatWasInterrupted(profile.id).has_value()
                ? bisection_.WhereItStands(profile)
                : bisection_.WhatWouldBeSearched(profile, snapshot);
        },
        [this, found, read = std::move(enabled)]
        {
            readFor_ = read;

            Take(*found);
        });
}

void BisectionViewModel::RunTheProcedure(std::function<BisectionReport()> work)
{
    if (Working())
    {
        return;
    }

    const auto found = std::make_shared<BisectionReport>();

    mutating_.Run(
        [work = std::move(work), found]
        {
            *found = work();
        },
        [this, found]
        {
            Take(*found);
        });
}

void BisectionViewModel::Begin()
{
    RunTheProcedure(
        [this, profile = session_.Profile(), snapshot = session_.Snapshot()]
        {
            return bisection_.Begin(profile, snapshot);
        });
}

void BisectionViewModel::Answer(const BisectionAnswer answer)
{
    if (Working())
    {
        return;
    }

    heldAnswer_ = answer;
    aSplitWasHeld_ = false;

    RunTheProcedure(
        [this, profile = session_.Profile(), answer]
        {
            return bisection_.Answer(profile, answer);
        });
}

void BisectionViewModel::Refine()
{
    if (Working())
    {
        return;
    }

    heldAnswer_.reset();
    aSplitWasHeld_ = true;

    RunTheProcedure(
        [this, profile = session_.Profile()]
        {
            return bisection_.Refine(profile);
        });
}

void BisectionViewModel::CarryOn()
{
    RunTheProcedure(
        [this, profile = session_.Profile(), held = heldAnswer_, splitWasHeld = aSplitWasHeld_]
        {
            const BisectionReport accepted = bisection_.AcceptWhatJoinedTheLibrary(profile);

            if (accepted.refusal != BisectionRefusal::None)
            {
                return accepted;
            }

            if (held.has_value())
            {
                return bisection_.Answer(profile, *held);
            }

            if (splitWasHeld)
            {
                return bisection_.Refine(profile);
            }

            return accepted;
        });
}

void BisectionViewModel::Stop()
{
    RunTheProcedure(
        [this, profile = session_.Profile()]
        {
            return EndedReport(bisection_.Stop(profile), profile);
        });
}

void BisectionViewModel::Resume(const ResumeChoice choice)
{
    RunTheProcedure(
        [this, profile = session_.Profile(), choice]
        {
            BisectionReport answered = bisection_.Resume(profile, choice);

            if (choice == ResumeChoice::CarryOnFromWhereItStopped)
            {
                return answered;
            }

            return EndedReport(std::move(answered), profile);
        });
}

BisectionReport BisectionViewModel::EndedReport(BisectionReport ended, const SimulatorProfile& profile) const
{
    if (ended.refusal != BisectionRefusal::None)
    {
        return ended;
    }

    BisectionReport announced = bisection_.WhatWouldBeSearchedNow(profile);
    announced.results = ended.results;

    return announced;
}

void BisectionViewModel::Take(const BisectionReport& report)
{
    report_ = report;

    if (report.refusal == BisectionRefusal::TheLibraryGainedAnAddon)
    {
        stage_ = BisectionStage::TheLibraryGainedAnAddon;
    }
    else if (!report.drift.empty())
    {
        stage_ = BisectionStage::ItDrifted;
    }
    else if (report.outcome != BisectionOutcome::StillSearching)
    {
        stage_ = BisectionStage::Finished;
    }
    else if (AProcedureWasInterrupted())
    {
        stage_ = BisectionStage::Asking;
    }
    else
    {
        stage_ = BisectionStage::NotStarted;
    }

    if (stage_ != BisectionStage::TheLibraryGainedAnAddon)
    {
        heldAnswer_.reset();
        aSplitWasHeld_ = false;
    }

    emit Changed();
}

BisectionStage BisectionViewModel::Stage() const
{
    return stage_;
}

const BisectionReport& BisectionViewModel::Report() const
{
    return report_;
}

bool BisectionViewModel::AProcedureWasInterrupted() const
{
    return bisection_.WhatWasInterrupted(session_.Profile().id).has_value();
}

bool BisectionViewModel::Working() const
{
    return reading_.Busy() || mutating_.Busy();
}

bool BisectionViewModel::ReadingWhatIsOn() const
{
    return reading_.Busy();
}

bool BisectionViewModel::ItIsRunning() const
{
    return stage_ == BisectionStage::Asking || stage_ == BisectionStage::ItDrifted
        || stage_ == BisectionStage::TheLibraryGainedAnAddon;
}

std::size_t BisectionViewModel::LaunchesAlreadyMade() const
{
    return report_.launchesBehind;
}

std::size_t BisectionViewModel::RoundsLeftInTheWorstCase() const
{
    if (report_.roundsInTheWorstCase <= report_.round)
    {
        return 0;
    }

    return report_.roundsInTheWorstCase - report_.round;
}

std::vector<UnitOnScreen> BisectionViewModel::WhatIsLeft() const
{
    return Showing(report_.unitsUnderSuspicion);
}

std::vector<UnitOnScreen> BisectionViewModel::WhatToTurnOn() const
{
    return Showing(report_.unitsTurnedOn);
}
