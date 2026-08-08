#include <QtTest/QtTest>

#include <filesystem>
#include <string>
#include <vector>

#include "application/PresetService.h"
#include "domain/tree/LibraryTrees.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PresetServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void CreatingAPresetCapturesWhatIsEnabledRightNow();
        static void UpdatingRewritesTheEnabledEntriesAndKeepsTheDisableOnes();
        static void UpdatingDropsADisableEntryForAnAddonThatIsOnAgain();
        static void SettingAnActionRefusesWhenTheRowNoLongerHoldsThatAddon();
        static void ApplyingInReplaceRestoresExactlyTheSavedSet();
        static void ReplaceLeavesAFolderTheAppDidNotLinkAlone();
        static void ApplyingReportsTheEntriesThatNoLongerResolve();
        static void ApplyingLinksAnAddonWhoseLinkVanishedAfterTheScan();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kLibraryId = "library-1";
    constexpr auto kProfileId = "msfs2024";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
                              AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                              AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/aerosoft-crj");
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
            catalog.SetTree(kLibrary, LibraryTree());
        }

        [[nodiscard]] ProfileSnapshot Snapshot(const SimulatorProfile& profile) const
        {
            return profiles.Scan(profile);
        }

        [[nodiscard]] static const TreeNode* AddonAt(const ProfileSnapshot& snapshot, const std::size_t index)
        {
            return &snapshot.libraries.front().children.front().children[index];
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService profiles{catalog, filesystemProbe, classifier, linking, log, identities, LinkType::Junction};
        FakePresetRepository repository;
        PresetService service{repository, profiles};
    };
}

void PresetServiceTest::CreatingAPresetCapturesWhatIsEnabledRightNow()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();

    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QCOMPARE(saved->entries.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(saved->entries.front().addonId.folderName), QString{"aerosoft-crj"});
    QVERIFY(saved->entries.front().action == PresetAction::Enable);
}

void PresetServiceTest::UpdatingRewritesTheEnabledEntriesAndKeepsTheDisableOnes()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    Preset stored;
    stored.name = "Voo curto";
    stored.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "fenix-a320"},
                                  .action = PresetAction::Enable},
                      PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "pmdg-aircraft-77w"},
                                  .action = PresetAction::Disable}};
    QVERIFY(f.repository.Save(kProfileId, stored));

    const SimulatorProfile profile = Profile();

    QVERIFY(f.service.Update(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QCOMPARE(saved->entries.size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(saved->entries.front().addonId.folderName), QString{"pmdg-aircraft-77w"});
    QVERIFY(saved->entries.front().action == PresetAction::Disable);
    QCOMPARE(QString::fromStdString(saved->entries.back().addonId.folderName), QString{"aerosoft-crj"});
    QVERIFY(saved->entries.back().action == PresetAction::Enable);
}

void PresetServiceTest::UpdatingDropsADisableEntryForAnAddonThatIsOnAgain()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    Preset stored;
    stored.name = "Voo curto";
    stored.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "pmdg-aircraft-77w"},
                                  .action = PresetAction::Disable}};
    QVERIFY(f.repository.Save(kProfileId, stored));

    const SimulatorProfile profile = Profile();

    QVERIFY(f.service.Update(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QCOMPARE(saved->entries.size(), std::size_t{1});
    QVERIFY(saved->entries.front().action == PresetAction::Enable);
}

void PresetServiceTest::SettingAnActionRefusesWhenTheRowNoLongerHoldsThatAddon()
{
    Fixture f;

    Preset stored;
    stored.name = "Voo curto";
    stored.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable},
                      PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "fenix-a320"},
                                  .action = PresetAction::Enable}};
    QVERIFY(f.repository.Save(kProfileId, stored));

    QVERIFY(f.service.SetAction(kProfileId, "Voo curto", 1, AddonId{kLibraryId, "fenix-a320"}, PresetAction::Disable));
    QVERIFY(
        !f.service.SetAction(kProfileId, "Voo curto", 1, AddonId{kLibraryId, "aerosoft-crj"}, PresetAction::Disable));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->entries.front().action == PresetAction::Enable);
    QVERIFY(saved->entries.back().action == PresetAction::Disable);
}

void PresetServiceTest::ApplyingInReplaceRestoresExactlyTheSavedSet()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/fenix-a320", "D:/MSFS 2024/Aircrafts/fenix-a320");

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    const ProfileSnapshot before = f.Snapshot(profile);
    QCOMPARE(
        f.profiles.SetEnabled(profile, before, LinkBatch{{Fixture::AddonAt(before, 2)}, {Fixture::AddonAt(before, 0)}})
            .results.size(),
        std::size_t{2});

    const std::optional<Preset> preset = f.service.Load(kProfileId, "Voo curto");
    QVERIFY(preset.has_value());

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), *preset, ApplyMode::Replace);

    QCOMPARE(report.results.size(), std::size_t{2});
    QVERIFY(report.unresolved.empty());
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/fenix-a320"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
}

void PresetServiceTest::ReplaceLeavesAFolderTheAppDidNotLinkAlone()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/some-other-package");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/fenix-a320", "D:/MSFS 2024/Aircrafts/fenix-a320");

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    const SimulatorProfile profile = Profile();

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Replace);

    QCOMPARE(report.results.size(), std::size_t{2});
    QVERIFY(f.fileSystem.IsDirectory("E:/Flight Simulator 2024/Community/some-other-package"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/fenix-a320"));
}

void PresetServiceTest::ApplyingReportsTheEntriesThatNoLongerResolve()
{
    Fixture f;

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable},
                      PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aircraft-que-sumiu"},
                                  .action = PresetAction::Enable}};

    const SimulatorProfile profile = Profile();

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Cumulative);

    QCOMPARE(report.unresolved.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(report.unresolved.front().folderName), QString{"aircraft-que-sumiu"});
    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void PresetServiceTest::ApplyingLinksAnAddonWhoseLinkVanishedAfterTheScan()
{
    const std::filesystem::path link = "E:/Flight Simulator 2024/Community/aerosoft-crj";

    Fixture f;
    f.fileSystem.AddLink(link, "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> preset = f.service.Load(kProfileId, "Voo curto");
    QVERIFY(preset.has_value());

    const ProfileSnapshot shown = f.Snapshot(profile);
    QVERIFY(shown.enabled.Contains("D:/MSFS 2024/Aircrafts/aerosoft-crj"));

    QVERIFY(f.fileSystem.RemoveNode(link));

    const PresetApplyReport report = f.service.Apply(profile, shown, *preset, ApplyMode::Replace);

    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(report.results.front().outcome.Succeeded());
    QVERIFY(f.fileSystem.IsLink(link));
}

QTEST_APPLESS_MAIN(PresetServiceTest)

#include "tst_preset_service.moc"
