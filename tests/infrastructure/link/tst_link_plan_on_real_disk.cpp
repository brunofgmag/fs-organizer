#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "domain/journal/JournalEntries.h"
#include "domain/ports/ImportedFolders.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonDestinations.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class LinkPlanOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EnablingAnAddonWhoseJunctionTheUserDeletedCreatesItAgain();
        static void ABatchWithNothingToDoStaysEmptyWhenTheDiskAgreesWithTheScan();
        static void ApplyingAPresetReachesAnAddonWhoseJunctionTheUserDeleted();
        static void SwappingTheOccupantMovesTheRealJunctionAndIsOneEntryInTheJournal();
        static void TheIncrementalEntriesEqualAFullReadAfterARealEnableAndARealDisable();
        static void TheEntriesTheServiceReturnsAfterEachBatchEqualAFullReadOfTheRealDisk();
        static void RelinkingABrokenAddonThatNeverStrayedReplacesTheDeadJunctionAndUndoPutsTheOtherNameBack();
        static void AFullReadClassifiesEveryKindOfEntryOnRealJunctions();
        static void ARepairRemovesTheJunctionWhoseTargetStayedDeletedAndLeavesTheOneWhoseTargetCameBack();
    };
}

namespace
{
    const std::string kManifest = R"({"title": "Fenix A320", "package_version": "1.2.3"})";
    constexpr auto kAddonFolder = "fenix-a320";
    constexpr auto kLibraryId = "library-1";
    constexpr auto kProfileId = "msfs2024";

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path Library() const
        {
            return Root() / "Library";
        }

        [[nodiscard]] std::filesystem::path Community() const
        {
            return Root() / "Community";
        }

        [[nodiscard]] std::filesystem::path Addon() const
        {
            return Library() / "Aircrafts" / kAddonFolder;
        }

        [[nodiscard]] std::filesystem::path Link() const
        {
            return Community() / kAddonFolder;
        }

        Disk()
        {
            std::filesystem::create_directories(Community());
            std::filesystem::create_directories(Addon());
            std::ofstream(ManifestPathIn(Addon()), std::ios::binary) << kManifest;
        }
    };

    struct Linking
    {
        JsonManifestParser manifestParser;
        WindowsFilesystemProbe filesystemProbe;
        WindowsSidecarStore sidecars;
        WindowsLinkService linkService;
        FilesystemScanner scanner{manifestParser, filesystemProbe, nothingWasImported};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{scanner, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        FakePresetRepository presets;
        PresetService service{presets, profiles, startup.service};
    };

    SimulatorProfile ProfileOn(const Disk& disk)
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {disk.Community()};
        profile.defaultDestination = disk.Community();
        profile.libraries = {Library{.id = kLibraryId, .path = disk.Library(), .label = "Library"}};

        return profile;
    }

    const TreeNode* OnlyAddonOf(const ProfileSnapshot& snapshot)
    {
        if (snapshot.libraries.size() != 1 || snapshot.libraries.front().children.size() != 1)
        {
            return nullptr;
        }

        const TreeNode& category = snapshot.libraries.front().children.front();

        return category.children.size() == 1 ? &category.children.front() : nullptr;
    }
}

