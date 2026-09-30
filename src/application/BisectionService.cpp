#include "application/BisectionService.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

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

    [[nodiscard]] std::map<std::string, const TreeNode*> AddonsByComparablePath(const std::vector<TreeNode>& libraries)
    {
        std::map<std::string, const TreeNode*> byPath;

        for (const TreeNode& library : libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                byPath.emplace(ComparablePath(addon->path), addon);
            }
        }

        return byPath;
    }

    [[nodiscard]] std::set<std::string> ComparablePathsOf(const std::vector<std::filesystem::path>& paths)
    {
        std::set<std::string> comparable;

        for (const std::filesystem::path& path : paths)
        {
            comparable.insert(ComparablePath(path));
        }

        return comparable;
    }
}

BisectionService::BisectionService(ProfileService& profiles,
                                   const CouplingScan& coupling,
                                   const FilesystemProbe& filesystemProbe,
                                   BisectionStore& store,
                                   const Clock& clock)
    : profiles_(profiles), coupling_(coupling), filesystemProbe_(filesystemProbe), store_(store), clock_(clock)
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

    return BeginFrom(profile, shown, ReadTheDisk(profile));
}

BisectionReport
BisectionService::BeginFrom(const SimulatorProfile& profile, const ProfileSnapshot& shown, const Reading& reading)
{
    const BisectionRun run = RunFor(profile, shown);

    return TakeTheNextRound(profile, run, run, reading);
}

BisectionReport BisectionService::StartOver(const SimulatorProfile& profile)
{
    BisectionReport putBack = Stop(profile);

    const Reading reading = ReadTheDisk(profile);
    BisectionReport started = BeginFrom(profile, reading.snapshot, reading);

    putBack.results.insert(putBack.results.end(), started.results.begin(), started.results.end());
    started.results = std::move(putBack.results);

    return started;
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
        return RefusingTheDriftOf(*run, reading, drift);
    }

    if (OutcomeOf(*run) != BisectionOutcome::StillSearching)
    {
        return TellAbout(*run, reading);
    }

    return TakeTheNextRound(profile, *run, AfterAnswering(*run, answer, clock_.Now()), reading);
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
        return RefusingTheDriftOf(*run, reading, drift);
    }

    return TakeTheNextRound(profile, *run, IntoTheSecondPass(*run), reading);
}

BisectionReport BisectionService::AcceptWhatJoinedTheLibrary(const SimulatorProfile& profile)
{
    const std::optional<BisectionRun> run = store_.Load(profile.id);

    if (!run.has_value())
    {
        return BisectionReport{.refusal = BisectionRefusal::NoProcedureIsRunning};
    }

    const Reading reading = ReadTheDisk(profile);
    const std::vector<Divergence> drift = WhatMovedSince(reading.disk);

    if (drift.empty())
    {
        return TellAbout(*run, reading);
    }

    if (!NothingThatLoadedMoved(drift))
    {
        return RefusingTheDriftOf(*run, reading, drift);
    }

    AdoptAsTheBaseline(reading.disk);

    return TellAbout(*run, reading);
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
    run.units = coupling_.WithTheKindOfEachGroup(UnitsFrom(facts));
    run.startingConfiguration = EntriesForWhatIsEnabled(profile, shown.libraries, shown.enabled);
    run.startedAt = clock_.Now();

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

    const std::map<std::string, const TreeNode*> inTheLibraries = AddonsByComparablePath(reading.snapshot.libraries);
    const std::set<std::string> turnedOn = ComparablePathsOf(round.addonsOn);

    LinkBatch batch;

    for (const std::filesystem::path& addon : TheSearchSpaceOf(run))
    {
        const std::string key = ComparablePath(addon);
        const auto found = inTheLibraries.find(key);

        if (found == inTheLibraries.end())
        {
            continue;
        }

        if (turnedOn.contains(key))
        {
            batch.toEnable.push_back(found->second);

            continue;
        }

        batch.toDisable.push_back(found->second);
    }

    const LinkBatchReport applied = profiles_.SetEnabled(profile, reading.snapshot, batch);

    BisectionReport report = TellAbout(run, reading);
    report.results = applied.results;

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
    report.story = run.story;
    report.launchesBehind = LaunchesBehind(run);

    for (const std::size_t suspect : run.suspects)
    {
        report.unitsUnderSuspicion.push_back(run.units[suspect]);
    }

    const BisectionRound round = TheRound(run);

    for (const std::size_t on : round.unitsOn)
    {
        report.unitsTurnedOn.push_back(run.units[on]);
    }

    report.addonsTurnedOn = round.addonsOn;

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

BisectionReport BisectionService::RefusingTheDriftOf(const BisectionRun& run,
                                                     const Reading& reading,
                                                     const std::vector<Divergence>& drift) const
{
    const BisectionRefusal refusal = NothingThatLoadedMoved(drift) ? BisectionRefusal::TheLibraryGainedAnAddon
                                                                   : BisectionRefusal::TheDiskMovedSinceTheLastRound;

    BisectionReport refused = Refusing(run, reading, refusal);
    refused.drift = drift;

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
