#include <QtTest/QtTest>

#include <variant>

#include "domain/journal/OperationLog.h"
#include "application/ImportService.h"
#include "domain/linking/EntryClassifier.h"
#include "tests/doubles/FakeCatalogScanner.h"
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
    static void TheQuarantineIsListedFromTheDiskAndItsOriginComesFromTheJournal();
    static void RestoringPutsTheFolderBackWhereItCameFromAndSaysSoInTheJournal();
    static void RestoringIsRefusedWhenSomethingElseAlreadyOccupiesTheOrigin();
    static void AnItemWhoseOriginTheJournalNeverSawIsNotRestored();
    static void EmptyingTheQuarantineNeverReachesAnythingOutsideIt();
    static void NoQuarantineIsTouchedWhileTheSimulatorIsRunning();
    static void LeftoverStagingIsFoundInTheLibraryWithTheSourceItCameFrom();
    static void TheSearchForLeftoversNeverWalksIntoAnAddon();
    static void ResumingThrowsTheHalfCopyAwayAndImportsAgainFromTheSource();
    static void DiscardingALeftoverRemovesOnlyTheStaging();
    static void KeepingTheDestinationCopyUnlinksTheLibraryCopyBeforeQuarantiningIt();
    static void NothingIsMovedWhenALinkToTheLibraryCopyCannotBeRemoved();
    static void ImportingIsRefusedWhenTheBaseNameAlreadyExistsInTheLibrary();
    static void ResumingIsRefusedWhenTheBaseNameAppearedWhileTheImportWasLost();
    static void RestoringIsRefusedWhenTheBaseNameTookTheIdentityElsewhere();
    static void TheIdentityIsOnlyTakenInsideTheLibraryThatHoldsIt();
};

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kInDestination = "E:/Sim/Community/simbridge";
    const std::filesystem::path kInLibrary = "D:/Library/Utils/simbridge";

    const std::filesystem::path kOtherDestination = "E:/Sim/Community2024";
    const std::filesystem::path kLinkedElsewhere = "E:/Sim/Community2024/simbridge";

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeProcessProbe processProbe;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeCatalogScanner catalog;
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};
        ImportService service{engine, processProbe, filesystemProbe, catalog, files, linking, log, LinkType::Junction};

        SimulatorProfile profile{.destinations = {kDestination},
                                 .defaultDestination = kDestination,
                                 .libraries = {Library{"lib-1", kLibrary}}};

        [[nodiscard]] std::vector<DestinationEntry> Entries() const
        {
            return classifier.Resolve(profile.destinations, {kLibrary});
        }

        void AddALinkToTheLibraryCopyInAnotherDestination()
        {
            profile.destinations.push_back(kOtherDestination);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddLink(kLinkedElsewhere, kInLibrary);
        }

        void TheLibraryHolds(const std::vector<std::filesystem::path>& addons)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;

            for (const std::filesystem::path& addon : addons)
            {
                TreeNode node;
                node.kind = TreeNodeKind::Addon;
                node.path = addon;
                node.addon = Addon{addon, Manifest{}};

                library.children.push_back(std::move(node));
            }

            catalog.SetTree(kLibrary, std::move(library));
        }

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
        f.service.Import(f.profile, {ImportRequest{kInDestination, "D:/Library/Utils"}}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/imported"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/imported.fsorg-partial"));
}

void ImportServiceTest::ResolvingAConflictSendsTheLoserToQuarantineAndDeletesNothing()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.fileSystem.FileSize("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"), 2 * kMegabyte);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kInDestination));
    QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
}

void ImportServiceTest::KeepingTheDestinationCopySendsTheLibraryCopyToItsOwnQuarantine()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheDestinationCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.fileSystem.FileSize("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"), 1 * kMegabyte);
    QVERIFY(f.fileSystem.IsDirectory(kInDestination));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
}

