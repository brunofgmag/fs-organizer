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
    : QObject(parent), bisection_(bisection), session_(session), runner_(runner)
{
}

void BisectionViewModel::Show()
{
    if (reading_)
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

    reading_ = true;

    emit Changed();

    runner_.Run(
        [this, profile, snapshot, found]
        {
            *found = bisection_.WhatWasInterrupted(profile.id).has_value()
                ? bisection_.WhereItStands(profile)
                : bisection_.WhatWouldBeSearched(profile, snapshot);
        },
        [this, found, read = std::move(enabled)]
        {
            reading_ = false;
            readFor_ = read;

            Take(*found);
        });
}

void BisectionViewModel::Begin()
{
    Take(bisection_.Begin(session_.Profile(), session_.Snapshot()));
}

void BisectionViewModel::Answer(const BisectionAnswer answer)
{
    heldAnswer_ = answer;
    aSplitWasHeld_ = false;

    Take(bisection_.Answer(session_.Profile(), answer));
}

void BisectionViewModel::Refine()
{
    heldAnswer_.reset();
    aSplitWasHeld_ = true;

    Take(bisection_.Refine(session_.Profile()));
}

void BisectionViewModel::CarryOn()
{
    const BisectionReport accepted = bisection_.AcceptWhatJoinedTheLibrary(session_.Profile());

    if (accepted.refusal != BisectionRefusal::None)
    {
        Take(accepted);

        return;
    }

    if (heldAnswer_.has_value())
    {
        Answer(*heldAnswer_);

        return;
    }

    if (aSplitWasHeld_)
    {
        Refine();

        return;
    }

    Take(accepted);
}

void BisectionViewModel::Stop()
{
    TakeTheEndOf(bisection_.Stop(session_.Profile()));
}

void BisectionViewModel::Resume(const ResumeChoice choice)
{
    const BisectionReport answered = bisection_.Resume(session_.Profile(), choice);

    if (choice == ResumeChoice::CarryOnFromWhereItStopped)
    {
        Take(answered);

        return;
    }

    TakeTheEndOf(answered);
}

void BisectionViewModel::TakeTheEndOf(const BisectionReport& ended)
{
    if (ended.refusal != BisectionRefusal::None)
    {
        Take(ended);

        return;
    }

    BisectionReport announced = bisection_.WhatWouldBeSearchedNow(session_.Profile());
    announced.results = ended.results;

    Take(announced);
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

bool BisectionViewModel::ReadingWhatIsOn() const
{
    return reading_;
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
