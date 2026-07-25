#include <QtTest/QtTest>

#include "domain/importing/ImportEngine.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

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
    static void AFolderOutsideTheConfiguredDestinationsIsNeverImported();
    static void ADestinationRootIsNotAFolderInsideItself();
    static void AForeignLinkIsNeverImportedAsIfItWereAFolder();
    static void AFinishedImportLeavesALinkWhereTheFolderUsedToBe();
};

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kSource = "E:/Sim/Community/flybywire-externaltools-simbridge";
    const std::filesystem::path kTarget = "D:/Library/Utils/flybywire-externaltools-simbridge";
    const std::filesystem::path kStaging =
        "D:/Library/Utils/flybywire-externaltools-simbridge.fsorg-partial";

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeLinkService linkService{fileSystem};
        LinkingEngine linking{linkService, filesystemProbe};
        ImportEngine engine{filesystemProbe, files, linking, LinkType::Junction};

        SimulatorProfile profile{.destinations = {"E:/Sim/Community"},
                                 .defaultDestination = "E:/Sim/Community"};

        ImportRequest request{kSource, kTarget};

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
            [&] {
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

    QCOMPARE(outcome.Result(), ImportResult::NotEnoughFreeSpace);
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

    QCOMPARE(outcome.Result(), ImportResult::CouldNotCheckFreeSpace);
    QVERIFY(!f.fileSystem.Exists(kTarget));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::CancellingTheCopyRemovesTheStagingAndLeavesTheSourceIntact()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    int reports = 0;
    const ImportOutcome outcome =
        f.engine.Import(f.profile, f.request,
                        [&reports](const CopyProgress&)
                        {
                            ++reports;
                            return reports < 2;
                        });

    QCOMPARE(outcome.Result(), ImportResult::Cancelled);
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

    QCOMPARE(outcome.Result(), ImportResult::CouldNotCopy);
    QVERIFY(f.fileSystem.Exists(kStaging));
    QVERIFY(!f.fileSystem.Exists(kTarget));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::AFinishedImportLandsTheAddonInTheLibraryAndTakesThePhysicalCopyAway()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), ImportResult::Completed);
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

    QCOMPARE(outcome.Result(), ImportResult::VerificationFailed);
    f.VerifySimBridgeIsStillWhereItWas();
    QVERIFY(!f.fileSystem.Exists(kTarget));
    QVERIFY(f.fileSystem.Exists(kStaging));
}

void ImportEngineTest::AFolderOutsideTheConfiguredDestinationsIsNeverImported()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Packages/orbx-central");
    f.fileSystem.AddFile("E:/Packages/orbx-central/manifest.json", 2 * kMegabyte);
    f.fileSystem.AddDirectory("D:/Library/Utils");

    const ImportRequest request{"E:/Packages/orbx-central", "D:/Library/Utils/orbx-central"};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), ImportResult::SourceIsNotUnderADestination);
    QVERIFY(f.fileSystem.Exists("E:/Packages/orbx-central/manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/orbx-central"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/orbx-central.fsorg-partial"));
}

void ImportEngineTest::ADestinationRootIsNotAFolderInsideItself()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportRequest request{"E:/Sim/Community", "D:/Library/Utils/Community"};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), ImportResult::SourceIsNotUnderADestination);
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community"));
    f.VerifySimBridgeIsStillWhereItWas();
}

void ImportEngineTest::AForeignLinkIsNeverImportedAsIfItWereAFolder()
{
    const std::filesystem::path foreign =
        "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory(foreign);
    f.fileSystem.AddFile(foreign / "manifest.json", 2 * kMegabyte);
    f.fileSystem.AddLink("E:/Sim/Community/fsdreamteam-gsx-pro", foreign);
    f.fileSystem.AddDirectory("D:/Library/Utils");

    const ImportRequest request{"E:/Sim/Community/fsdreamteam-gsx-pro",
                                "D:/Library/Utils/fsdreamteam-gsx-pro"};

    const ImportOutcome outcome = f.engine.Import(f.profile, request, {});

    QCOMPARE(outcome.Result(), ImportResult::SourceIsAReparsePoint);
    QVERIFY(f.fileSystem.IsLink("E:/Sim/Community/fsdreamteam-gsx-pro"));
    QVERIFY(f.fileSystem.Exists(foreign / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/fsdreamteam-gsx-pro"));
}

void ImportEngineTest::AFinishedImportLeavesALinkWhereTheFolderUsedToBe()
{
    Fixture f;
    f.AddSimBridgeToTheDestination();

    const ImportOutcome outcome = f.engine.Import(f.profile, f.request, {});

    QCOMPARE(outcome.Result(), ImportResult::Completed);
    QVERIFY(f.fileSystem.IsLink(kSource));
    QCOMPARE(f.fileSystem.LinkTarget(kSource).value(), kTarget);
}

QTEST_APPLESS_MAIN(ImportEngineTest)

#include "tst_import_engine.moc"
