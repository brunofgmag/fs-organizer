#include <QtTest/QtTest>

#include <filesystem>
#include <string>
#include <vector>

#include "application/PresetService.h"
#include "application/preset/PresetStartupPlan.h"
#include "domain/tree/LibraryTrees.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
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
        static void ApplyingWalksTheDestinationOnceAndPlansBothHalvesFromThatWalk();
        static void APresetThatDoesNotGovernStartupPlansNothingEvenHoldingEntries();
        static void AGoverningPresetTurnsOnWhatItNamesAndLeavesWhatIsAlreadyOn();
        static void ReplaceTurnsOffTheStartupEntriesThePresetDoesNotName();
        static void CumulativeLeavesTheStartupEntriesThePresetDoesNotName();
        static void TheDisableModeIgnoresTheStartupEntriesThePresetTurnsOff();
        static void WithStartupManagementOffThePlanCountsWhatWillNotBeApplied();
        static void AStartupEntryTheFileNoLongerHoldsIsReportedAndPlansNothing();
        static void ApplyingWritesTheReturnPresetWithWhatWasOnBeforeIt();
        static void NothingIsAppliedWhenTheReturnPresetCannotBeWritten();
        static void TheReturnPresetIsOverwrittenAtEachApplication();
        static void TheReturnPresetIsNotOneOfThePresetsOfTheProfile();
        static void ApplyingTheReturnPresetPutsBackWhatWasOnBeforeTheLastApplication();
        static void ApplyingTheReturnPresetDoesNotOverwriteTheReturnPreset();
        static void TheReturnPresetGovernsStartupOnlyWhenTheAppliedPresetDid();
        static void TurningTheFlagOnCapturesTheStartupEntriesThatAreOnRightNow();
        static void UpdatingRefreshesTheStartupEntriesOnlyOfAPresetThatGovernsThem();
        static void ApplyingAGoverningPresetActuallyFlipsTheStartupEntry();
        static void AGoverningPresetIsNotSatisfiedWhenTheStartupFileDisagrees();
        static void AGoverningPresetIsSatisfiedWhenTheStartupFileMatches();
        static void ANonGoverningPresetLeavesTheStartupFileOutOfSatisfaction();
        static void SettingAStartupActionRefusesWhenTheRowNoLongerHoldsThatEntry();
        static void RecapturingTheStartupTakesWhatIsEnabledNowAndGoverns();
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

        FakeSidecarStore sidecars{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        FakePresetRepository repository;
        PresetService service{repository, profiles, startup.service};
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

void PresetServiceTest::ApplyingWalksTheDestinationOnceAndPlansBothHalvesFromThatWalk()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj");

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> preset = f.service.Load(kProfileId, "Voo curto");
    QVERIFY(preset.has_value());

    const ProfileSnapshot shown = f.Snapshot(profile);
    const std::size_t before = f.filesystemProbe.TimesEnumerated(kCommunity);

    static_cast<void>(f.service.Apply(profile, shown, *preset, ApplyMode::Replace));

    QCOMPARE(f.filesystemProbe.TimesEnumerated(kCommunity) - before, std::size_t{1});
}

namespace
{
    constexpr auto kLauncher = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe";
    constexpr auto kOtherLauncher = "D:/MSFS 2024/Aircrafts/aerosoft-crj/tool.exe";
    constexpr auto kStranger = "C:/Program Files/Outro/agent.exe";

    StartupLine Line(const std::filesystem::path& path, const bool enabled)
    {
        return {.label = path.stem().string(), .path = path, .enabled = enabled};
    }

    Preset GoverningStartup(std::vector<PresetStartupEntry> entries)
    {
        return {.name = "Voo curto", .entries = {}, .startupEntries = std::move(entries), .governsStartup = true};
    }

    PresetStartupEntry TurningOn(const std::filesystem::path& path)
    {
        return {.path = path, .action = PresetAction::Enable};
    }

    PresetStartupEntry TurningOff(const std::filesystem::path& path)
    {
        return {.path = path, .action = PresetAction::Disable};
    }
}

void PresetServiceTest::APresetThatDoesNotGovernStartupPlansNothingEvenHoldingEntries()
{
    Preset preset = GoverningStartup({TurningOn(kLauncher)});
    preset.governsStartup = false;

    const PresetStartupPlan plan =
        PlanPresetStartup(preset, ApplyMode::Replace, {Line(kLauncher, false), Line(kStranger, true)}, true);

    QVERIFY(plan.toTurnOn.empty());
    QVERIFY(plan.toTurnOff.empty());
    QCOMPARE(plan.asked, std::size_t{0});
    QCOMPARE(plan.notApplied, std::size_t{0});
}