void LinkPlanOnRealDiskTest::EnablingAnAddonWhoseJunctionTheUserDeletedCreatesItAgain()
{
    const Disk disk;
    Linking linking;
    const SimulatorProfile profile = ProfileOn(disk);

    const ProfileSnapshot first = linking.profiles.Scan(profile);
    const TreeNode* addon = OnlyAddonOf(first);
    QVERIFY(addon != nullptr);
    QCOMPARE(linking.profiles.SetEnabled(profile, first, {addon}, true).results.size(), std::size_t{1});
    QVERIFY(std::filesystem::exists(disk.Link()));

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    QVERIFY(shown.enabled.Contains(disk.Addon()));

    QVERIFY(std::filesystem::remove(disk.Link()));
    QVERIFY(!std::filesystem::exists(disk.Link()));
    QVERIFY(std::filesystem::exists(ManifestPathIn(disk.Addon())));

    linking.journal.appended.clear();
    const LinkBatchReport report = linking.profiles.SetEnabled(profile, shown, {OnlyAddonOf(shown)}, true);

    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(report.results.front().outcome.Succeeded());
    QCOMPARE(report.drifted, std::size_t{1});
    QVERIFY(std::filesystem::exists(disk.Link()));
    QCOMPARE(linking.linkService.ReadLinkTarget(disk.Link()), std::optional<std::filesystem::path>{disk.Addon()});

    QCOMPARE(linking.journal.appended.size(), std::size_t{1});
    QCOMPARE(linking.journal.appended.front().kind, OperationKind::EnableAddon);
    QCOMPARE(linking.journal.appended.front().target, disk.Link());
}

void LinkPlanOnRealDiskTest::ABatchWithNothingToDoStaysEmptyWhenTheDiskAgreesWithTheScan()
{
    const Disk disk;
    Linking linking;
    const SimulatorProfile profile = ProfileOn(disk);

    const ProfileSnapshot first = linking.profiles.Scan(profile);
    QVERIFY(OnlyAddonOf(first) != nullptr);
    QCOMPARE(linking.profiles.SetEnabled(profile, first, {OnlyAddonOf(first)}, true).results.size(), std::size_t{1});

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    linking.journal.appended.clear();

    const LinkBatchReport report = linking.profiles.SetEnabled(profile, shown, {OnlyAddonOf(shown)}, true);

    QVERIFY(report.results.empty());
    QCOMPARE(report.drifted, std::size_t{0});
    QVERIFY(linking.journal.appended.empty());
    QVERIFY(std::filesystem::exists(disk.Link()));
}

void LinkPlanOnRealDiskTest::ApplyingAPresetReachesAnAddonWhoseJunctionTheUserDeleted()
{
    const Disk disk;
    Linking linking;
    const SimulatorProfile profile = ProfileOn(disk);

    const ProfileSnapshot first = linking.profiles.Scan(profile);
    QVERIFY(OnlyAddonOf(first) != nullptr);
    QCOMPARE(linking.profiles.SetEnabled(profile, first, {OnlyAddonOf(first)}, true).results.size(), std::size_t{1});

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    QVERIFY(linking.service.Create(profile, shown, "Voo curto"));

    const std::optional<Preset> preset = linking.service.Load(kProfileId, "Voo curto");
    QVERIFY(preset.has_value());
    QCOMPARE(preset->entries.size(), std::size_t{1});

    QVERIFY(std::filesystem::remove(disk.Link()));

    const PresetApplyReport report =
        linking.service.Apply(EntriesStamp{.profile = profile, .adoptions = 0}, shown, *preset, ApplyMode::Replace)
            .report;

    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(report.results.front().outcome.Succeeded());
    QVERIFY(report.unresolved.empty());
    QVERIFY(std::filesystem::exists(disk.Link()));
    QCOMPARE(linking.linkService.ReadLinkTarget(disk.Link()), std::optional<std::filesystem::path>{disk.Addon()});
}

