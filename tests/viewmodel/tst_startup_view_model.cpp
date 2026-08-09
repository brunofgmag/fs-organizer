#include <QtTest/QtTest>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
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
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/StartupViewModel.h"

namespace
{
    class StartupViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheScreenShowsWhatTheServiceReadAndStampsWhenItReadIt();
        static void TheLineOfAnAddonThatIsOffNowCarriesTheAlarmOfTheActiveProfile();
        static void WithTheEntriesLeftLooseTheScreenShowsNothingAndNoMomentOfReading();
        static void TakingTheEntriesBackIsWrittenDownAndTheFileIsReadAgain();
        static void LeavingTheEntriesLooseIsWrittenDownAndTheFileStopsBeingRead();
        static void AChoiceTheDiskRefusesLeavesTheOptionWhereItWas();
        static void TurningAnEntryOffRereadsTheFileSoTheLineSaysWhatTheDiskSays();
        static void WithTheSimulatorOpenTheSwitchIsRefusedAndTheProcessIsNamed();
        static void TurningOnAnEntryWhoseProgramIsGoneIsWrittenAndTheAlarmAppears();
    };

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kFlowInTheLibrary = "D:/Library/Utilities/p42-util-flow-pro";
    const std::filesystem::path kFlowExecutable = "E:/Sim/Community/p42-util-flow-pro/bin/flow.exe";
    const std::filesystem::path kSimlink = "C:/Program Files/Navigraph/Simlink/simlink.exe";

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
        TreeNode utilities;
        utilities.kind = TreeNodeKind::Category;
        utilities.path = "D:/Library/Utilities";
        utilities.children = {AddonNode(kFlowInTheLibrary)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(utilities)};

        return library;
    }

    SimulatorProfile Active()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        explicit Fixture(const bool managing = true)
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utilities");
            fileSystem.AddDirectory(kFlowInTheLibrary);
            fileSystem.AddFile(kFlowInTheLibrary / "manifest.json");
            fileSystem.AddFile(kSimlink);

            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Active()};
            settings.stored.activeProfileId = Active().id;
            settings.stored.manageStartupEntries = managing;

            entries.Carry(StartupEntry{.label = "FlowPro", .path = kFlowExecutable, .enabled = true});
            entries.Carry(StartupEntry{.label = "Navigraph Simlink", .path = kSimlink, .enabled = true});

            service.Manage(managing);
            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        ProfileService profiles{catalog, filesystemProbe, sidecars,          classifier, linking,
                                log,     identities,      LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, processProbe, runner, notifier};
        FakeStartupEntries entries;
        StartupService service{entries, processProbe, filesystemProbe, true};
        StartupViewModel viewModel{service, session, settings, clock};
    };
}

void StartupViewModelTest::TheScreenShowsWhatTheServiceReadAndStampsWhenItReadIt()
{
    Fixture fixture;

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QCOMPARE(fixture.viewModel.Lines().front().label, std::string("FlowPro"));
    QCOMPARE(fixture.viewModel.ReadAt(), fixture.clock.now);
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::TheLineOfAnAddonThatIsOffNowCarriesTheAlarmOfTheActiveProfile()
{
    Fixture fixture;
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().front().reach, StartupReach::InsideAnAddon);
    QCOMPARE(fixture.viewModel.Lines().front().alarm, StartupAlarm::TheAddonHoldingItIsOff);
    QCOMPARE(fixture.viewModel.Lines().front().addonFolder, kDestination / "p42-util-flow-pro");
    QCOMPARE(fixture.viewModel.Lines().back().alarm, StartupAlarm::None);
}

void StartupViewModelTest::WithTheEntriesLeftLooseTheScreenShowsNothingAndNoMomentOfReading()
{
    Fixture fixture(false);

    fixture.viewModel.Show();

    QVERIFY(!fixture.viewModel.Managing());
    QVERIFY(fixture.viewModel.Lines().empty());
    QVERIFY(!fixture.viewModel.ReadAt().has_value());
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupViewModelTest::TakingTheEntriesBackIsWrittenDownAndTheFileIsReadAgain()
{
    Fixture fixture(false);
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);
    fixture.viewModel.Manage(true);

    QVERIFY(fixture.viewModel.Managing());
    QVERIFY(fixture.settings.stored.manageStartupEntries);
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
    QVERIFY(fixture.viewModel.ReadAt().has_value());
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::LeavingTheEntriesLooseIsWrittenDownAndTheFileStopsBeingRead()
{
    Fixture fixture;
    fixture.viewModel.Show();

    fixture.entries.reads = 0;
    fixture.viewModel.Manage(false);

    QVERIFY(!fixture.viewModel.Managing());
    QVERIFY(!fixture.settings.stored.manageStartupEntries);
    QVERIFY(fixture.viewModel.Lines().empty());
    QVERIFY(!fixture.viewModel.ReadAt().has_value());
    QCOMPARE(fixture.entries.reads, std::size_t{0});
}

void StartupViewModelTest::AChoiceTheDiskRefusesLeavesTheOptionWhereItWas()
{
    Fixture fixture;
    fixture.viewModel.Show();
    fixture.settings.refusing = true;

    const QSignalSpy refused(&fixture.viewModel, &StartupViewModel::SettingsCouldNotBeSaved);
    fixture.viewModel.Manage(false);

    QCOMPARE(refused.count(), 1);
    QVERIFY(fixture.viewModel.Managing());
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
}

void StartupViewModelTest::TurningAnEntryOffRereadsTheFileSoTheLineSaysWhatTheDiskSays()
{
    Fixture fixture;
    fixture.viewModel.Show();

    const QSignalSpy changed(&fixture.viewModel, &StartupViewModel::Changed);

    QCOMPARE(fixture.viewModel.Switch(kFlowExecutable, false), FileResult::Completed);
    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QVERIFY(!fixture.viewModel.Lines().front().enabled);
    QCOMPARE(fixture.viewModel.Lines().front().alarm, StartupAlarm::None);
    QCOMPARE(changed.count(), 1);
}

void StartupViewModelTest::WithTheSimulatorOpenTheSwitchIsRefusedAndTheProcessIsNamed()
{
    Fixture fixture;
    fixture.viewModel.Show();
    fixture.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(fixture.viewModel.Switch(kFlowExecutable, false), FileResult::TheSimulatorIsRunning);
    QCOMPARE(fixture.entries.writes, std::size_t{0});
    QCOMPARE(fixture.viewModel.RunningSimulator(), std::optional<std::string>("FlightSimulator2024.exe"));
    QCOMPARE(fixture.viewModel.Lines().size(), std::size_t{2});
}

void StartupViewModelTest::TurningOnAnEntryWhoseProgramIsGoneIsWrittenAndTheAlarmAppears()
{
    Fixture fixture;
    fixture.entries.Carry(StartupEntry{.label = "Ghost", .path = "C:/Program Files/Ghost/ghost.exe", .enabled = false});
    fixture.viewModel.Show();

    QCOMPARE(fixture.viewModel.Lines().back().alarm, StartupAlarm::None);
    QCOMPARE(fixture.viewModel.Switch("C:/Program Files/Ghost/ghost.exe", true), FileResult::Completed);

    QCOMPARE(fixture.entries.writes, std::size_t{1});
    QVERIFY(fixture.viewModel.Lines().back().enabled);
    QCOMPARE(fixture.viewModel.Lines().back().alarm, StartupAlarm::TheExecutableIsMissing);
}

QTEST_MAIN(StartupViewModelTest)

#include "tst_startup_view_model.moc"
