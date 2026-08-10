#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>

#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "domain/importing/ImportPaths.h"
#include "domain/journal/JournalEntries.h"
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
    class LinkPlanOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EnablingAnAddonWhoseJunctionTheUserDeletedCreatesItAgain();
        static void ABatchWithNothingToDoStaysEmptyWhenTheDiskAgreesWithTheScan();
        static void ApplyingAPresetReachesAnAddonWhoseJunctionTheUserDeleted();
        static void SwappingTheOccupantMovesTheRealJunctionAndIsOneEntryInTheJournal();
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
        FilesystemScanner scanner{manifestParser, filesystemProbe};
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

    const PresetApplyReport report = linking.service.Apply(profile, shown, *preset, ApplyMode::Replace);

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

    const std::vector<TakenPlace> taken = linking.profiles.PlacesTaken(profile, {wanted});

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

QTEST_APPLESS_MAIN(LinkPlanOnRealDiskTest)

#include "tst_link_plan_on_real_disk.moc"