void LinkPlanOnRealDiskTest::SwappingTheOccupantMovesTheRealJunctionAndIsOneEntryInTheJournal()
{
    const Disk disk;
    Linking linking;

    const std::filesystem::path spare = disk.Root() / "Spare";
    const std::filesystem::path rival = spare / "Aircrafts" / kAddonFolder;
    std::filesystem::create_directories(rival);
    std::ofstream(ManifestPathIn(rival), std::ios::binary) << kManifest;

    SimulatorProfile profile = ProfileOn(disk);
    profile.libraries.push_back(Library{.id = "library-2", .path = spare, .label = "Spare"});

    const ProfileSnapshot first = linking.profiles.Scan(profile);
    QCOMPARE(
        linking.profiles.SetEnabled(profile, first, {&first.libraries.front().children.front().children.front()}, true)
            .results.size(),
        std::size_t{1});
    QCOMPARE(linking.linkService.ReadLinkTarget(disk.Link()), std::optional<std::filesystem::path>{disk.Addon()});

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    const TreeNode* wanted = &shown.libraries.back().children.front().children.front();
    const TreeNode* occupant = &shown.libraries.front().children.front().children.front();

    const std::vector<TakenPlace> taken = linking.profiles.PlacesTaken(profile, {wanted}, shown.libraries);

    QCOMPARE(taken.size(), std::size_t{1});
    QCOMPARE(taken.front().occupant, disk.Addon());
    QCOMPARE(taken.front().linkPath, disk.Link());

    linking.journal.appended.clear();

    const LinkBatchReport report =
        linking.profiles.SetEnabled(profile, shown, LinkBatch{.toDisable = {occupant}, .toEnable = {wanted}});

    QCOMPARE(report.results.size(), std::size_t{2});
    QVERIFY(report.results.front().outcome.Succeeded());
    QVERIFY(report.results.back().outcome.Succeeded());
    QCOMPARE(linking.linkService.ReadLinkTarget(disk.Link()), std::optional<std::filesystem::path>{rival});

    const std::vector<JournalEntry> entries = GroupOperations(linking.journal.appended);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(entries.front().IsASwap());
    QVERIFY(entries.front().Succeeded());
}

void LinkPlanOnRealDiskTest::TheIncrementalEntriesEqualAFullReadAfterARealEnableAndARealDisable()
{
    const Disk disk;
    Linking linking;

    const std::filesystem::path second = disk.Root() / "Community2";
    const std::filesystem::path other = disk.Library() / "Aircrafts" / "other-addon";
    std::filesystem::create_directories(second);
    std::filesystem::create_directories(other);
    std::ofstream(ManifestPathIn(other), std::ios::binary) << kManifest;

    SimulatorProfile profile = ProfileOn(disk);
    profile.destinations = {disk.Community(), second};

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    QVERIFY(shown.libraries.size() == 1 && shown.libraries.front().children.size() == 1);

    const std::vector<TreeNode>& addons = shown.libraries.front().children.front().children;
    QCOMPARE(addons.size(), std::size_t{2});

    const std::vector<ExternalAddon> externals = linking.profiles.WhatCameFromAnotherProgram(profile, shown.libraries);

    const auto verifyTheSame = [&](const std::vector<DestinationEntry>& incremental)
    {
        const std::vector<DestinationEntry> full = linking.profiles.ResolveEntries(profile, shown.libraries);

        QCOMPARE(incremental.size(), full.size());

        for (std::size_t index = 0; index < full.size(); ++index)
        {
            QCOMPARE(incremental[index].path, full[index].path);
            QCOMPARE(incremental[index].target, full[index].target);
            QCOMPARE(incremental[index].classification, full[index].classification);
            QCOMPARE(incremental[index].externalOrigin, full[index].externalOrigin);
            QCOMPARE(incremental[index].libraryCopy, full[index].libraryCopy);
        }
    };

    const ProfileService::LinksOnDisk empty = linking.profiles.ReadLinksNow(profile, externals);
    QVERIFY(empty.entries.empty());

    const LinkBatchReport enabling = linking.profiles.SetEnabled(
        profile, shown, LinkBatch{.toDisable = {}, .toEnable = {&addons[0], &addons[1]}}, empty);
    QCOMPARE(enabling.results.size(), std::size_t{2});

    const std::vector<DestinationEntry> afterEnabling =
        linking.profiles.EntriesAfter(profile, empty.entries, enabling.results, externals);
    QCOMPARE(afterEnabling.size(), std::size_t{2});
    verifyTheSame(afterEnabling);

    QCOMPARE(linking.linkService.CreateLink(second / addons[0].path.filename(), addons[0].path, LinkType::Junction),
             LinkFailure::None);

    const ProfileService::LinksOnDisk duplicated = linking.profiles.ReadLinksNow(profile, externals);
    QCOMPARE(duplicated.entries.size(), std::size_t{3});
    QCOMPARE(std::ranges::count(duplicated.entries, EntryClassification::Duplicated, &DestinationEntry::classification),
             std::ptrdiff_t{2});

    const LinkBatchReport disabling =
        linking.profiles.SetEnabled(profile, shown, LinkBatch{.toDisable = {&addons[1]}, .toEnable = {}}, duplicated);
    QCOMPARE(disabling.results.size(), std::size_t{1});

    const std::vector<DestinationEntry> afterDisabling =
        linking.profiles.EntriesAfter(profile, duplicated.entries, disabling.results, externals);
    QCOMPARE(afterDisabling.size(), std::size_t{2});
    verifyTheSame(afterDisabling);

    QVERIFY(linking.linkService.RemoveReparseNode(second / addons[0].path.filename()));

    const std::vector<DestinationEntry> survivor = linking.classifier.Refresh(
        afterDisabling, {second / addons[0].path.filename()}, profile.destinations, {disk.Library()}, externals);
    QCOMPARE(survivor.size(), std::size_t{1});
    QCOMPARE(survivor.front().classification, EntryClassification::Managed);
    verifyTheSame(survivor);
}

