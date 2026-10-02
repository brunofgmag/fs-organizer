#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "application/ports/ProcessProbe.h"
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
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/SimulatorText.h"

namespace
{
    class OptionsViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ALibraryLineCarriesItsCategoriesAddonsAndWhatIsEnabledFromIt();
        static void UnregisteringWithoutDisablingLeavesEveryLinkWhereItIs();
        static void UnregisteringWhileDisablingRemovesTheLinksAndSparesTheRealFolders();
        static void UnregisteringWhileDisablingHandsTheBatchToTheRunnerInsteadOfRunningIt();
        static void TheLibraryStaysRegisteredUntilTheWorkerLands();
        static void TheLibraryBatchSurvivesAScanReplacingTheSnapshotMeanwhile();
        static void TheLinksAreAnnouncedBeforeTheLibraryLeaves();
        static void TheLibrarySimulatorWarningComesFromTheWorkersReading();
        static void UnregisteringAnnouncesTheLibraryByItsLabelWithAndWithoutTheBatch();
        static void RemovingTheActiveProfileWithoutDisablingLeavesEveryLinkWhereItIs();
        static void RemovingTheActiveProfileWithoutDisablingDoesNotWaitOnTheRunner();
        static void RemovingTheActiveProfileWhileDisablingRemovesTheLinksThenTheProfile();
        static void RemovingTheActiveProfileWhileDisablingHandsTheBatchToTheRunner();
        static void TheProfileStaysRegisteredUntilTheWorkerLands();
        static void TheProfileSimulatorWarningComesFromTheWorkersReading();
        static void RemovingTheLastProfileWhileDisablingDisablesNothing();
        static void RemovingAProfileThatIsNotInUseDisablesNothingEvenWhenAsked();
        static void OnlyTheActiveProfileIsAskedForItsAddonCount();
        static void TheProfileMarkedActiveIsTheOneTheOtherPanelsDescribe();
        static void TheScreenIsToldToRedrawWhenTheScanForTheNewProfileLands();
        static void TheChosenTypeOfLinkIsWrittenWhereTheNextStartupReadsIt();
        static void TheChosenTypeOfLinkReachesTheNextLinkWithoutReopeningTheApp();
        static void TheChosenCheckIsWrittenDownAndAnnouncedToWhoeverImportsNext();
        static void RemovingAProfileWhileDisablingAnnouncesBusyAtTheStartAndAtTheLanding();
        static void UnregisteringWhileDisablingAnnouncesBusyAtTheStartAndAtTheLanding();
        static void RegisteringALibraryAnnouncesBusyAtTheStartAndAtTheLanding();
        static void AProfileThatCouldNotBeSavedIsNotReportedAsMissing();
        static void AProfileThatCouldNotBeSavedAfterTheBatchIsNotReportedAsMissing();
        static void ARegistrationOvertakenBySaveIsRunAgainOnTheProfileAsItIsNow();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kOtherAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kExtraLibrary = "D:/MSFS 2024 Extra";
    constexpr auto kThirdLibrary = "D:/Third Library";

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

    class CountingProcessProbe final : public ProcessProbe
    {
    public:
        void ReportTheSimulatorAsRunning()
        {
            running_ = "FlightSimulator2024.exe";
        }

        [[nodiscard]] std::optional<std::string> RunningSimulator() const override
        {
            ++asked;

            return running_;
        }

        mutable int asked = 0;

    private:
        std::optional<std::string> running_;
    };

