#include "application/BisectionService.h"

#include <algorithm>

#include "domain/linking/EntryClassifier.h"
#include "domain/model/Manifest.h"
#include "domain/preset/PresetPlan.h"
#include "domain/tree/AddonTree.h"

namespace
{
    [[nodiscard]] std::vector<std::filesystem::path> AddonFoldersOf(const std::vector<TreeNode>& libraries)
    {
        std::vector<std::filesystem::path> folders;

        for (const TreeNode& library : libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                folders.push_back(addon->path);
            }
        }

        return folders;
    }

    [[nodiscard]] bool ItIsAmong(const std::vector<std::filesystem::path>& where, const std::filesystem::path& what)
    {
        return std::ranges::any_of(where,
                                   [&what](const std::filesystem::path& one)
                                   {
                                       return ComparablePath(one) == ComparablePath(what);
                                   });
    }
}

BisectionService::BisectionService(ProfileService& profiles,
                                   const CouplingScan& coupling,
                                   const FilesystemProbe& filesystemProbe,
                                   BisectionStore& store)
    : profiles_(profiles), coupling_(coupling), filesystemProbe_(filesystemProbe), store_(store)
{
}

BisectionReport BisectionService::WhatWouldBeSearched(const SimulatorProfile& profile,
                                                      const ProfileSnapshot& shown) const
{
    if (EnabledAddonFolders(shown.entries).empty())
    {
        return BisectionReport{.refusal = BisectionRefusal::NothingIsEnabledToSearch};
    }

    const BisectionRun run = RunFor(profile, shown);

    BisectionReport report = TellAbout(run, ReadingOf(shown));
    report.unitsUnderSuspicion = run.units;

    return report;
}

BisectionReport BisectionService::WhatWouldBeSearchedNow(const SimulatorProfile& profile) const
{
    return WhatWouldBeSearched(profile, ReadTheDisk(profile).snapshot);
}

BisectionReport BisectionService::WhereItStands(const SimulatorProfile& profile) const
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    return TellAbout(*run, ReadTheDisk(profile));
}

BisectionReport BisectionService::Begin(const SimulatorProfile& profile, const ProfileSnapshot& shown)
{
    if (EnabledAddonFolders(shown.entries).empty())
    {
        return BisectionReport{.refusal = BisectionRefusal::NothingIsEnabledToSearch};
    }

    const BisectionRun run = RunFor(profile, shown);
    const Reading reading = ReadTheDisk(profile);

    return TakeTheNextRound(profile, run, run, reading);
}

BisectionReport BisectionService::Answer(const SimulatorProfile& profile, const BisectionAnswer answer)
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    const Reading reading = ReadTheDisk(profile);
    const std::vector<Divergence> drift = WhatMovedSince(reading.disk);

    if (!drift.empty())
    {
        BisectionReport refused = Refusing(*run, reading, BisectionRefusal::TheDiskMovedSinceTheLastRound);
        refused.drift = drift;

        return refused;
    }

    if (OutcomeOf(*run) != BisectionOutcome::StillSearching)
    {
        return TellAbout(*run, reading);
    }

    return TakeTheNextRound(profile, *run, AfterAnswering(*run, answer), reading);
}

BisectionReport BisectionService::Refine(const SimulatorProfile& profile)
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    const Reading reading = ReadTheDisk(profile);

    if (!ASecondPassIsPossible(*run))
    {
        return Refusing(*run, reading, BisectionRefusal::ThisUnitDoesNotSplit);
    }

    const std::vector<Divergence> drift = WhatMovedSince(reading.disk);

    if (!drift.empty())
    {
        BisectionReport refused = Refusing(*run, reading, BisectionRefusal::TheDiskMovedSinceTheLastRound);
        refused.drift = drift;

        return refused;
    }

    return TakeTheNextRound(profile, *run, IntoTheSecondPass(*run), reading);
}

BisectionReport BisectionService::Stop(const SimulatorProfile& profile)
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    BisectionReport report;
    report.results = PutBack(profile, run->startingConfiguration);

    store_.Forget(profile.id);
    leftBehind_ = {};
    weLeftARound_ = false;

    return report;
}

std::optional<BisectionRun> BisectionService::WhatWasInterrupted(const std::string& profileId) const
{
    return store_.Load(profileId);
}

BisectionReport BisectionService::Resume(const SimulatorProfile& profile, const ResumeChoice choice)
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    if (choice == ResumeChoice::ForgetItAndLeaveTheDiskAsItIs)
    {
        store_.Forget(profile.id);
        leftBehind_ = {};
        weLeftARound_ = false;

        return BisectionReport{};
    }

    if (choice == ResumeChoice::PutBackTheStartingConfiguration)
    {
        return Stop(profile);
    }

    return ApplyTheRound(profile, *run, ReadTheDisk(profile));
}

BisectionService::Reading BisectionService::ReadingOf(ProfileSnapshot snapshot)
{
    Reading reading;
    reading.disk.entries = snapshot.entries;
    reading.disk.libraryAddons = AddonFoldersOf(snapshot.libraries);
    reading.snapshot = std::move(snapshot);

    return reading;
}

