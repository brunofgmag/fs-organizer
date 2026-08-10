#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class OptionsViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ALibraryLineCarriesItsCategoriesAddonsAndWhatIsEnabledFromIt();
        static void UnregisteringWithoutDisablingLeavesEveryLinkWhereItIs();
        static void UnregisteringWhileDisablingRemovesTheLinksAndSparesTheRealFolders();
        static void RemovingTheActiveProfileWithoutDisablingLeavesEveryLinkWhereItIs();
        static void OnlyTheActiveProfileIsAskedForItsAddonCount();
        static void TheProfileMarkedActiveIsTheOneTheOtherPanelsDescribe();
        static void TheScreenIsToldToRedrawWhenTheScanForTheNewProfileLands();
        static void TheChosenTypeOfLinkIsWrittenWhereTheNextStartupReadsIt();
        static void TheChosenTypeOfLinkReachesTheNextLinkWithoutReopeningTheApp();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kOtherAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";

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
        aircrafts.children = {AddonNode(kAddon), AddonNode(kOtherAddon)};

        TreeNode root;
        root.kind = TreeNodeKind::Library;
        root.path = kLibrary;
        root.children = {std::move(aircrafts)};

        return root;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kOtherDestination};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    SimulatorProfile LegacyProfile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2020";
        profile.variant = SimulatorVariant::MSFS2020;
        profile.destinations = {"C:/Packages/Community"};
        profile.defaultDestination = "C:/Packages/Community";
        profile.libraries = {Library{.id = "library-9", .path = "Z:/Legado", .label = "Legado"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddFile(std::filesystem::path(kAddon) / "manifest.json");
            fileSystem.AddDirectory(kOtherAddon);
            fileSystem.AddFile(std::filesystem::path(kOtherAddon) / "manifest.json");
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = "msfs2024";
        }

        void EnableOnDisk(const std::filesystem::path& addon) const
        {
            fileSystem.AddLink(std::filesystem::path(kCommunity) / addon.filename(), addon);
        }

        mutable InMemoryFileSystem fileSystem;
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
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        OptionsViewModel viewModel{session, service, settings, notifier};
    };
}

void OptionsViewModelTest::ALibraryLineCarriesItsCategoriesAddonsAndWhatIsEnabledFromIt()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const std::vector<LibraryLine> lines = f.viewModel.Libraries();

    QCOMPARE(lines.size(), std::size_t{1});
    QCOMPARE(lines.front().label, QStringLiteral("MSFS 2024"));
    QCOMPARE(lines.front().categories, std::size_t{1});
    QCOMPARE(lines.front().addons, std::size_t{2});
    QCOMPARE(lines.front().enabled, std::size_t{1});
}

void OptionsViewModelTest::UnregisteringWithoutDisablingLeavesEveryLinkWhereItIs()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    f.viewModel.UnregisterLibrary("library-1", false);

    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.session.Profile().libraries.empty());
    QCOMPARE(f.session.Snapshot().entries.front().classification, EntryClassification::External);
}

void OptionsViewModelTest::UnregisteringWhileDisablingRemovesTheLinksAndSparesTheRealFolders()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    f.viewModel.UnregisterLibrary("library-1", true);

    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.Exists(kAddon));
    QVERIFY(f.fileSystem.Exists(std::filesystem::path(kAddon) / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kOtherAddon));
    QVERIFY(f.session.Profile().libraries.empty());
    QVERIFY(f.session.Snapshot().entries.empty());
}

void OptionsViewModelTest::RemovingTheActiveProfileWithoutDisablingLeavesEveryLinkWhereItIs()
{
    Fixture f;
    f.settings.stored.profiles.push_back(LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    QVERIFY(f.viewModel.RemoveProfile("msfs2024", false));

    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.Exists(kAddon));
    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});
    QCOMPARE(f.session.Profile().id, std::string{"msfs2020"});
}

