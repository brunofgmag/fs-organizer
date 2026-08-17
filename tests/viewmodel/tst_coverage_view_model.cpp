#include <QtTest/QtTest>

#include <string>
#include <vector>

#include "application/CoverageService.h"
#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePackageList.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/CoverageViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class CoverageViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheCheckIsHandedToTheRunnerInsteadOfRunningWhereItWasAsked();
        static void ATurnAskedWhileAnotherIsRunningWaitsInsteadOfBeingDropped();
        static void StoppingTheCheckBringsBackNothingToAskAbout();
        static void OnlyWhatIsOnAndAlreadyReadIsWhatTheTurnMeets();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");
    const std::filesystem::path kCommunity = PathFromUtf8("E:/Flight Simulator 2024/Community");

    [[nodiscard]] SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.libraries = {{.id = "library-1", .path = kLibrary, .label = "Library"}};
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;

        return profile;
    }

    [[nodiscard]] TreeNode AddonNamed(const std::string& folderName)
    {
        return {.kind = TreeNodeKind::Addon,
                .path = PathUnder(kLibrary, PathFromUtf8(folderName)),
                .addon = Addon{},
                .children = {},
                .declaredAsCategory = false};
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kCommunity);

            catalog.SetTree(
                kLibrary,
                TreeNode{.kind = TreeNodeKind::Category,
                         .path = kLibrary,
                         .addon = {},
                         .children = {AddonNamed("one-eham"), AddonNamed("another-eham"), AddonNamed("third-eham")},
                         .declaredAsCategory = true});

            for (const std::string& name :
                 {std::string("one-eham"), std::string("another-eham"), std::string("third-eham")})
            {
                fileSystem.AddFileWithContents(PathUnder(kLibrary, PathFromUtf8(name)) / "scenery" / "APX.bgl",
                                               FakeSceneryParser::Carrying({"EHAM"}));
            }

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
        FakePackageList packageList;
        CoverageService coverageService{packageList, processProbe, false};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        CoverageViewModel viewModel{coverageService, scenery, session, clock, runner};

        [[nodiscard]] const TreeNode* Addon(const std::string& folderName) const
        {
            return AddonAt(session.Snapshot().libraries, PathUnder(kLibrary, PathFromUtf8(folderName)));
        }

        void TurnOn(const std::string& folderName)
        {
            static_cast<void>(service.SetEnabled(session.Profile(), session.Snapshot(),
                                                 LinkBatch{.toDisable = {}, .toEnable = {Addon(folderName)}}));
            session.RefreshEntries();
        }

        [[nodiscard]] std::vector<SharedAirportsLine> WhatItMeets(const std::string& folderName)
        {
            QSignalSpy answered(&viewModel, &CoverageViewModel::TurningThemOnWasChecked);

            viewModel.CheckWhatWasTurnedOn({Addon(folderName)});

            return answered.isEmpty() ? std::vector<SharedAirportsLine>{}
                                      : answered.last().first().value<WhatTurningThemOnFound>().shared;
        }

        void TurnOff(const std::string& folderName)
        {
            static_cast<void>(service.SetEnabled(session.Profile(), session.Snapshot(),
                                                 LinkBatch{.toDisable = {Addon(folderName)}, .toEnable = {}}));
            session.RefreshEntries();
        }

        void ReadEverySceneryFolder()
        {
            static_cast<void>(
                scenery.SceneryOfEach(SceneryService::AddonsOf(session.Profile(), session.Snapshot()), {}));
        }
    };
}

void CoverageViewModelTest::TheCheckIsHandedToTheRunnerInsteadOfRunningWhereItWasAsked()
{
    Fixture f;
    f.runner.defer = true;
    f.TurnOn("one-eham");

    QSignalSpy answered(&f.viewModel, &CoverageViewModel::TurningThemOnWasChecked);
    const int before = f.runner.runs;

    f.viewModel.CheckWhatWasTurnedOn({f.Addon("another-eham")});

    QCOMPARE(f.runner.runs, before + 1);
    QVERIFY2(answered.isEmpty(),
             "reading the scenery of what was turned on is what used to freeze the window, so the answer cannot be "
             "back before the worker ran");
    QVERIFY(f.viewModel.Checking());

    f.runner.Finish();

    QCOMPARE(answered.count(), 1);
    QVERIFY(!f.viewModel.Checking());
}

void CoverageViewModelTest::ATurnAskedWhileAnotherIsRunningWaitsInsteadOfBeingDropped()
{
    Fixture f;
    f.TurnOn("one-eham");
    f.ReadEverySceneryFolder();
    f.runner.defer = true;

    QSignalSpy answered(&f.viewModel, &CoverageViewModel::TurningThemOnWasChecked);

    f.viewModel.CheckWhatWasTurnedOn({f.Addon("another-eham")});
    f.viewModel.CheckWhatWasTurnedOn({f.Addon("third-eham")});

    QCOMPARE(answered.count(), 0);

    f.runner.Finish();

    QCOMPARE(answered.count(), 1);
    QVERIFY2(f.viewModel.Checking(),
             "the second addon the user turned on while the first was being read is waiting, not gone");

    f.runner.Finish();

    QCOMPARE(answered.count(), 2);
    QVERIFY(!f.viewModel.Checking());

    const auto second = answered.last().first().value<WhatTurningThemOnFound>();

    QCOMPARE(second.shared.size(), std::size_t{1});
    QCOMPARE(second.shared.front().turningOn, QStringLiteral("third-eham"));
}

void CoverageViewModelTest::StoppingTheCheckBringsBackNothingToAskAbout()
{
    Fixture f;
    f.runner.defer = true;
    f.TurnOn("one-eham");

    QSignalSpy answered(&f.viewModel, &CoverageViewModel::TurningThemOnWasChecked);

    f.viewModel.CheckWhatWasTurnedOn({f.Addon("another-eham")});
    f.viewModel.StopChecking();
    f.runner.Finish();

    QCOMPARE(answered.count(), 1);
    QVERIFY2(answered.last().first().value<WhatTurningThemOnFound>().shared.empty(),
             "a check the user stopped raises no dialog, so it comes back with nothing to ask about");
}

void CoverageViewModelTest::OnlyWhatIsOnAndAlreadyReadIsWhatTheTurnMeets()
{
    Fixture f;

    QVERIFY2(f.WhatItMeets("another-eham").empty(),
             "nothing of the library is on, so the addon turning on meets nobody");

    f.TurnOn("one-eham");

    QVERIFY2(f.WhatItMeets("one-eham").empty(),
             "the first one on meets nobody, and it is this gesture that reads its scenery and remembers it: what "
             "the app compares against later is what it has read");

    const std::vector<SharedAirportsLine> shared = f.WhatItMeets("another-eham");

    QCOMPARE(shared.size(), std::size_t{1});
    QCOMPARE(shared.front().turningOn, QStringLiteral("another-eham"));
    QCOMPARE(shared.front().alreadyOn, QStringLiteral("one-eham"));
    QCOMPARE(shared.front().codes, QStringList{QStringLiteral("EHAM")});

    f.TurnOff("one-eham");

    QVERIFY2(f.WhatItMeets("another-eham").empty(),
             "an addon that is installed and off covers nothing, and the question of this gesture is what is on");
}

QTEST_MAIN(CoverageViewModelTest)

#include "tst_coverage_view_model.moc"
