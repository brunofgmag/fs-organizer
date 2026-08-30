#include <QtTest/QtTest>

#include "application/LegacyConfigImporter.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/Session.h"
#include "domain/journal/OperationLog.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLegacyConfigSource.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "viewmodel/LegacyImportViewModel.h"

namespace
{
    class LegacyImportViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void NothingWaitsWhenNoInstallationWasFound();
        static void ALibraryTheProfileDoesNotHaveIsWaiting();
        static void ALibraryAlreadyRegisteredWithEveryCategoryOnDiskWaitsForNothing();
        static void ACategoryTheLibraryDoesNotHaveYetIsWaiting();
        static void ALibraryWhoseRootIsGoneWaitsForNothing();
        static void TheCounterAnswersHowManyPresetsAreInTheFolder();
        static void NoPresetWaitsInAnInstallationThatNamesNoPresetsFolder();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kLegacy2024 = "C:/ProgramData/MSFS Addons Linker 2024";

    TreeNode LibraryTree()
    {
        TreeNode addon;
        addon.kind = TreeNodeKind::Addon;
        addon.path = kAddon;
        addon.addon = Addon{.folderPath = kAddon, .manifest = Manifest{}};

        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {std::move(addon)};

        TreeNode root;
        root.kind = TreeNodeKind::Library;
        root.path = kLibrary;
        root.children = {std::move(aircrafts)};

        return root;
    }

    LegacyInstallation InstallationAt(const std::filesystem::path& folder,
                                      const std::vector<std::filesystem::path>& addonPaths)
    {
        LegacyInstallation installation;
        installation.folder = folder;
        installation.addonPaths = addonPaths;

        return installation;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakeLegacyConfigSource legacy;
        LegacyConfigImporter importer{legacy, filesystemProbe};
        FakePresetRepository presetRepository;
        PresetService presets{presetRepository, service, startup.service};
        LegacyImportViewModel viewModel{session, notifier, importer, presets, runner};
    };
}

void LegacyImportViewModelTest::NothingWaitsWhenNoInstallationWasFound()
{
    const Fixture f;

    QVERIFY(!f.viewModel.SomethingIsWaiting());
}

void LegacyImportViewModelTest::ALibraryTheProfileDoesNotHaveIsWaiting()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024 Extra");
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024 Extra/Aircrafts", "D:/MSFS 2024 Extra/Sceneries"}));

    QVERIFY(f.viewModel.SomethingIsWaiting());
}

void LegacyImportViewModelTest::ALibraryAlreadyRegisteredWithEveryCategoryOnDiskWaitsForNothing()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024/Aircrafts"}));

    QVERIFY(!f.viewModel.SomethingIsWaiting());
}

void LegacyImportViewModelTest::ACategoryTheLibraryDoesNotHaveYetIsWaiting()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}));

    QVERIFY(f.viewModel.SomethingIsWaiting());
}

void LegacyImportViewModelTest::ALibraryWhoseRootIsGoneWaitsForNothing()
{
    Fixture f;
    f.legacy.Add(InstallationAt("C:/ProgramData/MSFS Addons Linker", {"D:/MSFS 2020/Aircrafts", "D:/MSFS 2020/Utils"}));

    QVERIFY(!f.viewModel.SomethingIsWaiting());
}

void LegacyImportViewModelTest::TheCounterAnswersHowManyPresetsAreInTheFolder()
{
    Fixture f;
    const std::filesystem::path presets = std::filesystem::path{kLegacy2024} / "Presets";

    f.legacy.PlacePreset(presets, LegacyPresetSelection{.name = "VFR", .enabledAddonNames = {"pmdg-aircraft-77w"}});
    f.legacy.PlacePreset(presets, LegacyPresetSelection{.name = "IFR", .enabledAddonNames = {"nobody-scanned-this"}});
    f.legacy.PlacePreset(presets, LegacyPresetSelection{.name = "Empty"});

    QCOMPARE(f.viewModel.PresetsWaitingIn(presets), std::size_t{3});
}

void LegacyImportViewModelTest::NoPresetWaitsInAnInstallationThatNamesNoPresetsFolder()
{
    const Fixture f;

    QCOMPARE(f.viewModel.PresetsWaitingIn({}), std::size_t{0});
}

QTEST_GUILESS_MAIN(LegacyImportViewModelTest)

#include "tst_legacy_import_view_model.moc"