void ImportServiceTest::QuarantiningTheDestinationCopyIsJournalledAlongWithTheLinkThatReplacesIt()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{2});

    const OperationRecord& quarantine = f.journal.appended[0];
    QCOMPARE(quarantine.kind, OperationKind::QuarantineFromDestination);
    QCOMPARE(std::get<FileResult>(quarantine.outcome), FileResult::Completed);
    QCOMPARE(quarantine.timestamp, f.clock.now);
    QCOMPARE(quarantine.addonId.libraryId, LibraryId{"lib-1"});
    QCOMPARE(quarantine.addonId.folderName, std::string{"simbridge"});
    QCOMPARE(quarantine.source, kInDestination);
    QCOMPARE(quarantine.target, std::filesystem::path{"E:/Sim/_fsorganizer-quarantine/simbridge"});

    QCOMPARE(f.journal.appended[1].kind, OperationKind::EnableAddon);
    QCOMPARE(std::get<LinkFailure>(f.journal.appended[1].outcome), LinkFailure::None);
    QCOMPARE(f.journal.appended[1].source, kInLibrary);
    QCOMPARE(f.journal.appended[1].target, kInDestination);
}

void ImportServiceTest::QuarantiningTheLibraryCopyIsJournalledOnItsOwn()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{kInDestination, kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheDestinationCopy),
             FileResult::Completed);

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended[0].kind, OperationKind::QuarantineFromLibrary);
    QCOMPARE(std::get<FileResult>(f.journal.appended[0].outcome), FileResult::Completed);
    QCOMPARE(f.journal.appended[0].source, kInLibrary);
    QCOMPARE(f.journal.appended[0].target, std::filesystem::path{"D:/Library/_fsorganizer-quarantine/simbridge"});
}

void ImportServiceTest::ARefusedBatchLeavesNothingInTheJournal()
{
    Fixture f;
    f.AddBothCopies();
    f.processProbe.ReportTheSimulatorAsRunning();

    static_cast<void>(f.service.Import(f.profile, {ImportRequest{kInDestination, "D:/Library/Utils"}}, {}));
    static_cast<void>(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                                ConflictChoice::KeepTheLibraryCopy));

    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::TheQuarantineIsListedFromTheDiskAndItsOriginComesFromTheJournal()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().path, std::filesystem::path{"E:/Sim/_fsorganizer-quarantine/simbridge"});
    QCOMPARE(items.front().origin, kInDestination);
    QVERIFY(items.front().KnowsWhereItCameFrom());
    QCOMPARE(items.front().quarantinedAt.value(), f.clock.now);
}

void ImportServiceTest::RestoringPutsTheFolderBackWhereItCameFromAndSaysSoInTheJournal()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheDestinationCopy),
             FileResult::Completed);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);
    QCOMPARE(items.size(), std::size_t{1});

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, items);

    QCOMPARE(restored.size(), std::size_t{1});
    QCOMPARE(restored.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge"));
    QVERIFY(f.service.Quarantined(f.profile).empty());

    const OperationRecord& record = f.journal.appended.back();
    QCOMPARE(record.kind, OperationKind::RestoreFromQuarantine);
    QCOMPARE(std::get<FileResult>(record.outcome), FileResult::Completed);
    QCOMPARE(record.source, std::filesystem::path{"D:/Library/_fsorganizer-quarantine/simbridge"});
    QCOMPARE(record.target, kInLibrary);
    QCOMPARE(record.addonId.libraryId, LibraryId{"lib-1"});
}

void ImportServiceTest::RestoringIsRefusedWhenSomethingElseAlreadyOccupiesTheOrigin()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheDestinationCopy),
             FileResult::Completed);

    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(restored.front().result, FileResult::CouldNotRestore);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 3 * kMegabyte);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
}

