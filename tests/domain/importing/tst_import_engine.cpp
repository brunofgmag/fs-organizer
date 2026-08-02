#include <QtTest/QtTest>

#include <variant>

#include "domain/journal/OperationLog.h"
#include "domain/importing/ImportEngine.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class ImportEngineTest : public QObject
{
    class ImportEngineTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnImportThatDoesNotFitOnTheTargetVolumeTouchesNothing();
        static void AVolumeThatCannotReportItsFreeSpaceIsNotTheSameAsAFullOne();
        static void CancellingTheCopyRemovesTheStagingAndLeavesTheSourceIntact();
        static void ACopyThatFailsKeepsItsStagingForTheResumeToFind();
        static void AFinishedImportLandsTheAddonInTheLibraryAndTakesThePhysicalCopyAway();
        static void AnImportWhoseVerificationFailsLeavesTheSourceWhereItIs();
        static void ASourceThatCannotBeWalkedStopsTheImportBeforeAnythingIsCopied();
        static void AnEmptySourceIsNotVerifiedAgainstAStagingThatCannotBeWalked();
        static void AFolderOutsideTheConfiguredDestinationsIsNeverImported();
        static void ADestinationRootIsNotAFolderInsideItself();
        static void AForeignLinkIsNeverImportedAsIfItWereAFolder();
        static void AFinishedImportLeavesALinkWhereTheFolderUsedToBe();
        static void EveryStepOfAFinishedImportReachesTheJournalInOrder();
        static void AVerificationThatFailsSaysSoInsteadOfLeavingTheJournalSilent();
        static void ACopyThatFailsIsTheLastThingTheJournalHears();
        static void AMoveThatFailsIsRecordedAndTheSourceSurvives();
        static void ARemovalThatFailsIsRecordedAndTheSourceSurvives();
        static void ALinkThatFailsIsRecordedAgainstTheAddonItWouldHaveEnabled();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kSource = "E:/Sim/Community/flybywire-externaltools-simbridge";
    const std::filesystem::path kCategory = "D:/Library/Utils";
    const std::filesystem::path kTarget = "D:/Library/Utils/flybywire-externaltools-simbridge";
    const std::filesystem::path kStaging = "D:/Library/Utils/flybywire-externaltools-simbridge.fsorg-partial";

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeLinkService linkService{fileSystem};
        LinkingEngine linking{linkService, filesystemProbe};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};

        SimulatorProfile profile{.destinations = {"E:/Sim/Community"},
                                 .defaultDestination = "E:/Sim/Community",
                                 .libraries = {Library{"lib-1", "D:/Library"}}};

        ImportRequest request{kSource, kCategory};

        void AddSimBridgeToTheDestination()
        {
            fileSystem.AddDirectory("E:/Sim/Community");
            fileSystem.AddDirectory(kSource);
            fileSystem.AddFile(kSource / "manifest.json", 2 * kMegabyte);
            fileSystem.AddDirectory(kSource / "dist");
            fileSystem.AddFile(kSource / "dist/simbridge.exe", 400 * kMegabyte);
            fileSystem.AddDirectory("D:/Library/Utils");
        }

        void VerifySimBridgeIsStillWhereItWas() const
        {
            [&]
            {
                QVERIFY(fileSystem.Exists(kSource));
                QVERIFY(fileSystem.Exists(kSource / "manifest.json"));
                QVERIFY(fileSystem.Exists(kSource / "dist/simbridge.exe"));
            }();
        }
    };
}