BisectionService::Reading BisectionService::ReadTheDisk(const SimulatorProfile& profile) const
{
    return ReadingOf(profiles_.Scan(profile));
}

BisectionRun BisectionService::RunFor(const SimulatorProfile& profile, const ProfileSnapshot& shown) const
{
    const std::vector<CouplingFacts> facts = coupling_.FactsAbout(EnabledAddonFolders(shown.entries));

    BisectionRun run;
    run.profileId = profile.id;
    run.units = coupling_.WithTheKindOfEachGroup(facts, UnitsFrom(facts));
    run.startingConfiguration = EntriesForWhatIsEnabled(profile, shown.libraries, shown.enabled);

    return run;
}

std::size_t BisectionService::WhatCarriesOnOutOfReach(const std::vector<DestinationEntry>& entries) const
{
    std::size_t counted = 0;

    for (const DestinationEntry& entry : entries)
    {
        if (CountsAsEnabled(entry.classification))
        {
            continue;
        }

        if (filesystemProbe_.EntryExistsWithoutFollowingLinks(ManifestPathIn(entry.path)))
        {
            ++counted;
        }
    }

    return counted;
}

BisectionReport
BisectionService::ApplyTheRound(const SimulatorProfile& profile, const BisectionRun& run, const Reading& reading)
{
    const BisectionRound round = TheRound(run);

    LinkBatch batch;

    for (const std::filesystem::path& addon : TheSearchSpaceOf(run))
    {
        const TreeNode* node = AddonAt(reading.snapshot.libraries, addon);

        if (node == nullptr)
        {
            continue;
        }

        if (ItIsAmong(round.addonsOn, addon))
        {
            batch.toEnable.push_back(node);

            continue;
        }

        batch.toDisable.push_back(node);
    }

    const LinkBatchReport applied = profiles_.SetEnabled(profile, reading.snapshot, batch);

    BisectionReport report = TellAbout(run, reading);
    report.results = applied.results;
    report.addonsTurnedOn = round.addonsOn;

    AdoptAsTheBaseline(ReadTheDisk(profile).disk);

    return report;
}

BisectionReport BisectionService::TellAbout(const BisectionRun& run, const Reading& reading) const
{
    BisectionReport report;
    report.outcome = OutcomeOf(run);
    report.whatIsLeft = WhatIsLeft(run);
    report.round = run.round;
    report.units = run.units.size();
    report.roundsInTheWorstCase = RoundsInTheWorstCase(run.units.size());
    report.outOfReach = WhatCarriesOnOutOfReach(reading.disk.entries);
    report.aSecondPassIsPossible = ASecondPassIsPossible(run);

    for (const std::size_t suspect : run.suspects)
    {
        report.unitsUnderSuspicion.push_back(run.units[suspect]);
    }

    for (const std::size_t on : TheRound(run).unitsOn)
    {
        report.unitsTurnedOn.push_back(run.units[on]);
    }

    return report;
}

std::vector<std::filesystem::path> BisectionService::TheSearchSpaceOf(const BisectionRun& run)
{
    std::vector<std::filesystem::path> space = run.alwaysOn;

    for (const SearchUnit& unit : run.units)
    {
        for (const std::filesystem::path& addon : unit.addons)
        {
            space.push_back(addon);
        }
    }

    return space;
}

BisectionReport
BisectionService::Refusing(const BisectionRun& run, const Reading& reading, const BisectionRefusal refusal) const
{
    BisectionReport refused = TellAbout(run, reading);
    refused.refusal = refusal;

    return refused;
}

BisectionReport BisectionService::TakeTheNextRound(const SimulatorProfile& profile,
                                                   const BisectionRun& run,
                                                   const BisectionRun& next,
                                                   const Reading& reading)
{
    if (!store_.Save(profile.id, next))
    {
        return Refusing(run, reading, BisectionRefusal::TheStateCouldNotBeWritten);
    }

    if (OutcomeOf(next) != BisectionOutcome::StillSearching)
    {
        return TellAbout(next, reading);
    }

    return ApplyTheRound(profile, next, reading);
}

std::vector<Divergence> BisectionService::WhatMovedSince(const DiskAsItWas& now) const
{
    if (!weLeftARound_)
    {
        return {};
    }

    return DriftBetween(leftBehind_, now);
}

void BisectionService::AdoptAsTheBaseline(const DiskAsItWas& disk)
{
    leftBehind_ = disk;
    weLeftARound_ = true;
}

std::vector<LinkOperationResult> BisectionService::PutBack(const SimulatorProfile& profile,
                                                           const std::vector<PresetEntry>& configuration)
{
    const Reading reading = ReadTheDisk(profile);
    const Preset asAPreset{.entries = configuration};
    const PresetPlan plan = PlanPresetApplication(asAPreset, ApplyMode::Replace, profile, reading.snapshot.libraries,
                                                  reading.snapshot.enabled);

    return profiles_
        .SetEnabled(profile, reading.snapshot, LinkBatch{.toDisable = plan.toDisable, .toEnable = plan.toEnable})
        .results;
}