void ImportServiceTest::AnItemWhoseOriginTheJournalNeverSawIsNotRestored()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine");
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine/orphan");
    f.fileSystem.AddFile("D:/Library/_fsorganizer-quarantine/orphan/manifest.json", 1 * kMegabyte);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);
    QCOMPARE(items.size(), std::size_t{1});
    QVERIFY(!items.front().KnowsWhereItCameFrom());

    QCOMPARE(f.service.Restore(f.profile, items).front().result, FileResult::TheOriginIsUnknown);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/orphan/manifest.json"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::EmptyingTheQuarantineNeverReachesAnythingOutsideIt()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine");
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine/old-simbridge");
    f.fileSystem.AddFile("D:/Library/_fsorganizer-quarantine/old-simbridge/manifest.json", kMegabyte);

    const std::vector<FileOperationResult> discarded = f.service.Discard(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(discarded.size(), std::size_t{1});
    QCOMPARE(discarded.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/old-simbridge"));
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));

    QCOMPARE(f.journal.appended.back().kind, OperationKind::DiscardFromQuarantine);

    const std::vector<FileOperationResult> refused =
        f.service.Discard(f.profile, {QuarantinedItem{kInLibrary, {}, std::nullopt}});

    QCOMPARE(refused.front().result, FileResult::CouldNotDiscard);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
}

void ImportServiceTest::NoQuarantineIsTouchedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine");
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine/old-simbridge");
    f.fileSystem.AddFile("D:/Library/_fsorganizer-quarantine/old-simbridge/manifest.json", kMegabyte);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);
    f.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.service.Restore(f.profile, items).front().result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.service.Discard(f.profile, items).front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/old-simbridge/manifest.json"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::LeftoverStagingIsFoundInTheLibraryWithTheSourceItCameFrom()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Utils/imported.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Utils/imported.fsorg-partial/manifest.json", kMegabyte);

    f.journal.Append(OperationRecord::OfImport(f.clock.now, OperationKind::ImportCopyToStaging,
                                               AddonId{"lib-1", "imported"}, "E:/Sim/Community/imported",
                                               "D:/Library/Utils/imported.fsorg-partial", FileResult::Completed));

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, std::filesystem::path{"D:/Library/Utils/imported.fsorg-partial"});
    QCOMPARE(leftovers.front().target, std::filesystem::path{"D:/Library/Utils/imported"});
    QCOMPARE(leftovers.front().source, std::filesystem::path{"E:/Sim/Community/imported"});
    QVERIFY(leftovers.front().CanBeResumed());
}

void ImportServiceTest::TheSearchForLeftoversNeverWalksIntoAnAddon()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Utils/imported.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Utils/imported.fsorg-partial/manifest.json", kMegabyte);
    f.fileSystem.AddDirectory(kInLibrary / "effects");
    f.fileSystem.AddDirectory(kInLibrary / "effects/texture.fsorg-partial");

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, std::filesystem::path{"D:/Library/Utils/imported.fsorg-partial"});
    QVERIFY(f.filesystemProbe.WasEnumerated("D:/Library/Utils"));
    QVERIFY(!f.filesystemProbe.WasEnumerated(kInLibrary));
    QVERIFY(!f.filesystemProbe.WasEnumerated(kInLibrary / "effects"));
}

void ImportServiceTest::ResumingThrowsTheHalfCopyAwayAndImportsAgainFromTheSource()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("E:/Sim/Community/orbx-yssy");
    f.fileSystem.AddFile("E:/Sim/Community/orbx-yssy/manifest.json", 4 * kMegabyte);
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory("D:/Library/Sceneries/orbx-yssy.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Sceneries/orbx-yssy.fsorg-partial/half.bin", kMegabyte);

    const StagingLeftover leftover{"D:/Library/Sceneries/orbx-yssy.fsorg-partial", "D:/Library/Sceneries/orbx-yssy",
                                   "E:/Sim/Community/orbx-yssy"};

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, {leftover}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/orbx-yssy.fsorg-partial"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Sceneries/orbx-yssy/manifest.json"));
    QVERIFY(f.fileSystem.IsLink("E:/Sim/Community/orbx-yssy"));
    QCOMPARE(f.fileSystem.LinkTarget("E:/Sim/Community/orbx-yssy").value(),
             std::filesystem::path{"D:/Library/Sceneries/orbx-yssy"});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::DiscardStaging);
}