void LinkPlanOnRealDiskTest::TheEntriesTheServiceReturnsAfterEachBatchEqualAFullReadOfTheRealDisk()
{
    const Disk disk;
    Linking linking;

    const std::filesystem::path second = disk.Root() / "Community2";
    const std::filesystem::path other = disk.Library() / "Aircrafts" / "other-addon";
    std::filesystem::create_directories(second);
    std::filesystem::create_directories(other);
    std::ofstream(ManifestPathIn(other), std::ios::binary) << kManifest;

    SimulatorProfile profile = ProfileOn(disk);
    profile.destinations = {disk.Community(), second};

    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    QVERIFY(shown.libraries.size() == 1 && shown.libraries.front().children.size() == 1);

    const std::vector<TreeNode>& addons = shown.libraries.front().children.front().children;
    QCOMPARE(addons.size(), std::size_t{2});

    const EntriesStamp stamp{.profile = profile, .adoptions = 3};

    const auto verifyTheSame = [&](const EntriesRead& read)
    {
        const std::vector<DestinationEntry> full = linking.profiles.ResolveEntries(profile, shown.libraries);

        QCOMPARE(read.stamp.adoptions, 3);
        QCOMPARE(read.entries.size(), full.size());

        for (std::size_t index = 0; index < full.size(); ++index)
        {
            QCOMPARE(read.entries[index].path, full[index].path);
            QCOMPARE(read.entries[index].target, full[index].target);
            QCOMPARE(read.entries[index].classification, full[index].classification);
        }

        for (const DestinationEntry& entry : full)
        {
            QVERIFY(read.enabled.Contains(entry.target));
        }
    };

    const LinkBatchOutcome enabling =
        linking.profiles.SetEnabled(stamp, shown, LinkBatch{.toDisable = {}, .toEnable = {&addons[0], &addons[1]}});
    QCOMPARE(enabling.report.results.size(), std::size_t{2});
    QCOMPARE(enabling.read.entries.size(), std::size_t{2});
    verifyTheSame(enabling.read);

    const LinkBatchOutcome relinking = linking.profiles.Relink(stamp, shown, {&addons[1]});
    QCOMPARE(relinking.report.results.size(), std::size_t{2});
    verifyTheSame(relinking.read);

    const LinkBatchOutcome undoing = linking.profiles.UndoLastBatch(stamp, shown.libraries);
    QCOMPARE(undoing.report.results.size(), std::size_t{2});
    verifyTheSame(undoing.read);

    const LinkBatchOutcome disabling =
        linking.profiles.SetEnabled(stamp, shown, LinkBatch{.toDisable = {&addons[0]}, .toEnable = {}});
    QCOMPARE(disabling.report.results.size(), std::size_t{1});
    QCOMPARE(disabling.read.entries.size(), std::size_t{1});
    verifyTheSame(disabling.read);
}