    struct Fixture
    {
        Fixture()
        {
            QObject::connect(&viewModel, &OptionsViewModel::LinksDisabled, &viewModel,
                             [this](const std::vector<LinkOperationResult>& results)
                             {
                                 ++linksDisabledAnnouncements;
                                 linksDisabledResults = results.size();
                                 librariesWhenLinksWereDisabled = session.Profile().libraries.size();
                             });

            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddFile(std::filesystem::path(kAddon) / "manifest.json");
            fileSystem.AddDirectory(kOtherAddon);
            fileSystem.AddFile(std::filesystem::path(kOtherAddon) / "manifest.json");
            catalog.SetTree(kLibrary, LibraryTree());
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
        CountingProcessProbe processProbe;
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
        OptionsViewModel viewModel{session, service, runner, notifier};
        int linksDisabledAnnouncements = 0;
        std::size_t linksDisabledResults = 0;
        std::size_t librariesWhenLinksWereDisabled = 0;
    };

    void ExpectBusyAtTheStartAndAtTheLanding(Fixture& f, const std::function<void()>& ask)
    {
        const QSignalSpy busy(&f.viewModel, &OptionsViewModel::BusyChanged);
        f.runner.defer = true;

        QVERIFY(!f.viewModel.Busy());

        ask();

        QCOMPARE(busy.count(), 1);
        QVERIFY(f.viewModel.Busy());

        f.runner.defer = false;
        f.runner.Finish();

        QCOMPARE(busy.count(), 2);
        QVERIFY(!f.viewModel.Busy());
    }

    std::vector<std::string> LibraryPathsStored(const FakeSettingsRepository& repository)
    {
        std::vector<std::string> paths;

        for (const Library& library : repository.stored.profiles.front().libraries)
        {
            paths.push_back(ComparablePath(library.path));
        }

        return paths;
    }

    void AddProfile(Fixture& f, const SimulatorProfile& profile)
    {
        static_cast<void>(f.session.Rewrite(
            [&profile](AppSettings& settings)
            {
                settings.profiles.push_back(profile);

                return true;
            }));
    }
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

void OptionsViewModelTest::UnregisteringWhileDisablingHandsTheBatchToTheRunnerInsteadOfRunningIt()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    const int runsBefore = f.runner.runs;
    f.runner.defer = true;

    f.viewModel.UnregisterLibrary("library-1", true);

    QVERIFY2(f.runner.Pending(), "the batch goes through the runner instead of holding the calling thread");
    QCOMPARE(f.runner.runs, runsBefore + 1);
    QCOMPARE(f.linksDisabledAnnouncements, 0);
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void OptionsViewModelTest::TheLibraryStaysRegisteredUntilTheWorkerLands()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy unregistered(&f.viewModel, &OptionsViewModel::LibraryUnregistered);
    f.runner.defer = true;

    f.viewModel.UnregisterLibrary("library-1", true);
    f.runner.RunPendingWork();

    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(f.settings.stored.profiles.front().libraries.size(), std::size_t{1});
    QCOMPARE(unregistered.count(), 0);

    f.runner.Finish();

    QVERIFY(f.settings.stored.profiles.front().libraries.empty());
    QCOMPARE(unregistered.count(), 1);
}

void OptionsViewModelTest::TheLibraryBatchSurvivesAScanReplacingTheSnapshotMeanwhile()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    f.runner.defer = true;

    f.viewModel.UnregisterLibrary("library-1", true);
    f.session.ShowActiveProfile();
    f.runner.FinishNewestDone();
    f.runner.Finish();

    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QCOMPARE(f.linksDisabledResults, std::size_t{2});
    QVERIFY(f.settings.stored.profiles.front().libraries.empty());
}

void OptionsViewModelTest::TheLinksAreAnnouncedBeforeTheLibraryLeaves()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    f.viewModel.UnregisterLibrary("library-1", true);

    QCOMPARE(f.linksDisabledAnnouncements, 1);
    QCOMPARE(f.librariesWhenLinksWereDisabled, std::size_t{1});
    QVERIFY(f.session.Profile().libraries.empty());
}