void PresetServiceTest::AGoverningPresetTurnsOnWhatItNamesAndLeavesWhatIsAlreadyOn()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher), TurningOn(kOtherLauncher)});

    const PresetStartupPlan plan =
        PlanPresetStartup(preset, ApplyMode::Cumulative, {Line(kLauncher, false), Line(kOtherLauncher, true)}, true);

    QCOMPARE(plan.toTurnOn.size(), std::size_t{1});
    QCOMPARE(plan.toTurnOn.front().path, std::filesystem::path{kLauncher});
    QVERIFY(plan.toTurnOff.empty());
    QCOMPARE(plan.asked, std::size_t{2});
    QCOMPARE(plan.notApplied, std::size_t{0});
}

void PresetServiceTest::ReplaceTurnsOffTheStartupEntriesThePresetDoesNotName()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher)});

    const PresetStartupPlan plan = PlanPresetStartup(
        preset, ApplyMode::Replace, {Line(kLauncher, true), Line(kOtherLauncher, true), Line(kStranger, false)}, true);

    QVERIFY(plan.toTurnOn.empty());
    QCOMPARE(plan.toTurnOff.size(), std::size_t{1});
    QCOMPARE(plan.toTurnOff.front().path, std::filesystem::path{kOtherLauncher});
}

void PresetServiceTest::CumulativeLeavesTheStartupEntriesThePresetDoesNotName()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher)});

    const PresetStartupPlan plan =
        PlanPresetStartup(preset, ApplyMode::Cumulative, {Line(kLauncher, true), Line(kOtherLauncher, true)}, true);

    QVERIFY(plan.toTurnOn.empty());
    QVERIFY(plan.toTurnOff.empty());
}

void PresetServiceTest::TheDisableModeIgnoresTheStartupEntriesThePresetTurnsOff()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher), TurningOff(kOtherLauncher)});

    const PresetStartupPlan plan =
        PlanPresetStartup(preset, ApplyMode::Disable, {Line(kLauncher, true), Line(kOtherLauncher, false)}, true);

    QCOMPARE(plan.toTurnOff.size(), std::size_t{1});
    QCOMPARE(plan.toTurnOff.front().path, std::filesystem::path{kLauncher});
    QVERIFY(plan.toTurnOn.empty());
}

void PresetServiceTest::WithStartupManagementOffThePlanCountsWhatWillNotBeApplied()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher), TurningOn(kOtherLauncher)});

    const PresetStartupPlan plan = PlanPresetStartup(preset, ApplyMode::Replace, {}, false);

    QVERIFY(plan.toTurnOn.empty());
    QVERIFY(plan.toTurnOff.empty());
    QCOMPARE(plan.asked, std::size_t{2});
    QCOMPARE(plan.notApplied, std::size_t{2});
}

void PresetServiceTest::AStartupEntryTheFileNoLongerHoldsIsReportedAndPlansNothing()
{
    const Preset preset = GoverningStartup({TurningOn(kLauncher), TurningOn(kOtherLauncher)});

    const PresetStartupPlan plan = PlanPresetStartup(preset, ApplyMode::Cumulative, {Line(kLauncher, false)}, true);

    QCOMPARE(plan.toTurnOn.size(), std::size_t{1});
    QCOMPARE(plan.unresolved.size(), std::size_t{1});
    QCOMPARE(plan.unresolved.front(), std::filesystem::path{kOtherLauncher});
    QCOMPARE(plan.notApplied, std::size_t{0});
}

namespace
{
    std::vector<std::string> FolderNamesOf(const Preset& preset)
    {
        std::vector<std::string> names;

        for (const PresetEntry& entry : preset.entries)
        {
            names.push_back(entry.addonId.folderName);
        }

        std::ranges::sort(names);

        return names;
    }
}

void PresetServiceTest::ApplyingWritesTheReturnPresetWithWhatWasOnBeforeIt()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const Preset preset{
        .name = "Voo curto",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "aerosoft-crj"}, .action = PresetAction::Enable}}};

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Replace);

    QVERIFY(report.refusal == PresetApplyRefusal::None);

    const std::optional<Preset> back = f.service.ReturnPreset(kProfileId);

    QVERIFY(back.has_value());
    QCOMPARE(FolderNamesOf(*back), std::vector<std::string>{"pmdg-aircraft-77w"});
    QVERIFY(!back->governsStartup);
}