void LinkPlanOnRealDiskTest::RelinkingABrokenAddonThatNeverStrayedReplacesTheDeadJunctionAndUndoPutsTheOtherNameBack()
{
    const Disk disk;
    Linking linking;

    const std::filesystem::path renamed = disk.Community() / "renamed-link";
    const std::filesystem::path doomed = disk.Root() / "doomed-target";
    std::filesystem::create_directories(doomed);

    QCOMPARE(linking.linkService.CreateLink(renamed, disk.Addon(), LinkType::Junction), LinkFailure::None);
    QCOMPARE(linking.linkService.CreateLink(disk.Link(), doomed, LinkType::Junction), LinkFailure::None);
    std::filesystem::remove_all(doomed);

    const SimulatorProfile profile = ProfileOn(disk);
    const ProfileSnapshot shown = linking.profiles.Scan(profile);
    const TreeNode* addon = OnlyAddonOf(shown);
    QVERIFY(addon != nullptr);

    const AddonDestinations destinations(profile, shown.entries);
    QVERIFY(destinations.Of(disk.Addon()).IsBroken());
    QVERIFY(destinations.Of(disk.Addon()).strayedTo.empty());

    const EntriesStamp stamp{.profile = profile, .adoptions = 3};

    const auto verifyTheSame = [&](const EntriesRead& read)
    {
        const std::vector<DestinationEntry> full = linking.profiles.ResolveEntries(profile, shown.libraries);

        QCOMPARE(read.entries.size(), full.size());

        for (std::size_t index = 0; index < full.size(); ++index)
        {
            QCOMPARE(read.entries[index].path, full[index].path);
            QCOMPARE(read.entries[index].target, full[index].target);
            QCOMPARE(read.entries[index].classification, full[index].classification);
        }
    };

    const auto present = [](const std::filesystem::path& path)
    {
        return std::filesystem::exists(std::filesystem::symlink_status(path));
    };

    const LinkBatchOutcome relinking = linking.profiles.Relink(stamp, shown, {addon});

    QCOMPARE(relinking.report.results.size(), std::size_t{2});
    QCOMPARE(linking.linkService.ReadLinkTarget(disk.Link()), std::optional<std::filesystem::path>{disk.Addon()});
    QVERIFY(!present(renamed));
    QCOMPARE(relinking.read.entries.size(), std::size_t{1});
    QCOMPARE(relinking.read.entries.front().path, disk.Link());
    QCOMPARE(relinking.read.entries.front().classification, EntryClassification::Managed);
    verifyTheSame(relinking.read);

    const LinkBatchOutcome undoing = linking.profiles.UndoLastBatch(stamp, shown.libraries);

    QCOMPARE(undoing.report.results.size(), std::size_t{2});
    QCOMPARE(linking.linkService.ReadLinkTarget(renamed), std::optional<std::filesystem::path>{disk.Addon()});
    QVERIFY(!present(disk.Link()));
    verifyTheSame(undoing.read);
}