void OptionsViewModelTest::TheLibrarySimulatorWarningComesFromTheWorkersReading()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();
    f.processProbe.ReportTheSimulatorAsRunning();

    const QSignalSpy warned(&f.notifier, &SessionNotifier::SimulatorIsRunning);
    const int askedBefore = f.processProbe.asked;
    f.runner.defer = true;

    f.viewModel.UnregisterLibrary("library-1", true);

    QCOMPARE(f.processProbe.asked, askedBefore);

    f.runner.RunPendingWork();

    QCOMPARE(f.processProbe.asked, askedBefore + 1);
    QCOMPARE(warned.count(), 0);

    f.runner.Finish();

    QCOMPARE(f.processProbe.asked, askedBefore + 1);
    QCOMPARE(warned.count(), 1);
}

void OptionsViewModelTest::UnregisteringAnnouncesTheLibraryByItsLabelWithAndWithoutTheBatch()
{
    Fixture plain;
    const QSignalSpy plainly(&plain.viewModel, &OptionsViewModel::LibraryUnregistered);
    plain.session.ShowActiveProfile();

    plain.viewModel.UnregisterLibrary("library-1", false);

    QCOMPARE(plainly.count(), 1);
    QCOMPARE(plainly.front().front().toString(), QStringLiteral("MSFS 2024"));

    Fixture disabling;
    disabling.EnableOnDisk(kAddon);
    const QSignalSpy afterTheBatch(&disabling.viewModel, &OptionsViewModel::LibraryUnregistered);
    disabling.session.ShowActiveProfile();

    disabling.viewModel.UnregisterLibrary("library-1", true);

    QCOMPARE(afterTheBatch.count(), 1);
    QCOMPARE(afterTheBatch.front().front().toString(), QStringLiteral("MSFS 2024"));
}

void OptionsViewModelTest::RemovingTheActiveProfileWithoutDisablingDoesNotWaitOnTheRunner()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    f.runner.defer = true;

    f.viewModel.RemoveProfile("msfs2024", false);

    QCOMPARE(removed.count(), 1);
    QCOMPARE(f.settings.stored.activeProfileId, std::string{"msfs2020"});
}

void OptionsViewModelTest::RemovingTheActiveProfileWhileDisablingRemovesTheLinksThenTheProfile()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    const QSignalSpy refused(&f.viewModel, &OptionsViewModel::ProfileNotRemoved);

    f.viewModel.RemoveProfile("msfs2024", true);

    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.Exists(kAddon));
    QVERIFY(f.fileSystem.Exists(kOtherAddon));
    QCOMPARE(f.linksDisabledAnnouncements, 1);
    QCOMPARE(f.linksDisabledResults, std::size_t{2});
    QCOMPARE(f.librariesWhenLinksWereDisabled, std::size_t{1});
    QCOMPARE(refused.count(), 0);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.front().front().toString(), NameOf(SimulatorVariant::MSFS2024));
    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});
    QCOMPARE(f.session.Profile().id, std::string{"msfs2020"});
}

void OptionsViewModelTest::RemovingTheActiveProfileWhileDisablingHandsTheBatchToTheRunner()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    const int runsBefore = f.runner.runs;
    f.runner.defer = true;

    f.viewModel.RemoveProfile("msfs2024", true);

    QVERIFY2(f.runner.Pending(), "the batch goes through the runner instead of holding the calling thread");
    QCOMPARE(f.runner.runs, runsBefore + 1);
    QCOMPARE(f.linksDisabledAnnouncements, 0);
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
}

void OptionsViewModelTest::TheProfileStaysRegisteredUntilTheWorkerLands()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    f.runner.defer = true;

    f.viewModel.RemoveProfile("msfs2024", true);
    f.runner.RunPendingWork();

    QVERIFY(!f.fileSystem.Exists("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.activeProfileId, std::string{"msfs2024"});
    QCOMPARE(removed.count(), 0);

    f.runner.Finish();

    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.activeProfileId, std::string{"msfs2020"});
    QCOMPARE(removed.count(), 1);
}