void PresetServiceTest::NothingIsAppliedWhenTheReturnPresetCannotBeWritten()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.repository.RefuseEveryWrite();

    const SimulatorProfile profile = Profile();
    const Preset preset{
        .name = "Voo curto",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "aerosoft-crj"}, .action = PresetAction::Enable}}};

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Replace);

    QVERIFY(report.refusal == PresetApplyRefusal::TheReturnPresetCouldNotBeWritten);
    QVERIFY(report.results.empty());
    QVERIFY(!f.service.ReturnPreset(kProfileId).has_value());
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void PresetServiceTest::TheReturnPresetIsOverwrittenAtEachApplication()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const Preset first{
        .name = "Um",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "aerosoft-crj"}, .action = PresetAction::Enable}}};
    const Preset second{
        .name = "Dois",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "fenix-a320"}, .action = PresetAction::Enable}}};

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), first, ApplyMode::Replace));
    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), second, ApplyMode::Replace));

    const std::optional<Preset> back = f.service.ReturnPreset(kProfileId);

    QVERIFY(back.has_value());
    QCOMPARE(FolderNamesOf(*back), std::vector<std::string>{"aerosoft-crj"});
}

void PresetServiceTest::TheReturnPresetIsNotOneOfThePresetsOfTheProfile()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> preset = f.service.Load(kProfileId, "Voo curto");
    QVERIFY(preset.has_value());

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), *preset, ApplyMode::Replace));

    QVERIFY(f.service.ReturnPreset(kProfileId).has_value());
    QCOMPARE(f.service.List(kProfileId).size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(f.service.List(kProfileId).front().name), QString{"Voo curto"});
}

void PresetServiceTest::ApplyingTheReturnPresetPutsBackWhatWasOnBeforeTheLastApplication()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const Preset preset{
        .name = "Voo curto",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "aerosoft-crj"}, .action = PresetAction::Enable}}};

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Replace));

    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));

    const std::optional<Preset> back = f.service.ReturnPreset(kProfileId);
    QVERIFY(back.has_value());

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), *back, ApplyMode::Replace));

    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void PresetServiceTest::ApplyingTheReturnPresetDoesNotOverwriteTheReturnPreset()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");

    const SimulatorProfile profile = Profile();
    const Preset preset{
        .name = "Voo curto",
        .entries = {PresetEntry{.addonId = AddonId{kLibraryId, "aerosoft-crj"}, .action = PresetAction::Enable}}};

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Replace));

    const std::optional<Preset> back = f.service.ReturnPreset(kProfileId);
    QVERIFY(back.has_value());
    QCOMPARE(FolderNamesOf(*back), std::vector<std::string>{"pmdg-aircraft-77w"});

    static_cast<void>(f.service.ApplyTheReturn(profile, f.Snapshot(profile), *back));

    const std::optional<Preset> anchor = f.service.ReturnPreset(kProfileId);
    QVERIFY(anchor.has_value());
    QCOMPARE(FolderNamesOf(*anchor), std::vector<std::string>{"pmdg-aircraft-77w"});

    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void PresetServiceTest::TheReturnPresetGovernsStartupOnlyWhenTheAppliedPresetDid()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = true});

    const SimulatorProfile profile = Profile();
    const Preset governing = GoverningStartup({TurningOn(kLauncher)});

    static_cast<void>(f.service.Apply(profile, f.Snapshot(profile), governing, ApplyMode::Cumulative));

    const std::optional<Preset> back = f.service.ReturnPreset(kProfileId);

    QVERIFY(back.has_value());
    QVERIFY(back->governsStartup);
    QCOMPARE(back->startupEntries.size(), std::size_t{1});
    QCOMPARE(back->startupEntries.front().path, std::filesystem::path{kLauncher});
    QVERIFY(back->startupEntries.front().action == PresetAction::Enable);
}

void PresetServiceTest::TurningTheFlagOnCapturesTheStartupEntriesThatAreOnRightNow()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = true});
    f.startup.entries.Carry(StartupEntry{.label = "Aerosoft", .path = kOtherLauncher, .enabled = false});

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    QVERIFY(f.service.GovernStartup(profile, f.Snapshot(profile), "Voo curto", true));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->governsStartup);
    QCOMPARE(saved->startupEntries.size(), std::size_t{1});
    QCOMPARE(saved->startupEntries.front().path, std::filesystem::path{kLauncher});
}