void OptionsViewModelTest::OnlyTheActiveProfileIsAskedForItsAddonCount()
{
    Fixture f;
    SimulatorProfile legacy;
    legacy.id = "msfs2020";
    legacy.variant = SimulatorVariant::MSFS2020;
    legacy.destinations = {"C:/Packages/Community"};
    legacy.libraries = {Library{.id = "library-9", .path = "Z:/Never Scanned", .label = "Legado"}};
    f.settings.stored.profiles.push_back(legacy);

    f.session.ShowActiveProfile();

    const std::vector<ProfileLine> profiles = f.viewModel.Profiles();

    QCOMPARE(profiles.size(), std::size_t{2});
    QVERIFY(profiles[0].active);
    QVERIFY(!profiles[1].active);
    QCOMPARE(profiles[1].destinations, std::size_t{1});
    QCOMPARE(profiles[1].libraries, std::size_t{1});
    QCOMPARE(f.viewModel.AddonsInTheActiveProfile(), std::size_t{2});
    QCOMPARE(f.catalog.scanned, std::size_t{1});
}

void OptionsViewModelTest::TheProfileMarkedActiveIsTheOneTheOtherPanelsDescribe()
{
    Fixture f;
    f.settings.stored.profiles.push_back(LegacyProfile());
    f.session.ShowActiveProfile();

    f.runner.defer = true;
    f.session.ChooseProfile("msfs2020");

    QVERIFY(f.runner.Pending());

    const std::vector<ProfileLine> profiles = f.viewModel.Profiles();
    const std::vector<DestinationLine> destinations = f.viewModel.Destinations();
    const std::vector<LibraryLine> libraries = f.viewModel.Libraries();

    QCOMPARE(profiles.size(), std::size_t{2});
    QVERIFY(profiles[0].active);
    QVERIFY(!profiles[1].active);

    QCOMPARE(destinations.size(), std::size_t{2});
    QCOMPARE(destinations.front().path, std::filesystem::path(kCommunity));
    QCOMPARE(libraries.size(), std::size_t{1});
    QCOMPARE(libraries.front().label, QStringLiteral("MSFS 2024"));
}

void OptionsViewModelTest::TheScreenIsToldToRedrawWhenTheScanForTheNewProfileLands()
{
    Fixture f;
    f.settings.stored.profiles.push_back(LegacyProfile());
    f.session.ShowActiveProfile();

    f.runner.defer = true;
    f.session.ChooseProfile("msfs2020");

    const QSignalSpy redraws(&f.viewModel, &OptionsViewModel::Changed);
    QCOMPARE(redraws.count(), 0);

    f.runner.Finish();

    QCOMPARE(redraws.count(), 1);

    const std::vector<ProfileLine> profiles = f.viewModel.Profiles();
    QVERIFY(profiles[1].active);
    QCOMPARE(f.viewModel.Destinations().front().path, std::filesystem::path("C:/Packages/Community"));
}

void OptionsViewModelTest::TheChosenTypeOfLinkIsWrittenWhereTheNextStartupReadsIt()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QCOMPARE(f.viewModel.TypeOfLink(), LinkType::Junction);

    f.viewModel.ChooseTypeOfLink(LinkType::Symbolic);

    QCOMPARE(f.settings.stored.linkType, LinkType::Symbolic);
    QCOMPARE(f.viewModel.TypeOfLink(), LinkType::Symbolic);
    QCOMPARE(f.viewModel.VerifiesWithHash(), false);
}

void OptionsViewModelTest::TheChosenTypeOfLinkReachesTheNextLinkWithoutReopeningTheApp()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode* addon = AddonNamed(f.session.Snapshot().libraries, "pmdg-aircraft-77w");
    QVERIFY(addon != nullptr);

    static_cast<void>(f.service.SetEnabled(f.session.Profile(), f.session.Snapshot(), {addon}, true));
    QCOMPARE(f.linkService.lastLinkType, LinkType::Junction);

    f.viewModel.ChooseTypeOfLink(LinkType::Symbolic);

    const TreeNode* other = AddonNamed(f.session.Snapshot().libraries, "aerosoft-crj");
    QVERIFY(other != nullptr);

    static_cast<void>(f.service.SetEnabled(f.session.Profile(), f.session.Snapshot(), {other}, true));

    QCOMPARE(f.linkService.lastLinkType, LinkType::Symbolic);
}

QTEST_MAIN(OptionsViewModelTest)

#include "tst_options_view_model.moc"