void OptionsViewModelTest::TheProfileSimulatorWarningComesFromTheWorkersReading()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();
    f.processProbe.ReportTheSimulatorAsRunning();

    const QSignalSpy warned(&f.notifier, &SessionNotifier::SimulatorIsRunning);
    const int askedBefore = f.processProbe.asked;
    f.runner.defer = true;

    f.viewModel.RemoveProfile("msfs2024", true);

    QCOMPARE(f.processProbe.asked, askedBefore);

    f.runner.RunPendingWork();

    QCOMPARE(f.processProbe.asked, askedBefore + 1);
    QCOMPARE(warned.count(), 0);

    f.runner.Finish();

    QCOMPARE(f.processProbe.asked, askedBefore + 1);
    QCOMPARE(warned.count(), 1);
}

void OptionsViewModelTest::RemovingTheLastProfileWhileDisablingDisablesNothing()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.EnableOnDisk(kOtherAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    const QSignalSpy refused(&f.viewModel, &OptionsViewModel::ProfileNotRemoved);
    const int runsBefore = f.runner.runs;

    f.viewModel.RemoveProfile("msfs2024", true);

    QCOMPARE(f.runner.runs, runsBefore);
    QCOMPARE(f.linksDisabledAnnouncements, 0);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.front().front().toString(), NameOf(SimulatorVariant::MSFS2024));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/aerosoft-crj"));
    QCOMPARE(f.session.Profile().id, std::string{"msfs2024"});
}

void OptionsViewModelTest::RemovingAProfileThatIsNotInUseDisablesNothingEvenWhenAsked()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    const int runsBefore = f.runner.runs;

    f.viewModel.RemoveProfile("msfs2020", true);

    QCOMPARE(f.runner.runs, runsBefore);
    QCOMPARE(f.linksDisabledAnnouncements, 0);
    QCOMPARE(removed.count(), 1);
    QVERIFY(f.fileSystem.IsLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(f.session.Profile().id, std::string{"msfs2024"});
}

void OptionsViewModelTest::RemovingTheActiveProfileWithoutDisablingLeavesEveryLinkWhereItIs()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);

    f.viewModel.RemoveProfile("msfs2024", false);

    QCOMPARE(removed.count(), 1);

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
    AddProfile(f, legacy);

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
    AddProfile(f, LegacyProfile());
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
    AddProfile(f, LegacyProfile());
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
    QCOMPARE(f.viewModel.VerificationUsed(), Verification::ByStructure);
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

void OptionsViewModelTest::TheChosenCheckIsWrittenDownAndAnnouncedToWhoeverImportsNext()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QCOMPARE(f.viewModel.VerificationUsed(), Verification::ByStructure);

    const QSignalSpy announced(&f.viewModel, &OptionsViewModel::VerificationChosen);

    f.viewModel.ChooseVerification(Verification::ByHash);

    QCOMPARE(f.settings.stored.verification, Verification::ByHash);
    QCOMPARE(f.viewModel.VerificationUsed(), Verification::ByHash);
    QCOMPARE(announced.count(), 1);
    QCOMPARE(announced.front().front().value<Verification>(), Verification::ByHash);

    f.viewModel.ChooseVerification(Verification::ByHash);

    QCOMPARE(announced.count(), 1);
}

void OptionsViewModelTest::RemovingAProfileWhileDisablingAnnouncesBusyAtTheStartAndAtTheLanding()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    ExpectBusyAtTheStartAndAtTheLanding(f,
                                        [&f]
                                        {
                                            f.viewModel.RemoveProfile("msfs2024", true);
                                        });
}

void OptionsViewModelTest::UnregisteringWhileDisablingAnnouncesBusyAtTheStartAndAtTheLanding()
{
    Fixture f;
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    ExpectBusyAtTheStartAndAtTheLanding(f,
                                        [&f]
                                        {
                                            f.viewModel.UnregisterLibrary("library-1", true);
                                        });
}