void PresetServiceTest::UpdatingRefreshesTheStartupEntriesOnlyOfAPresetThatGovernsThem()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = false});

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Quieto"));
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Governa"));
    QVERIFY(f.service.GovernStartup(profile, f.Snapshot(profile), "Governa", true));

    QCOMPARE(f.service.Load(kProfileId, "Governa")->startupEntries.size(), std::size_t{0});

    QVERIFY(f.startup.service.Switch(kLauncher, true) == FileResult::Completed);

    QVERIFY(f.service.Update(profile, f.Snapshot(profile), "Governa"));
    QVERIFY(f.service.Update(profile, f.Snapshot(profile), "Quieto"));

    QCOMPARE(f.service.Load(kProfileId, "Governa")->startupEntries.size(), std::size_t{1});
    QVERIFY(f.service.Load(kProfileId, "Governa")->governsStartup);
    QCOMPARE(f.service.Load(kProfileId, "Quieto")->startupEntries.size(), std::size_t{0});
    QVERIFY(!f.service.Load(kProfileId, "Quieto")->governsStartup);
}

void PresetServiceTest::ApplyingAGoverningPresetActuallyFlipsTheStartupEntry()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = false});
    f.startup.entries.Carry(StartupEntry{.label = "Outro", .path = kStranger, .enabled = true});

    const SimulatorProfile profile = Profile();
    const Preset preset = GoverningStartup({TurningOn(kLauncher)});

    const PresetApplyReport report = f.service.Apply(profile, f.Snapshot(profile), preset, ApplyMode::Cumulative);

    QVERIFY(report.refusal == PresetApplyRefusal::None);
    QCOMPARE(f.startup.entries.writes, std::size_t{1});
    QCOMPARE(report.results.size(), std::size_t{1});
    QVERIFY(report.results.front().kind == OperationKind::TurnOnTheStartupEntry);
    QVERIFY(report.results.front().outcome.Succeeded());
    QCOMPARE(report.results.front().linkPath, std::filesystem::path{kLauncher});

    const std::vector<StartupEntry> after = f.startup.entries.Entries();

    QCOMPARE(after.front().enabled, true);
    QCOMPARE(after.back().enabled, true);
}

void PresetServiceTest::AGoverningPresetIsNotSatisfiedWhenTheStartupFileDisagrees()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = false});

    const SimulatorProfile profile = Profile();
    const Preset preset = GoverningStartup({TurningOn(kLauncher)});

    QVERIFY(!f.service.IsSatisfied(profile, f.Snapshot(profile), preset));
}

void PresetServiceTest::AGoverningPresetIsSatisfiedWhenTheStartupFileMatches()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = true});

    const SimulatorProfile profile = Profile();
    const Preset preset = GoverningStartup({TurningOn(kLauncher)});

    QVERIFY(f.service.IsSatisfied(profile, f.Snapshot(profile), preset));
}

void PresetServiceTest::ANonGoverningPresetLeavesTheStartupFileOutOfSatisfaction()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = false});

    const SimulatorProfile profile = Profile();
    Preset preset;
    preset.name = "Voo curto";

    QVERIFY(f.service.IsSatisfied(profile, f.Snapshot(profile), preset));
}

void PresetServiceTest::SettingAStartupActionRefusesWhenTheRowNoLongerHoldsThatEntry()
{
    Fixture f;

    Preset stored;
    stored.name = "Voo curto";
    stored.governsStartup = true;
    stored.startupEntries = {TurningOn(kLauncher), TurningOn(kOtherLauncher)};
    QVERIFY(f.repository.Save(kProfileId, stored));

    QVERIFY(f.service.SetStartupAction(kProfileId, "Voo curto", 1, kOtherLauncher, PresetAction::Disable));
    QVERIFY(!f.service.SetStartupAction(kProfileId, "Voo curto", 1, kLauncher, PresetAction::Disable));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->startupEntries.front().action == PresetAction::Enable);
    QVERIFY(saved->startupEntries.back().action == PresetAction::Disable);
}

void PresetServiceTest::RecapturingTheStartupTakesWhatIsEnabledNowAndGoverns()
{
    Fixture f;
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = kLauncher, .enabled = true});
    f.startup.entries.Carry(StartupEntry{.label = "Aerosoft", .path = kOtherLauncher, .enabled = false});

    const SimulatorProfile profile = Profile();
    QVERIFY(f.service.Create(profile, f.Snapshot(profile), "Voo curto"));

    QVERIFY(f.service.RecaptureStartup(profile, f.Snapshot(profile), "Voo curto"));

    const std::optional<Preset> saved = f.service.Load(kProfileId, "Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->governsStartup);
    QCOMPARE(saved->startupEntries.size(), std::size_t{1});
    QCOMPARE(saved->startupEntries.front().path, std::filesystem::path{kLauncher});
}

QTEST_APPLESS_MAIN(PresetServiceTest)

#include "tst_preset_service.moc"