void ImportEngineTest::AnImportThatDoesNotFitOnTheTargetVolumeTouchesNothing()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.fileSystem.SetFreeSpace("D:/Library", 100 * kMegabyte);

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::NotEnoughFreeSpace);
    QVERIFY(!f.fileSystem.Exists(kTarget));
    QVERIFY(!f.fileSystem.Exists(kStaging));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::AVolumeThatCannotReportItsFreeSpaceIsNotTheSameAsAFullOne()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.fileSystem.MarkFreeSpaceUnknown("D:/Library");

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::CouldNotCheckFreeSpace);
    QVERIFY(!f.fileSystem.Exists(kTarget));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::CancellingTheCopyRemovesTheStagingAndLeavesTheSourceIntact()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    int reports = 0;
    const ImportOutcome outcome = f.engine.Import(f.profile, f.request,
                                                  [&reports](const CopyProgress&)
                                                  {
                                                      ++reports;
                                                      return reports < 2;
                                                  });

    QCOMPARE(outcome.Result(), FileResult::Cancelled);
    QVERIFY(!f.fileSystem.Exists(kStaging));
    QVERIFY(!f.fileSystem.Exists(kTarget));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::ACopyThatFailsKeepsItsStagingForTheResumeToFind()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheCopyFailPartWayThrough();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::CouldNotCopy);
    QVERIFY(f.fileSystem.Exists(kStaging));
    QVERIFY(!f.fileSystem.Exists(kTarget));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::AFinishedImportLandsTheAddonInTheLibraryAndTakesThePhysicalCopyAway()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(kTarget / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kTarget / "dist/simbridge.exe"));
    QCOMPARE(f.fileSystem.FileSize(kTarget / "dist/simbridge.exe"), 400 * kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kStaging));
    QVERIFY(!f.fileSystem.IsDirectory(kSource));
    QVERIFY(!f.fileSystem.Exists(kSource / "dist/simbridge.exe"));
}

void ImportEngineTest::AnImportWhoseVerificationFailsLeavesTheSourceWhereItIs()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheCopyDropAFile();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::VerificationFailed);
    f.VerifySimBridgeIsStillWhereItWas();
    QVERIFY(!f.fileSystem.Exists(kTarget));
    QVERIFY(f.fileSystem.Exists(kStaging));
}

void ImportEngineTest::ASourceThatCannotBeWalkedStopsTheImportBeforeAnythingIsCopied()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.filesystemProbe.RefuseToWalk(kSource);

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::CouldNotReadTheSource);
    f.VerifySimBridgeIsStillWhereItWas();
    QVERIFY(!f.fileSystem.Exists(kStaging));
    QVERIFY(!f.fileSystem.Exists(kTarget));
}

void ImportEngineTest::AnEmptySourceIsNotVerifiedAgainstAStagingThatCannotBeWalked()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory(kSource);
    f.fileSystem.AddDirectory("D:/Library/Utils");
    f.filesystemProbe.RefuseToWalk(kStaging);

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::VerificationFailed);
    QVERIFY(f.fileSystem.Exists(kSource));
    QVERIFY(!f.fileSystem.Exists(kTarget));
}

void ImportEngineTest::AFolderOutsideTheConfiguredDestinationsIsNeverImported()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Packages/orbx-central");
    f.fileSystem.AddFile("E:/Packages/orbx-central/manifest.json", 2 * kMegabyte);
    f.fileSystem.AddDirectory("D:/Library/Utils");

    const ImportRequest request{"E:/Packages/orbx-central", kCategory};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), FileResult::SourceIsNotUnderADestination);
    QVERIFY(f.fileSystem.Exists("E:/Packages/orbx-central/manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/orbx-central"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/orbx-central.fsorg-partial"));
}

void ImportEngineTest::ADestinationRootIsNotAFolderInsideItself()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportRequest request{"E:/Sim/Community", kCategory};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), FileResult::SourceIsNotUnderADestination);
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community"));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::AForeignLinkIsNeverImportedAsIfItWereAFolder()
{
    const std::filesystem::path foreign = "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory(foreign);
    f.fileSystem.AddFile(foreign / "manifest.json", 2 * kMegabyte);
    f.fileSystem.AddLink("E:/Sim/Community/fsdreamteam-gsx-pro", foreign);
    f.fileSystem.AddDirectory("D:/Library/Utils");

    const ImportRequest request{"E:/Sim/Community/fsdreamteam-gsx-pro", kCategory};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), FileResult::SourceIsAReparsePoint);
    QVERIFY(f.fileSystem.IsLink("E:/Sim/Community/fsdreamteam-gsx-pro"));
    QVERIFY(f.fileSystem.Exists(foreign / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/fsdreamteam-gsx-pro"));
}

void ImportEngineTest::AFinishedImportLeavesALinkWhereTheFolderUsedToBe()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), FileResult::Completed);
    QVERIFY(f.fileSystem.IsLink(kSource));
    QCOMPARE(f.fileSystem.LinkTarget(kSource).value(), kTarget);
}