void LinkPlanOnRealDiskTest::AFullReadClassifiesEveryKindOfEntryOnRealJunctions()
{
    const Disk disk;
    Linking linking;

    const std::filesystem::path second = disk.Root() / "Community2";
    const std::filesystem::path elsewhere = disk.Root() / "Elsewhere";
    const std::filesystem::path aircrafts = disk.Library() / "Aircrafts";

    const auto folder = [](const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path);
        return path;
    };
    const auto junction = [&linking](const std::filesystem::path& place, const std::filesystem::path& target)
    {
        QCOMPARE(linking.linkService.CreateLink(place, target, LinkType::Junction), LinkFailure::None);
    };

    folder(second);

    const std::filesystem::path managed = folder(aircrafts / "managed-addon");
    const std::filesystem::path lent = folder(aircrafts / "lent-addon");
    const std::filesystem::path returned = folder(aircrafts / "returned-addon");
    const std::filesystem::path gone = folder(aircrafts / "vanished-addon");
    const std::filesystem::path both = folder(aircrafts / "both-addon");
    const std::filesystem::path outside = folder(elsewhere / "outside-addon");
    const std::filesystem::path lentVendor = elsewhere / "lent-vendor";
    const std::filesystem::path returnedVendor = folder(elsewhere / "returned-vendor");
    const std::filesystem::path goneVendor = elsewhere / "vanished-vendor";
    const std::filesystem::path bothVendor = folder(elsewhere / "both-vendor");
    const std::filesystem::path deadTarget = folder(disk.Root() / "doomed-target");

    folder(disk.Community() / "plain-folder");
    junction(disk.Community() / kAddonFolder, disk.Addon());
    junction(second / "fenix-copy", disk.Addon());
    junction(disk.Community() / "managed-addon", managed);
    junction(disk.Community() / "dead-link", deadTarget);
    junction(disk.Community() / "outside-link", outside);
    junction(disk.Community() / "lent-addon", lent);
    junction(disk.Community() / "returned-addon", returned);
    junction(disk.Community() / "vanished-addon", gone);
    junction(disk.Community() / "both-addon", bothVendor);

    std::filesystem::remove_all(deadTarget);
    std::filesystem::remove_all(gone);

    const std::vector<ExternalAddon> externals{
        ExternalAddon{.addonFolder = lent, .externalPath = lentVendor},
        ExternalAddon{.addonFolder = returned, .externalPath = returnedVendor},
        ExternalAddon{.addonFolder = gone, .externalPath = goneVendor},
        ExternalAddon{.addonFolder = both, .externalPath = bothVendor},
    };

    struct Expected
    {
        std::filesystem::path place{};
        EntryClassification classification{};
        std::filesystem::path target{};
        std::filesystem::path externalOrigin{};
        std::filesystem::path libraryCopy{};
        bool tookItsFolderBack{};
    };

    const std::vector<Expected> expected{
        {disk.Community() / kAddonFolder, EntryClassification::Duplicated, disk.Addon(), {}, {}, false},
        {second / "fenix-copy", EntryClassification::Duplicated, disk.Addon(), {}, {}, false},
        {disk.Community() / "managed-addon", EntryClassification::Managed, managed, {}, {}, false},
        {disk.Community() / "plain-folder", EntryClassification::Unmanaged, {}, {}, {}, false},
        {disk.Community() / "dead-link", EntryClassification::Broken, deadTarget, {}, {}, false},
        {disk.Community() / "outside-link", EntryClassification::External, outside, {}, {}, false},
        {disk.Community() / "lent-addon", EntryClassification::Managed, lent, lentVendor, lent, false},
        {disk.Community() / "returned-addon", EntryClassification::Divergent, returned, returnedVendor, returned, true},
        {disk.Community() / "vanished-addon", EntryClassification::Vanished, gone, goneVendor, gone, false},
        {disk.Community() / "both-addon", EntryClassification::Divergent, bothVendor, bothVendor, both, true},
    };

    const std::vector<DestinationEntry> entries =
        linking.classifier.Resolve({disk.Community(), second}, {disk.Library()}, externals);

    QCOMPARE(entries.size(), expected.size());

    for (const Expected& want : expected)
    {
        const auto found = std::ranges::find_if(entries,
                                                [&want](const DestinationEntry& entry)
                                                {
                                                    return ComparablePath(entry.path) == ComparablePath(want.place);
                                                });
        const std::string label = AsUtf8(want.place);
        QVERIFY2(found != entries.end(), label.c_str());

        QVERIFY2(found->classification == want.classification, label.c_str());
        QCOMPARE(ComparablePath(found->target), ComparablePath(want.target));
        QCOMPARE(ComparablePath(found->externalOrigin), ComparablePath(want.externalOrigin));
        QCOMPARE(ComparablePath(found->libraryCopy), ComparablePath(want.libraryCopy));
        QCOMPARE(found->theOtherProgramTookItsFolderBack, want.tookItsFolderBack);
    }

    std::vector<std::filesystem::path> places;
    places.reserve(expected.size());
    for (const Expected& want : expected)
    {
        places.push_back(want.place);
    }
    places.push_back(disk.Community() / "never-created");

    const std::vector<std::optional<std::filesystem::path>> together = linking.linkService.ReadLinkTargets(places);
    QCOMPARE(together.size(), places.size());

    for (std::size_t index = 0; index < places.size(); ++index)
    {
        const std::optional<std::filesystem::path> alone = linking.linkService.ReadLinkTarget(places[index]);
        QCOMPARE(together[index].has_value(), alone.has_value());
        if (alone.has_value())
        {
            QCOMPARE(*together[index], *alone);
        }
    }
}

