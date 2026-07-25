#include <QtTest/QtTest>

#include "domain/importing/ImportEngine.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"

class ImportEngineTest : public QObject
{
    Q_OBJECT

private slots:
    static void AnImportThatDoesNotFitOnTheTargetVolumeTouchesNothing();
    static void AVolumeThatCannotReportItsFreeSpaceIsNotTheSameAsAFullOne();
    static void CancellingTheCopyRemovesTheStagingAndLeavesTheSourceIntact();
    static void ACopyThatFailsKeepsItsStagingForTheResumeToFind();
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
        ImportEngine engine{filesystemProbe, files};

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

QTEST_APPLESS_MAIN(ImportEngineTest)

#include "tst_import_engine.moc"