void ImportEngineTest::EveryStepOfAFinishedImportReachesTheJournalInOrder()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{5});

    const std::vector kinds{OperationKind::ImportCopyToStaging, OperationKind::ImportVerifyStaging,
                            OperationKind::ImportMoveIntoPlace, OperationKind::ImportRemoveSource,
                            OperationKind::EnableAddon};

    for (std::size_t step = 0; step < kinds.size(); ++step)
    {
        const OperationRecord& record = f.journal.appended[step];
        QCOMPARE(record.kind, kinds[step]);
        QCOMPARE(record.timestamp, f.clock.now);
        QCOMPARE(record.addonId.libraryId, LibraryId{"lib-1"});
        QCOMPARE(record.addonId.folderName, std::string{"flybywire-externaltools-simbridge"});
    }

    QCOMPARE(f.journal.appended[0].source, kSource);
    QCOMPARE(f.journal.appended[0].target, kStaging);
    QCOMPARE(std::get<FileResult>(f.journal.appended[0].outcome), FileResult::Completed);

    QCOMPARE(f.journal.appended[1].source, kStaging);
    QCOMPARE(f.journal.appended[1].target, kTarget);
    QCOMPARE(std::get<FileResult>(f.journal.appended[1].outcome), FileResult::Completed);

    QCOMPARE(f.journal.appended[2].source, kStaging);
    QCOMPARE(f.journal.appended[2].target, kTarget);

    QCOMPARE(f.journal.appended[3].source, kSource);
    QCOMPARE(f.journal.appended[3].target, kTarget);

    QCOMPARE(f.journal.appended[4].source, kTarget);
    QCOMPARE(f.journal.appended[4].target, kSource);
    QCOMPARE(std::get<LinkFailure>(f.journal.appended[4].outcome), LinkFailure::None);
}

void ImportEngineTest::AVerificationThatFailsSaysSoInsteadOfLeavingTheJournalSilent()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheCopyDropAFile();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::VerificationFailed);

    QCOMPARE(f.journal.appended.size(), std::size_t{2});
    QCOMPARE(f.journal.appended[1].kind, OperationKind::ImportVerifyStaging);
    QCOMPARE(std::get<FileResult>(f.journal.appended[1].outcome), FileResult::VerificationFailed);
    QCOMPARE(f.journal.appended[1].source, kStaging);
    QCOMPARE(f.journal.appended[1].target, kTarget);
}

void ImportEngineTest::ACopyThatFailsIsTheLastThingTheJournalHears()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheCopyFailPartWayThrough();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::CouldNotCopy);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended[0].kind, OperationKind::ImportCopyToStaging);
    QCOMPARE(std::get<FileResult>(f.journal.appended[0].outcome), FileResult::CouldNotCopy);
    QCOMPARE(f.journal.appended[0].source, kSource);
    QCOMPARE(f.journal.appended[0].target, kStaging);
}

void ImportEngineTest::AMoveThatFailsIsRecordedAndTheSourceSurvives()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheMoveFail();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::CouldNotMoveIntoPlace);

    QCOMPARE(f.journal.appended.size(), std::size_t{3});
    QCOMPARE(f.journal.appended[2].kind, OperationKind::ImportMoveIntoPlace);
    QCOMPARE(std::get<FileResult>(f.journal.appended[2].outcome), FileResult::CouldNotMoveIntoPlace);
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::ARemovalThatFailsIsRecordedAndTheSourceSurvives()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.files.MakeTheRemovalFail();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::CouldNotRemoveSource);

    QCOMPARE(f.journal.appended.size(), std::size_t{4});
    QCOMPARE(f.journal.appended[3].kind, OperationKind::ImportRemoveSource);
    QCOMPARE(std::get<FileResult>(f.journal.appended[3].outcome), FileResult::CouldNotRemoveSource);
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::ALinkThatFailsIsRecordedAgainstTheAddonItWouldHaveEnabled()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();
    f.linkService.MakeLinkCreationFail();

    QCOMPARE(f.engine.Import(f.profile, f.request, {}).Result(), FileResult::CouldNotCreateLink);

    QCOMPARE(f.journal.appended.size(), std::size_t{5});
    QCOMPARE(f.journal.appended[4].kind, OperationKind::EnableAddon);
    QCOMPARE(std::get<LinkFailure>(f.journal.appended[4].outcome), LinkFailure::CouldNotCreateLink);
    QCOMPARE(f.journal.appended[4].source, kTarget);
    QCOMPARE(f.journal.appended[4].target, kSource);
}

QTEST_APPLESS_MAIN(ImportEngineTest)

#include "tst_import_engine.moc"
