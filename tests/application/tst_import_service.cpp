#include <QtTest/QtTest>

#include "application/ImportService.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class ImportServiceTest : public QObject
{
    Q_OBJECT

private slots:
    static void NoFileIsTouchedWhileTheSimulatorIsRunning();
    static void ResolvingAConflictSendsTheLoserToQuarantineAndDeletesNothing();
    static void KeepingTheDestinationCopySendsTheLibraryCopyToItsOwnQuarantine();
    static void QuarantiningTheDestinationCopyIsJournalledAlongWithTheLinkThatReplacesIt();
    static void QuarantiningTheLibraryCopyIsJournalledOnItsOwn();
    static void ARefusedBatchLeavesNothingInTheJournal();
};

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kInDestination = "E:/Sim/Community/simbridge";
    const std::filesystem::path kInLibrary = "D:/Library/Utils/simbridge";

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeProcessProbe processProbe;
        LinkingEngine linking{linkService, filesystemProbe};
        FakeOperationJournal journal;
        FakeClock clock;
        ImportEngine engine{filesystemProbe, files, linking, journal, clock, LinkType::Junction};
        ImportService service{engine, processProbe, files, linking, journal, clock, LinkType::Junction};

        SimulatorProfile profile{
            .destinations = {kDestination},
            .defaultDestination = kDestination,
            .libraries = {Library{"lib-1", kLibrary}}
        };

        void AddBothCopies()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kInDestination);
            fileSystem.AddFile(kInDestination / "manifest.json", 2 * kMegabyte);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utils");
            fileSystem.AddDirectory(kInLibrary);
            fileSystem.AddFile(kInLibrary / "manifest.json", 1 * kMegabyte);
        }
    };
}

void ImportServiceTest::NoFileIsTouchedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.AddBothCopies();
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile, {ImportRequest{kInDestination, "D:/Library/Utils/imported"}}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, ImportResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/imported"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/imported.fsorg-partial"));
}

void ImportServiceTest::ResolvingAConflictSendsTheLoserToQuarantineAndDeletesNothing()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const ImportResult result =
        f.service.ResolveConflict(f.profile, conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, ImportResult::Completed);
    QVERIFY(f.fileSystem.Exists("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.fileSystem.FileSize("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"),
             2 * kMegabyte);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kInDestination));
    QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
}

void ImportServiceTest::KeepingTheDestinationCopySendsTheLibraryCopyToItsOwnQuarantine()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const ImportResult result =
        f.service.ResolveConflict(f.profile, conflict, ConflictChoice::KeepTheDestinationCopy);

    QCOMPARE(result, ImportResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.fileSystem.FileSize("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"),
             1 * kMegabyte);
    QVERIFY(f.fileSystem.IsDirectory(kInDestination));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
}

void ImportServiceTest::QuarantiningTheDestinationCopyIsJournalledAlongWithTheLinkThatReplacesIt()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, conflict, ConflictChoice::KeepTheLibraryCopy),
             ImportResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{2});

    const OperationRecord& quarantine = f.journal.appended[0];
    QCOMPARE(quarantine.kind, OperationKind::QuarantineFromDestination);
    QCOMPARE(quarantine.importResult, ImportResult::Completed);
    QCOMPARE(quarantine.timestamp, f.clock.now);
    QCOMPARE(quarantine.addonId.libraryId, LibraryId{"lib-1"});
    QCOMPARE(quarantine.addonId.folderName, std::string{"simbridge"});
    QCOMPARE(quarantine.source, kInDestination);
    QCOMPARE(quarantine.target, std::filesystem::path{"E:/Sim/_fsorganizer-quarantine/simbridge"});

    QCOMPARE(f.journal.appended[1].kind, OperationKind::EnableAddon);
    QCOMPARE(f.journal.appended[1].failure, LinkFailure::None);
    QCOMPARE(f.journal.appended[1].source, kInLibrary);
    QCOMPARE(f.journal.appended[1].target, kInDestination);
}

void ImportServiceTest::QuarantiningTheLibraryCopyIsJournalledOnItsOwn()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, conflict, ConflictChoice::KeepTheDestinationCopy),
             ImportResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended[0].kind, OperationKind::QuarantineFromLibrary);
    QCOMPARE(f.journal.appended[0].importResult, ImportResult::Completed);
    QCOMPARE(f.journal.appended[0].source, kInLibrary);
    QCOMPARE(f.journal.appended[0].target,
             std::filesystem::path{"D:/Library/_fsorganizer-quarantine/simbridge"});
}

void ImportServiceTest::ARefusedBatchLeavesNothingInTheJournal()
{
    Fixture f;
    f.AddBothCopies();
    f.processProbe.ReportTheSimulatorAsRunning();

    static_cast<void>(
        f.service.Import(f.profile, {ImportRequest{kInDestination, "D:/Library/Utils/imported"}}, {}));
    static_cast<void>(f.service.ResolveConflict(f.profile, CopyConflict{kInDestination, kInLibrary},
                                                ConflictChoice::KeepTheLibraryCopy));

    QVERIFY(f.journal.appended.empty());
}

QTEST_APPLESS_MAIN(ImportServiceTest)

#include "tst_import_service.moc"
