#include "domain/bisection/BisectionRounds.h"

#include <algorithm>

#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] bool TheReferenceRoundIsStillPending(const BisectionRun& run)
    {
        return run.round == 0;
    }

    [[nodiscard]] std::vector<std::size_t> TheHalfToTurnOn(const std::vector<std::size_t>& suspects)
    {
        return {suspects.begin(), suspects.begin() + static_cast<std::ptrdiff_t>(suspects.size() / 2)};
    }

    [[nodiscard]] std::vector<std::size_t> TheOnesLeftOut(const std::vector<std::size_t>& suspects,
                                                          const std::vector<std::size_t>& turnedOn)
    {
        std::vector<std::size_t> left;

        for (const std::size_t suspect : suspects)
        {
            if (std::ranges::find(turnedOn, suspect) == turnedOn.end())
            {
                left.push_back(suspect);
            }
        }

        return left;
    }

    [[nodiscard]] BisectionRun AfterTheReferenceRound(const BisectionRun& run, const BisectionAnswer answer)
    {
        BisectionRun next = run;
        next.theReferenceRoundCrashed = answer == BisectionAnswer::ItCrashed;

        if (next.theReferenceRoundCrashed)
        {
            return next;
        }

        next.suspects.resize(run.units.size());

        for (std::size_t unit = 0; unit < run.units.size(); ++unit)
        {
            next.suspects[unit] = unit;
        }

        next.round = 1;

        return next;
    }

    [[nodiscard]] const SearchUnit* TheOnlySuspectOf(const BisectionRun& run)
    {
        if (run.suspects.size() != 1 || run.suspects.front() >= run.units.size())
        {
            return nullptr;
        }

        return &run.units[run.suspects.front()];
    }
}

BisectionRun RunOver(const std::vector<SearchUnit>& units)
{
    BisectionRun run;
    run.units = units;

    return run;
}

BisectionRound TheRound(const BisectionRun& run)
{
    BisectionRound round;
    round.number = run.round;

    if (TheReferenceRoundIsStillPending(run))
    {
        return round;
    }

    round.unitsOn = TheHalfToTurnOn(run.suspects);
    round.addonsOn = run.alwaysOn;

    for (const std::size_t unit : round.unitsOn)
    {
        for (const std::filesystem::path& addon : run.units[unit].addons)
        {
            round.addonsOn.push_back(addon);
        }
    }

    return round;
}

BisectionRun AfterAnswering(const BisectionRun& run, const BisectionAnswer answer)
{
    if (OutcomeOf(run) != BisectionOutcome::StillSearching)
    {
        return run;
    }

    if (TheReferenceRoundIsStillPending(run))
    {
        return AfterTheReferenceRound(run, answer);
    }

    BisectionRun next = run;
    const std::vector<std::size_t> turnedOn = TheHalfToTurnOn(run.suspects);
    const std::vector<std::size_t> leftOut = TheOnesLeftOut(run.suspects, turnedOn);

    if (answer == BisectionAnswer::ItCrashed)
    {
        next.suspects = turnedOn;
        next.cleared.insert(next.cleared.end(), leftOut.begin(), leftOut.end());
    }
    else
    {
        next.suspects = leftOut;
        next.cleared.insert(next.cleared.end(), turnedOn.begin(), turnedOn.end());
    }

    ++next.round;

    return next;
}

BisectionOutcome OutcomeOf(const BisectionRun& run)
{
    if (run.theReferenceRoundCrashed)
    {
        return BisectionOutcome::NotAmongTheManagedOnes;
    }

    const SearchUnit* only = TheOnlySuspectOf(run);

    if (TheReferenceRoundIsStillPending(run) || only == nullptr)
    {
        return BisectionOutcome::StillSearching;
    }

    if (only->addons.size() == 1)
    {
        return BisectionOutcome::OneAddonLeft;
    }

    return BisectionOutcome::AnIrreducibleSet;
}

std::vector<std::filesystem::path> WhatIsLeft(const BisectionRun& run)
{
    if (run.theReferenceRoundCrashed)
    {
        return {};
    }

    const SearchUnit* only = TheOnlySuspectOf(run);

    if (only == nullptr)
    {
        return {};
    }

    return only->addons;
}

std::size_t RoundsInTheWorstCase(const std::size_t units)
{
    std::size_t rounds = 0;
    std::size_t reached = 1;

    while (reached < units)
    {
        reached *= 2;
        ++rounds;
    }

    return rounds;
}

bool ASecondPassIsPossible(const BisectionRun& run)
{
    if (run.pass != BisectionPass::OverTheUnits || OutcomeOf(run) != BisectionOutcome::AnIrreducibleSet)
    {
        return false;
    }

    return TheOnlySuspectOf(run)->base.has_value();
}

BisectionRun IntoTheSecondPass(const BisectionRun& run)
{
    if (!ASecondPassIsPossible(run))
    {
        return run;
    }

    const SearchUnit& group = *TheOnlySuspectOf(run);
    const std::filesystem::path base = *group.base;

    BisectionRun second;
    second.profileId = run.profileId;
    second.alwaysOn = {base};
    second.pass = BisectionPass::InsideTheGroup;
    second.round = run.round;
    second.startingConfiguration = run.startingConfiguration;
    second.startedAt = run.startedAt;

    for (const std::filesystem::path& member : group.addons)
    {
        if (ComparablePath(member) == ComparablePath(base))
        {
            continue;
        }

        second.suspects.push_back(second.units.size());
        second.units.push_back(SearchUnit{.addons = {member}});
    }

    return second;
}