void OptionsViewModelTest::RegisteringALibraryAnnouncesBusyAtTheStartAndAtTheLanding()
{
    Fixture f;
    f.fileSystem.AddDirectory(kThirdLibrary);
    f.catalog.SetTree(kThirdLibrary, TreeNode{});
    f.session.ShowActiveProfile();

    ExpectBusyAtTheStartAndAtTheLanding(f,
                                        [&f]
                                        {
                                            f.viewModel.RegisterLibrary(kThirdLibrary);
                                        });
}

void OptionsViewModelTest::AProfileThatCouldNotBeSavedIsNotReportedAsMissing()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.session.ShowActiveProfile();

    const QSignalSpy refused(&f.viewModel, &OptionsViewModel::ProfileNotRemoved);
    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    const QSignalSpy unsaved(&f.notifier, &SessionNotifier::SettingsCouldNotBeSaved);
    const QSignalSpy redraws(&f.viewModel, &OptionsViewModel::Changed);
    f.settings.refusing = true;

    f.viewModel.RemoveProfile("msfs2020", false);

    QCOMPARE(refused.count(), 0);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(unsaved.count(), 1);
    QVERIFY(redraws.count() >= 1);
    QCOMPARE(f.session.Settings().profiles.size(), std::size_t{2});
}

void OptionsViewModelTest::AProfileThatCouldNotBeSavedAfterTheBatchIsNotReportedAsMissing()
{
    Fixture f;
    AddProfile(f, LegacyProfile());
    f.EnableOnDisk(kAddon);
    f.session.ShowActiveProfile();

    const QSignalSpy refused(&f.viewModel, &OptionsViewModel::ProfileNotRemoved);
    const QSignalSpy removed(&f.viewModel, &OptionsViewModel::ProfileRemoved);
    const QSignalSpy unsaved(&f.notifier, &SessionNotifier::SettingsCouldNotBeSaved);
    const QSignalSpy redraws(&f.viewModel, &OptionsViewModel::Changed);
    f.settings.refusing = true;

    f.viewModel.RemoveProfile("msfs2024", true);

    QCOMPARE(f.linksDisabledAnnouncements, 1);
    QCOMPARE(refused.count(), 0);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(unsaved.count(), 1);
    QVERIFY(redraws.count() >= 1);
    QCOMPARE(f.session.Settings().profiles.size(), std::size_t{2});
}

void OptionsViewModelTest::ARegistrationOvertakenBySaveIsRunAgainOnTheProfileAsItIsNow()
{
    Fixture f;
    f.fileSystem.AddDirectory(kExtraLibrary);
    f.fileSystem.AddDirectory(kThirdLibrary);
    f.catalog.SetTree(kExtraLibrary, TreeNode{});
    f.catalog.SetTree(kThirdLibrary, TreeNode{});
    static_cast<void>(f.session.Rewrite(
        [](AppSettings& settings)
        {
            settings.profiles.front().libraries.push_back(
                Library{.id = "library-2", .path = kExtraLibrary, .label = "Extra"});

            return true;
        }));
    f.session.ShowActiveProfile();

    const QSignalSpy registered(&f.viewModel, &OptionsViewModel::LibraryRegistered);
    const int runsBefore = f.runner.runs;
    f.runner.defer = true;

    f.viewModel.RegisterLibrary(kThirdLibrary);
    f.viewModel.UnregisterLibrary("library-2", false);
    f.runner.Finish();

    QCOMPARE(registered.count(), 0);
    QVERIFY(f.viewModel.Busy());

    f.runner.defer = false;
    while (f.runner.Pending())
    {
        f.runner.Finish();
    }

    QCOMPARE(registered.count(), 1);
    QVERIFY(!f.viewModel.Busy());
    QCOMPARE(f.runner.runs, runsBefore + 4);
    QCOMPARE(LibraryPathsStored(f.settings),
             (std::vector<std::string>{ComparablePath(kLibrary), ComparablePath(kThirdLibrary)}));
}

QTEST_MAIN(OptionsViewModelTest)

#include "tst_options_view_model.moc"