void LinkPlanOnRealDiskTest::ARepairRemovesTheJunctionWhoseTargetStayedDeletedAndLeavesTheOneWhoseTargetCameBack()
{
    const Disk disk;
    Linking linking;
    const SimulatorProfile profile = ProfileOn(disk);

    const std::filesystem::path elsewhere = disk.Root() / "Elsewhere";
    const std::filesystem::path stays = elsewhere / "stays-dead";
    const std::filesystem::path returns = elsewhere / "comes-back";
    const std::filesystem::path staysPlace = disk.Community() / "stays-dead";
    const std::filesystem::path returnsPlace = disk.Community() / "comes-back";

    for (const std::filesystem::path& target : {stays, returns})
    {
        std::filesystem::create_directories(target);
    }

    QCOMPARE(linking.linkService.CreateLink(staysPlace, stays, LinkType::Junction), LinkFailure::None);
    QCOMPARE(linking.linkService.CreateLink(returnsPlace, returns, LinkType::Junction), LinkFailure::None);

    for (const std::filesystem::path& target : {stays, returns})
    {
        std::filesystem::remove_all(target);
    }

    const ProfileSnapshot shown = linking.profiles.Scan(profile);

    std::vector<RepairRequest> requests;
    for (const RepairCandidate& candidate : PlanRepairs(profile, shown.entries, shown.libraries))
    {
        requests.push_back({.candidate = candidate, .action = RepairAction::RemoveDeadNode});
    }

    QCOMPARE(requests.size(), std::size_t{2});

    std::filesystem::create_directories(returns);
    linking.journal.appended.clear();

    const LinkBatchOutcome outcome =
        linking.profiles.Repair(EntriesStamp{.profile = profile}, shown.libraries, requests);

    QCOMPARE(outcome.report.results.size(), std::size_t{1});
    QVERIFY(outcome.report.results.front().outcome.Succeeded());
    QCOMPARE(outcome.report.results.front().linkPath, staysPlace);
    QCOMPARE(outcome.report.drifted, std::size_t{1});

    QVERIFY(!linking.linkService.ReadLinkTarget(staysPlace).has_value());
    QCOMPARE(linking.linkService.ReadLinkTarget(returnsPlace), std::optional<std::filesystem::path>{returns});
    QVERIFY(std::filesystem::exists(returnsPlace));

    QCOMPARE(linking.journal.appended.size(), std::size_t{1});
    QCOMPARE(linking.journal.appended.front().kind, OperationKind::RemoveBrokenLink);
}

QTEST_APPLESS_MAIN(LinkPlanOnRealDiskTest)

#include "tst_link_plan_on_real_disk.moc"