void ImportServiceTest::DiscardingALeftoverRemovesOnlyTheStaging()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Utils/imported.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Utils/imported.fsorg-partial/manifest.json", kMegabyte);

    const std::vector<FileOperationResult> discarded =
        f.service.DiscardLeftovers(f.profile, f.service.Leftovers(f.profile));

    QCOMPARE(discarded.size(), std::size_t{1});
    QCOMPARE(discarded.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists("D:/Library/Utils/imported.fsorg-partial"));
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QCOMPARE(f.journal.appended.back().kind, OperationKind::DiscardStaging);

    const std::vector<FileOperationResult> refused =
        f.service.DiscardLeftovers(f.profile, {StagingLeftover{kInLibrary, kInLibrary, kInDestination}});

    QCOMPARE(refused.front().result, FileResult::CouldNotDiscard);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
}

void ImportServiceTest::KeepingTheDestinationCopyUnlinksTheLibraryCopyBeforeQuarantiningIt()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheDestinationCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kLinkedElsewhere));
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));

    QCOMPARE(f.journal.Read().size(), std::size_t{2});
    QCOMPARE(f.journal.Read().front().kind, OperationKind::DisableAddon);
    QCOMPARE(f.journal.Read().front().source, kLinkedElsewhere);
    QCOMPARE(f.journal.Read().back().kind, OperationKind::QuarantineFromLibrary);
}

void ImportServiceTest::NothingIsMovedWhenALinkToTheLibraryCopyCannotBeRemoved()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.linkService.MakeLinkRemovalFail();

    const CopyConflict conflict{kInDestination, kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheDestinationCopy);

    QCOMPARE(result, FileResult::CouldNotRemoveTheLink);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kLinkedElsewhere));
    QVERIFY(!f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge"));
}

void ImportServiceTest::ImportingIsRefusedWhenTheBaseNameAlreadyExistsInTheLibrary()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.TheLibraryHolds({kInLibrary});

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile, {ImportRequest{kInDestination, "D:/Library/Sceneries"}}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(results.front().occupant, kInLibrary);
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/simbridge"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/simbridge.fsorg-partial"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::ResumingIsRefusedWhenTheBaseNameAppearedWhileTheImportWasLost()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory("D:/Library/Sceneries/simbridge.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Sceneries/simbridge.fsorg-partial/half.bin", kMegabyte);
    f.TheLibraryHolds({kInLibrary});

    const StagingLeftover leftover{"D:/Library/Sceneries/simbridge.fsorg-partial", "D:/Library/Sceneries/simbridge",
                                   kInDestination};

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, {leftover}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(results.front().occupant, kInLibrary);
    QVERIFY(f.fileSystem.Exists("D:/Library/Sceneries/simbridge.fsorg-partial/half.bin"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::RestoringIsRefusedWhenTheBaseNameTookTheIdentityElsewhere()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheDestinationCopy),
             FileResult::Completed);

    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory("D:/Library/Sceneries/simbridge");
    f.fileSystem.AddFile("D:/Library/Sceneries/simbridge/manifest.json", kMegabyte);
    f.TheLibraryHolds({"D:/Library/Sceneries/simbridge"});

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(restored.size(), std::size_t{1});
    QCOMPARE(restored.front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(restored.front().occupant, std::filesystem::path{"D:/Library/Sceneries/simbridge"});
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Sceneries/simbridge/manifest.json"));
}

void ImportServiceTest::TheIdentityIsOnlyTakenInsideTheLibraryThatHoldsIt()
{
    Fixture f;
    f.AddBothCopies();
    f.TheLibraryHolds({kInLibrary});

    f.profile.libraries.push_back(Library{"lib-2", "F:/Spare"});
    f.fileSystem.AddDirectory("F:/Spare");
    f.fileSystem.AddDirectory("F:/Spare/Utils");

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile, {ImportRequest{kInDestination, "F:/Spare/Utils"}}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("F:/Spare/Utils/simbridge/manifest.json"));
}

QTEST_APPLESS_MAIN(ImportServiceTest)

#include "tst_import_service.moc"
