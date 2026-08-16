#include <QtTest/QtTest>

#include <algorithm>
#include <ranges>
#include <variant>

#include "domain/journal/OperationLog.h"
#include "application/ImportService.h"
#include "domain/importing/ImportPaths.h"
#include "domain/importing/ExternalSidecar.h"
#include "domain/importing/OriginSidecar.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/profile/ExternalOrigins.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
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
        static void AnOriginWearingOnlyALinkIsNotACollision();
        static void TheLinkStandingAtTheOriginComesOffSoTheItemCanGoBack();
        static void NothingGoesBackWhenTheLinkAtTheOriginCannotComeOff();
        static void AnItemWhoseOriginTheJournalNeverSawIsNotRestored();
        static void EmptyingTheQuarantineNeverReachesAnythingOutsideIt();
        static void NoQuarantineIsTouchedWhileTheSimulatorIsRunning();
        static void LeftoverStagingIsFoundInTheLibraryWithTheSourceItCameFrom();
        static void ALeftoverOfAnExternalImportRemembersWhichProgramItWasTakingOver();
        static void ALeftoverOfAnExternalImportKnowsTheEntryItWasImportingFrom();
        static void GivingAnAddonBackTakesTheRecordBesideItAway();
        static void AnAddonThatNeverCameFromAnotherProgramIsNotGivenBack();
        static void NothingIsGivenBackWhileTheSimulatorIsRunning();
        static void ALeftoverThatOnlyKnowsTheProgramIsNotResumedAsAnOrdinaryImport();
        static void ResumingAnExternalLeftoverTakesTheOtherProgramsFolderOverAgain();
        static void TheSearchForLeftoversNeverWalksIntoAnAddon();
        static void ASwapThatDiedHalfwayIsFoundInTheOtherProgramsFolder();
        static void ASwapThatFinishedIsNotOfferedAsInterrupted();
        static void PuttingTheFolderBackRenamesTheRoomToTheNameTheOtherProgramLooksFor();
        static void AGiveBackThatDiedHalfwayIsFoundBesideTheOtherProgramsFolder();
        static void ResumingThrowsTheHalfCopyAwayAndImportsAgainFromTheSource();
        static void DiscardingALeftoverRemovesOnlyTheStaging();
        static void KeepingTheDestinationCopyUnlinksTheLibraryCopyBeforeQuarantiningIt();
        static void NothingIsMovedWhenALinkToTheLibraryCopyCannotBeRemoved();
        static void ImportingIsRefusedWhenTheBaseNameAlreadyExistsInTheLibrary();
        static void ResumingIsRefusedWhenTheBaseNameAppearedWhileTheImportWasLost();
        static void RestoringIsRefusedWhenTheBaseNameTookTheIdentityElsewhere();
        static void TheIdentityIsOnlyTakenInsideTheLibraryThatHoldsIt();
        static void TheGuardAnswersTheFourWaysTheIdentityCanBeTaken();
        static void AnItemHeldInsideALibraryIsOfferedTheCategoriesOfThatLibrary();
        static void AnItemHeldBesideADestinationIsOfferedEveryDestinationSharingThatFolder();
        static void TheGuardsRunAgainstThePlaceTheUserChose();
        static void ACheckedRestoreCarriesTheVersionOfBothSides();
        static void ASideWithoutAReadableVersionStaysEmptyAndTheOtherStillShows();
        static void AQuarantinedItemCarriesTheVersionItsOwnManifestDeclares();
        static void AnItemWhoseNameSitsAsAPhysicalFolderInADestinationIsMarkedAsReplaced();
        static void TheReplacementMarkIsReadFromTheDestinationsAndNeverFromTheRecordedOrigin();
        static void ALinkWearingTheSameNameIsNotAReplacement();
        static void TheOriginIsRecordedBesideTheItemAndNeverInsideIt();
        static void NothingMovesWhenTheOriginCannotBeRecorded();
        static void LeavingTheQuarantineTakesTheOriginRecordAway();
        static void TheRecordBesideTheItemAnswersWhenTheJournalIsGone();
        static void TheJournalAnswersWhenTheRecordBesideTheItemIsGone();
        static void TheTwoSourcesAgreeingAnswerOnceAndNameTheRecord();
        static void AnItemWithNeitherSourceIsStillListedAndAsksWhereToGo();
        static void TheRecordBesideTheItemWinsWhenTheTwoDisagree();
        static void TheGuardsRunAgainstTheSourceThatWon();
        static void TheRestoreIsJournalledWithTheSourceItUsed();
        static void TheSwapSendsTheOccupantToQuarantineWithItsOwnOriginAndBringsTheItemBack();
        static void AFailedRestoreAfterTheOccupantMovedSaysNeitherIsInTheLibrary();
        static void AnOccupantWithoutAManifestIsNeverOfferedTheSwap();
        static void TheSwapTakesDownTheLinksToTheOccupantBeforeMovingIt();
        static void NothingIsSwappedWhileTheSimulatorIsRunning();
        static void TheSwapJustRestoresWhenTheOccupantLeftOnItsOwn();
        static void ASecondHomonymFailsToQuarantineWithoutTouchingTheRecordOfTheFirst();
        static void KeepingTheLibraryCopyOfAnExternalSendsTheVendorFolderToTheLibraryQuarantine();
        static void TheVendorFolderIsLeftWhereItIsWhenTheLibraryVolumeHasNoRoomForIt();
        static void CancellingTheCopyLeavesBothCopiesWhereTheyWere();
        static void AHalfCopiedResolutionIsFoundInsideTheQuarantineAndOnlyOfferedTheDiscard();
        static void AHalfCopiedResolutionIsNeverListedAsAQuarantinedAddon();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kInDestination = "E:/Sim/Community/simbridge";
    const std::filesystem::path kInLibrary = "D:/Library/Utils/simbridge";

    const std::filesystem::path kHeldInLibrary = "D:/Library/_fsorganizer-quarantine/simbridge";

    const std::filesystem::path kOtherDestination = "E:/Sim/Community2024";
    const std::filesystem::path kLinkedElsewhere = "E:/Sim/Community2024/simbridge";

    TreeNode AddonNodeDeclaring(const std::filesystem::path& path, const std::string& version)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{.packageVersion = version}};

        return node;
    }

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeProcessProbe processProbe;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeCatalogScanner catalog;
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService service{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking, log,          LinkType::Junction};

        SimulatorProfile profile{.destinations = {kDestination},
                                 .defaultDestination = kDestination,
                                 .libraries = {Library{.id = "lib-1", .path = kLibrary}}};

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
                node.addon = Addon{.folderPath = addon, .manifest = Manifest{}};

                catalog.SetTree(addon, node);
                library.children.push_back(std::move(node));
            }

            catalog.SetTree(kLibrary, std::move(library));
        }

        void TheLibraryIsShapedLike(const std::vector<std::filesystem::path>& categories)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;

            for (const std::filesystem::path& category : categories)
            {
                library.children.push_back(
                    TreeNode{.kind = TreeNodeKind::Category, .path = category, .declaredAsCategory = true});
            }

            catalog.SetTree(kLibrary, std::move(library));
        }

        void QuarantineHolds(const std::filesystem::path& item,
                             const std::filesystem::path& theJournalSays,
                             const std::filesystem::path& theRecordBesideItSays)
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(item.parent_path());
            fileSystem.AddDirectory(item);
            fileSystem.AddFile(item / "manifest.json", kMegabyte);

            log.RecordImport(OperationKind::QuarantineFromLibrary, AddonId{.libraryId = "lib-1"}, theJournalSays, item,
                             FileResult::Completed);

            QVERIFY(sidecars.Write(
                SidecarPathFor(item),
                TextOfTheOrigin(QuarantineOrigin{.origin = theRecordBesideItSays, .quarantinedAt = clock.now})));
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
        f.service.Import(f.profile, {ImportRequest{.source = kInDestination, .category = "D:/Library/Utils"}}, {});

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

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
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

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

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

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
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

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy),
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

    static_cast<void>(
        f.service.Import(f.profile, {ImportRequest{.source = kInDestination, .category = "D:/Library/Utils"}}, {}));
    static_cast<void>(f.service.ResolveConflict(
        f.profile, f.Entries(), CopyConflict{.provenancePath = kInDestination, .libraryPath = kInLibrary},
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
                                       ConflictChoice::KeepTheProvenanceCopy),
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
                                       ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);

    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(restored.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(restored.front().occupant, kInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 3 * kMegabyte);
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.journal.appended.back().kind, OperationKind::QuarantineFromLibrary);
}

void ImportServiceTest::AnOriginWearingOnlyALinkIsNotACollision()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);
    QVERIFY(f.fileSystem.IsLink(kInDestination));

    const std::vector<RestoreCheck> checks = f.service.CheckRestore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(checks.size(), std::size_t{1});
    QCOMPARE(checks.front().result, FileResult::Completed);
    QVERIFY(checks.front().theOriginHoldsALink);
    QVERIFY(checks.front().occupant.empty());
}

void ImportServiceTest::TheLinkStandingAtTheOriginComesOffSoTheItemCanGoBack()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(restored.size(), std::size_t{1});
    QCOMPARE(restored.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.IsLink(kInDestination));
    QCOMPARE(f.fileSystem.FileSize(kInDestination / "manifest.json"), 2 * kMegabyte);
    QVERIFY(!f.fileSystem.Exists("E:/Sim/_fsorganizer-quarantine/simbridge"));
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.service.Quarantined(f.profile).empty());

    const OperationRecord& disabled = f.journal.appended[f.journal.appended.size() - 2];
    QCOMPARE(disabled.kind, OperationKind::DisableAddon);
    QCOMPARE(disabled.addonId.libraryId, LibraryId{"lib-1"});
    QCOMPARE(disabled.source, kInLibrary);
    QCOMPARE(disabled.target, kInDestination);
    QCOMPARE(f.journal.appended.back().kind, OperationKind::RestoreFromQuarantine);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::Completed);
}

void ImportServiceTest::NothingGoesBackWhenTheLinkAtTheOriginCannotComeOff()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);

    f.linkService.MakeLinkRemovalFail();

    const std::vector<FileOperationResult> restored = f.service.Restore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(restored.front().result, FileResult::CouldNotRemoveTheLink);
    QVERIFY(f.fileSystem.IsLink(kInDestination));
    QVERIFY(f.fileSystem.Exists("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"));
    QCOMPARE(f.journal.appended.back().kind, OperationKind::RestoreFromQuarantine);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::CouldNotRemoveTheLink);
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

    const std::vector<FileOperationResult> refused = f.service.Discard(
        f.profile, {QuarantinedItem{.path = kInLibrary, .origin = {}, .quarantinedAt = std::nullopt}});

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

    f.journal.Append(OperationRecord::OfImport(
        f.clock.now, OperationKind::ImportCopyToStaging, AddonId{.libraryId = "lib-1", .folderName = "imported"},
        "E:/Sim/Community/imported", "D:/Library/Utils/imported.fsorg-partial", FileResult::Completed));

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, std::filesystem::path{"D:/Library/Utils/imported.fsorg-partial"});
    QCOMPARE(leftovers.front().target, std::filesystem::path{"D:/Library/Utils/imported"});
    QCOMPARE(leftovers.front().source, std::filesystem::path{"E:/Sim/Community/imported"});
    QVERIFY(leftovers.front().CanBeResumed());
}

namespace
{
    const std::filesystem::path kVendorFolder = "C:/Addon Manager/MSFS/gsx-pro";
    const std::filesystem::path kVendorEntry = "E:/Sim/Community/gsx-pro";
    const std::filesystem::path kVendorStaging = "D:/Library/Utils/gsx-pro.fsorg-partial";
    const std::filesystem::path kVendorInLibrary = "D:/Library/Utils/gsx-pro";

    struct HalfWayThroughAnExternalImport
    {
        static void SetUp(Fixture& f, const bool announced, const bool withSidecar)
        {
            f.fileSystem.AddDirectory(kDestination);
            f.fileSystem.AddDirectory(kLibrary);
            f.fileSystem.AddDirectory("D:/Library/Utils");
            f.fileSystem.AddDirectory(kVendorFolder.parent_path());
            f.fileSystem.AddDirectory(kVendorFolder);
            f.fileSystem.AddFile(kVendorFolder / "manifest.json", kMegabyte);
            f.fileSystem.AddLink(kVendorEntry, kVendorFolder);
            f.fileSystem.AddDirectory(kVendorStaging);
            f.fileSystem.AddFile(kVendorStaging / "half.bin", kMegabyte / 2);

            const AddonId addon{.libraryId = "lib-1", .folderName = "gsx-pro"};

            if (announced)
            {
                f.journal.Append(OperationRecord::OfImport(f.clock.now, OperationKind::ImportFromAnotherProgram, addon,
                                                           kVendorEntry, kVendorStaging, FileResult::Completed));
            }

            f.journal.Append(OperationRecord::OfImport(f.clock.now, OperationKind::ImportCopyToStaging, addon,
                                                       kVendorFolder, kVendorStaging, FileResult::Completed));

            if (withSidecar)
            {
                f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kVendorInLibrary),
                                                 TextOfTheExternalOrigin(kVendorFolder));
            }
        }
    };
}

void ImportServiceTest::ALeftoverOfAnExternalImportRemembersWhichProgramItWasTakingOver()
{
    Fixture f;
    HalfWayThroughAnExternalImport::SetUp(f, true, true);
    f.journal.appended.clear();
    f.journal.Append(OperationRecord::OfImport(f.clock.now, OperationKind::ImportFromAnotherProgram,
                                               AddonId{.libraryId = "lib-1", .folderName = "gsx-pro"}, kVendorEntry,
                                               kVendorStaging, FileResult::Completed));

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().externalSource, kVendorFolder);
    QVERIFY2(leftovers.front().CanBeResumed(),
             "the record beside the addon names the program even when the journal never got to the copy step");
}

void ImportServiceTest::ALeftoverOfAnExternalImportKnowsTheEntryItWasImportingFrom()
{
    Fixture f;
    HalfWayThroughAnExternalImport::SetUp(f, true, false);

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().source, kVendorEntry);
    QCOMPARE(leftovers.front().externalSource, kVendorFolder);
    QVERIFY2(leftovers.front().CanBeResumed(),
             "the journal alone answers both halves, so a crash during the copy is still resumable");
}

void ImportServiceTest::ALeftoverThatOnlyKnowsTheProgramIsNotResumedAsAnOrdinaryImport()
{
    Fixture f;
    HalfWayThroughAnExternalImport::SetUp(f, false, true);

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().externalSource, kVendorFolder);
    QVERIFY2(!leftovers.front().CanBeResumed(),
             "without the entry it was importing from, resuming would copy the bytes and leave the vendor folder "
             "untouched, which is the duplicate this guards against");

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, leftovers, {});

    QCOMPARE(results.front().result, FileResult::TheOriginIsUnknown);
    QVERIFY(f.fileSystem.IsDirectory(kVendorFolder));
    QVERIFY(!f.fileSystem.IsLink(kVendorFolder));
}

void ImportServiceTest::ResumingAnExternalLeftoverTakesTheOtherProgramsFolderOverAgain()
{
    Fixture f;
    HalfWayThroughAnExternalImport::SetUp(f, true, true);

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, f.service.Leftovers(f.profile), {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kVendorStaging));
    QVERIFY(f.fileSystem.Exists(kVendorInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kVendorFolder));
    QCOMPARE(f.fileSystem.LinkTarget(kVendorFolder).value(), kVendorInLibrary);
    QCOMPARE(f.fileSystem.LinkTarget(kVendorEntry).value(), kVendorInLibrary);
}

namespace
{
    void AManagedExternal(Fixture& f)
    {
        f.fileSystem.AddDirectory(kDestination);
        f.fileSystem.AddDirectory(kLibrary);
        f.fileSystem.AddDirectory("D:/Library/Utils");
        f.fileSystem.AddDirectory(kVendorFolder.parent_path());
        f.fileSystem.AddDirectory(kVendorInLibrary);
        f.fileSystem.AddFile(kVendorInLibrary / "manifest.json", kMegabyte);
        f.fileSystem.AddLink(kVendorFolder, kVendorInLibrary);
        f.fileSystem.AddLink(kVendorEntry, kVendorInLibrary);
        f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kVendorInLibrary),
                                         TextOfTheExternalOrigin(kVendorFolder));

        RememberWhereItCameFrom(f.profile, kVendorInLibrary, kVendorFolder);
    }
}

void ImportServiceTest::ASwapThatDiedHalfwayIsFoundInTheOtherProgramsFolder()
{
    Fixture f;
    AManagedExternal(f);

    f.fileSystem.RemoveTree(kVendorFolder);
    f.fileSystem.AddDirectory(SwapSlotFor(kVendorFolder));

    const std::vector<InterruptedSwap> interrupted = f.service.InterruptedSwaps(f.profile);

    QCOMPARE(interrupted.size(), std::size_t{1});
    QCOMPARE(interrupted.front().room, SwapSlotFor(kVendorFolder));
    QCOMPARE(interrupted.front().folder, kVendorFolder);
    QCOMPARE(interrupted.front().libraryCopy, kVendorInLibrary);
}

void ImportServiceTest::ASwapThatFinishedIsNotOfferedAsInterrupted()
{
    Fixture f;
    AManagedExternal(f);

    f.fileSystem.AddDirectory(SwapSlotFor(kVendorFolder));

    QVERIFY2(f.service.InterruptedSwaps(f.profile).empty(),
             "with the folder under its own name the other program finds it, so there is nothing to put back");
}

void ImportServiceTest::PuttingTheFolderBackRenamesTheRoomToTheNameTheOtherProgramLooksFor()
{
    Fixture f;
    AManagedExternal(f);

    f.fileSystem.RemoveTree(kVendorFolder);
    f.fileSystem.AddDirectory(SwapSlotFor(kVendorFolder));

    const std::vector<FileOperationResult> results =
        f.service.UndoInterruptedSwaps(f.profile, f.service.InterruptedSwaps(f.profile));

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(kVendorFolder));
    QVERIFY(!f.fileSystem.Exists(SwapSlotFor(kVendorFolder)));
    QVERIFY(f.service.InterruptedSwaps(f.profile).empty());
}

void ImportServiceTest::AGiveBackThatDiedHalfwayIsFoundBesideTheOtherProgramsFolder()
{
    Fixture f;
    AManagedExternal(f);

    f.fileSystem.AddDirectory(StagingPathFor(kVendorFolder));

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);
    const auto beside = std::ranges::find_if(leftovers,
                                             [](const StagingLeftover& leftover)
                                             {
                                                 return leftover.staging == StagingPathFor(kVendorFolder);
                                             });

    QVERIFY2(beside != leftovers.end(),
             "the manufacturer folder is neither destination nor library, so no other sweep walks it");
    QCOMPARE(beside->target, kVendorFolder);
    QVERIFY2(!beside->CanBeResumed(), "a half copied give back is discarded, not resumed");
}

void ImportServiceTest::GivingAnAddonBackTakesTheRecordBesideItAway()
{
    Fixture f;
    AManagedExternal(f);

    const FileOperationResult result = f.service.GiveBack(f.profile, f.Entries(), kVendorInLibrary, {});

    QCOMPARE(result.result, FileResult::Completed);
    QCOMPARE(result.path, kVendorInLibrary);
    QVERIFY(f.fileSystem.IsDirectory(kVendorFolder));
    QVERIFY(!f.fileSystem.IsLink(kVendorFolder));
    QVERIFY(!f.fileSystem.Exists(kVendorInLibrary));
    QVERIFY2(!f.fileSystem.Exists(ExternalSidecarPathFor(kVendorInLibrary)),
             "the record beside the addon outlives the folder unless the give back takes it too");
    QCOMPARE(f.fileSystem.LinkTarget(kVendorEntry).value(), kVendorFolder);
}

void ImportServiceTest::AnAddonThatNeverCameFromAnotherProgramIsNotGivenBack()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.GiveBack(f.profile, f.Entries(), kInLibrary, {}).result, FileResult::TheOriginIsUnknown);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
}

void ImportServiceTest::NothingIsGivenBackWhileTheSimulatorIsRunning()
{
    Fixture f;
    AManagedExternal(f);
    f.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.service.GiveBack(f.profile, f.Entries(), kVendorInLibrary, {}).result,
             FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.IsLink(kVendorFolder));
    QVERIFY(f.fileSystem.Exists(kVendorInLibrary / "manifest.json"));
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

    const StagingLeftover leftover{.staging = "D:/Library/Sceneries/orbx-yssy.fsorg-partial",
                                   .target = "D:/Library/Sceneries/orbx-yssy",
                                   .source = "E:/Sim/Community/orbx-yssy"};

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

    const std::vector<FileOperationResult> refused = f.service.DiscardLeftovers(
        f.profile, {StagingLeftover{.staging = kInLibrary, .target = kInLibrary, .source = kInDestination}});

    QCOMPARE(refused.front().result, FileResult::CouldNotDiscard);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
}

void ImportServiceTest::KeepingTheDestinationCopyUnlinksTheLibraryCopyBeforeQuarantiningIt()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kLinkedElsewhere));
    QVERIFY(f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge/manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));

    QCOMPARE(f.journal.Read().size(), std::size_t{2});
    QCOMPARE(f.journal.Read().front().kind, OperationKind::DisableAddon);
    QCOMPARE(f.journal.Read().front().source, kInLibrary);
    QCOMPARE(f.journal.Read().front().target, kLinkedElsewhere);
    QCOMPARE(f.journal.Read().back().kind, OperationKind::QuarantineFromLibrary);
}

void ImportServiceTest::NothingIsMovedWhenALinkToTheLibraryCopyCannotBeRemoved()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.linkService.MakeLinkRemovalFail();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

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
        f.service.Import(f.profile, {ImportRequest{.source = kInDestination, .category = "D:/Library/Sceneries"}}, {});

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

    const StagingLeftover leftover{.staging = "D:/Library/Sceneries/simbridge.fsorg-partial",
                                   .target = "D:/Library/Sceneries/simbridge",
                                   .source = kInDestination};

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
                                       ConflictChoice::KeepTheProvenanceCopy),
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

    f.profile.libraries.push_back(Library{.id = "lib-2", .path = "F:/Spare"});
    f.fileSystem.AddDirectory("F:/Spare");
    f.fileSystem.AddDirectory("F:/Spare/Utils");

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile, {ImportRequest{.source = kInDestination, .category = "F:/Spare/Utils"}}, {});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("F:/Spare/Utils/simbridge/manifest.json"));
}

void ImportServiceTest::TheGuardAnswersTheFourWaysTheIdentityCanBeTaken()
{
    Fixture f;
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path elsewhere = "D:/Library/Sceneries/simbridge";

    f.AddBothCopies();
    f.fileSystem.RemoveTree(kInLibrary);
    f.fileSystem.AddDirectory(held);
    f.fileSystem.AddFile(held / "manifest.json", kMegabyte);

    const std::vector<QuarantinedItem> items{QuarantinedItem{.path = held, .origin = kInLibrary}};

    QCOMPARE(f.service.CheckRestore(f.profile, items).front().result, FileResult::Completed);

    f.fileSystem.AddDirectory(elsewhere);
    f.TheLibraryHolds({elsewhere});

    QCOMPARE(f.service.CheckRestore(f.profile, items).front().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(f.service.CheckRestore(f.profile, items).front().occupant, elsewhere);

    f.fileSystem.RemoveTree(elsewhere);
    f.TheLibraryHolds({});
    f.fileSystem.AddDirectory(kInLibrary);

    QCOMPARE(f.service.CheckRestore(f.profile, items).front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(f.service.CheckRestore(f.profile, items).front().occupant, kInLibrary);

    f.fileSystem.AddDirectory(elsewhere);
    f.TheLibraryHolds({elsewhere});

    QCOMPARE(f.service.CheckRestore(f.profile, items).front().result, FileResult::TheIdentityIsTaken);
}

void ImportServiceTest::AnItemHeldInsideALibraryIsOfferedTheCategoriesOfThatLibrary()
{
    Fixture f;
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    f.AddBothCopies();
    f.fileSystem.AddDirectory(held);
    f.TheLibraryIsShapedLike({"D:/Library/Sceneries", "D:/Library/Utils"});

    const std::vector<RestorePlace> places = f.service.PlacesFor(f.profile, QuarantinedItem{.path = held});

    QCOMPARE(places.size(), std::size_t{3});
    QCOMPARE(places.front().place, kLibrary);
    QCOMPARE(places.front().target, std::filesystem::path{"D:/Library/simbridge"});
    QCOMPARE(places[1].place, std::filesystem::path{"D:/Library/Sceneries"});
    QCOMPARE(places[1].target, std::filesystem::path{"D:/Library/Sceneries/simbridge"});
    QCOMPARE(places[1].label, std::filesystem::path{"Sceneries"});
    QCOMPARE(places[2].place, std::filesystem::path{"D:/Library/Utils"});
}

void ImportServiceTest::AnItemHeldBesideADestinationIsOfferedEveryDestinationSharingThatFolder()
{
    Fixture f;
    const std::filesystem::path held = "E:/Sim/_fsorganizer-quarantine/simbridge";

    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.fileSystem.AddDirectory(held);

    const std::vector<RestorePlace> places = f.service.PlacesFor(f.profile, QuarantinedItem{.path = held});

    QCOMPARE(places.size(), std::size_t{2});
    QCOMPARE(places.front().place, kDestination);
    QCOMPARE(places.front().target, kInDestination);
    QCOMPARE(places.front().label, kDestination);
    QCOMPARE(places[1].place, kOtherDestination);
    QCOMPARE(places[1].target, kLinkedElsewhere);
}

void ImportServiceTest::TheGuardsRunAgainstThePlaceTheUserChose()
{
    Fixture f;
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    f.AddBothCopies();
    f.fileSystem.RemoveTree(kInLibrary);
    f.fileSystem.AddDirectory(held);
    f.fileSystem.AddFile(held / "manifest.json", kMegabyte);
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory("D:/Library/Sceneries/simbridge");
    f.TheLibraryIsShapedLike({"D:/Library/Sceneries", "D:/Library/Utils"});

    const std::vector<RestorePlace> places = f.service.PlacesFor(f.profile, QuarantinedItem{.path = held});
    QCOMPARE(places.size(), std::size_t{3});

    const QuarantinedItem intoSceneries{.path = held, .origin = places[1].target};
    QCOMPARE(f.service.CheckRestore(f.profile, {intoSceneries}).front().result, FileResult::TheOriginIsOccupied);

    const QuarantinedItem intoUtils{.path = held, .origin = places[2].target};
    QCOMPARE(f.service.Restore(f.profile, {intoUtils}).front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QCOMPARE(f.journal.appended.back().kind, OperationKind::RestoreFromQuarantine);
    QCOMPARE(f.journal.appended.back().target, kInLibrary);
}

void ImportServiceTest::ACheckedRestoreCarriesTheVersionOfBothSides()
{
    Fixture f;
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    f.AddBothCopies();
    f.fileSystem.AddDirectory(held);
    f.catalog.SetTree(held, AddonNodeDeclaring(held, "2.4.1"));
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "2.5.0"));

    const std::vector<RestoreCheck> checks =
        f.service.CheckRestore(f.profile, {QuarantinedItem{.path = held, .origin = kInLibrary}});

    QCOMPARE(checks.size(), std::size_t{1});
    QCOMPARE(checks.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(checks.front().target, kInLibrary);
    QCOMPARE(checks.front().version, std::string{"2.4.1"});
    QCOMPARE(checks.front().occupantVersion, std::string{"2.5.0"});
}

void ImportServiceTest::ASideWithoutAReadableVersionStaysEmptyAndTheOtherStillShows()
{
    Fixture f;
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    f.AddBothCopies();
    f.fileSystem.AddDirectory(held);
    f.catalog.SetTree(held, AddonNodeDeclaring(held, "2.4.1"));

    const std::vector<RestoreCheck> checks =
        f.service.CheckRestore(f.profile, {QuarantinedItem{.path = held, .origin = kInLibrary}});

    QCOMPARE(checks.front().version, std::string{"2.4.1"});
    QVERIFY(checks.front().occupantVersion.empty());
}

void ImportServiceTest::AQuarantinedItemCarriesTheVersionItsOwnManifestDeclares()
{
    Fixture f;
    const std::filesystem::path held = "E:/Sim/_fsorganizer-quarantine/simbridge";

    f.fileSystem.AddDirectory(held);
    f.catalog.SetTree(held, AddonNodeDeclaring(held, "2.4.1"));

    const std::vector<QuarantineDetail> details = f.service.Describe({}, {QuarantinedItem{.path = held}});

    QCOMPARE(details.size(), std::size_t{1});
    QCOMPARE(details.front().path, held);
    QCOMPARE(details.front().version, std::string{"2.4.1"});
    QVERIFY(!details.front().WasReplaced());
}

void ImportServiceTest::AnItemWhoseNameSitsAsAPhysicalFolderInADestinationIsMarkedAsReplaced()
{
    Fixture f;
    const std::filesystem::path held = "E:/Sim/_fsorganizer-quarantine/simbridge";

    f.fileSystem.AddDirectory(held);
    f.catalog.SetTree(held, AddonNodeDeclaring(held, "2.4.1"));
    f.catalog.SetTree(kInDestination, AddonNodeDeclaring(kInDestination, "2.5.0"));

    const std::vector<QuarantineDetail> details = f.service.Describe(
        {DestinationEntry{.path = kInDestination, .target = {}, .classification = EntryClassification::Unmanaged}},
        {QuarantinedItem{.path = held}});

    QVERIFY(details.front().WasReplaced());
    QCOMPARE(details.front().replacedBy, kInDestination);
    QCOMPARE(details.front().version, std::string{"2.4.1"});
    QCOMPARE(details.front().replacementVersion, std::string{"2.5.0"});
}

void ImportServiceTest::TheReplacementMarkIsReadFromTheDestinationsAndNeverFromTheRecordedOrigin()
{
    Fixture f;
    const std::filesystem::path held = "E:/Sim/_fsorganizer-quarantine/simbridge";

    f.fileSystem.AddDirectory(held);

    const std::vector<QuarantineDetail> withNoOrigin = f.service.Describe(
        {DestinationEntry{.path = kInDestination, .target = {}, .classification = EntryClassification::Unmanaged}},
        {QuarantinedItem{.path = held, .origin = {}, .quarantinedAt = std::nullopt}});

    QVERIFY(withNoOrigin.front().WasReplaced());
}

void ImportServiceTest::ALinkWearingTheSameNameIsNotAReplacement()
{
    Fixture f;
    const std::filesystem::path held = "E:/Sim/_fsorganizer-quarantine/simbridge";

    f.fileSystem.AddDirectory(held);

    const std::vector<QuarantineDetail> details = f.service.Describe(
        {DestinationEntry{
            .path = kInDestination, .target = kInLibrary, .classification = EntryClassification::Managed}},
        {QuarantinedItem{.path = held}});

    QVERIFY(!details.front().WasReplaced());
}

void ImportServiceTest::TheOriginIsRecordedBesideTheItemAndNeverInsideIt()
{
    Fixture f;
    f.AddBothCopies();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);

    const std::filesystem::path quarantined = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::optional<std::string> written = f.fileSystem.ContentsOf(SidecarPathFor(quarantined));

    QVERIFY(written.has_value());

    const std::optional<QuarantineOrigin> recorded = OriginFromText(*written);

    QVERIFY(recorded.has_value());
    QCOMPARE(recorded->origin, kInLibrary);
    QCOMPARE(recorded->quarantinedAt.value(), f.clock.now);

    QVERIFY(f.fileSystem.IsDirectory(quarantined));
    QVERIFY(!f.fileSystem.Exists(SidecarPathFor(quarantined.filename())));
    QCOMPARE(f.service.Quarantined(f.profile).size(), std::size_t{1});
}

void ImportServiceTest::NothingMovesWhenTheOriginCannotBeRecorded()
{
    Fixture f;
    f.AddBothCopies();
    f.sidecars.MakeTheWriteFail();

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheProvenanceCopy),
             FileResult::CouldNotRecordTheOrigin);

    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/_fsorganizer-quarantine/simbridge"));

    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::QuarantineFromLibrary);
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::CouldNotRecordTheOrigin);
}

void ImportServiceTest::LeavingTheQuarantineTakesTheOriginRecordAway()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory(kOtherDestination);
    f.profile.destinations.push_back(kOtherDestination);
    f.fileSystem.AddDirectory("E:/Sim/Community2024/other");
    f.fileSystem.AddFile("E:/Sim/Community2024/other/manifest.json", kMegabyte);

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, kInLibrary},
                                       ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);
    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{"E:/Sim/Community2024/other", kInLibrary},
                                       ConflictChoice::KeepTheLibraryCopy),
             FileResult::Completed);

    const std::filesystem::path restored = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path discarded = "E:/Sim/_fsorganizer-quarantine/other";

    QVERIFY(f.fileSystem.Exists(SidecarPathFor(restored)));
    QVERIFY(f.fileSystem.Exists(SidecarPathFor(discarded)));

    QCOMPARE(f.service.Restore(f.profile, {QuarantinedItem{.path = restored, .origin = kInLibrary}}).front().result,
             FileResult::Completed);
    QCOMPARE(f.service.Discard(f.profile, {QuarantinedItem{.path = discarded}}).front().result, FileResult::Completed);

    QVERIFY(!f.fileSystem.Exists(SidecarPathFor(restored)));
    QVERIFY(!f.fileSystem.Exists(SidecarPathFor(discarded)));
    QVERIFY(!f.fileSystem.Exists(SidecarPathFor(kInLibrary)));
}

void ImportServiceTest::TheRecordBesideTheItemAnswersWhenTheJournalIsGone()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.journal.appended.clear();

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, kInLibrary);
    QCOMPARE(items.front().source, OriginSource::Sidecar);
    QCOMPARE(items.front().quarantinedAt.value(), f.clock.now);
}

void ImportServiceTest::TheJournalAnswersWhenTheRecordBesideTheItemIsGone()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);

    QVERIFY(f.fileSystem.RemoveTree(SidecarPathFor(kHeldInLibrary)));

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, kInLibrary);
    QCOMPARE(items.front().source, OriginSource::Journal);
    QCOMPARE(items.front().quarantinedAt.value(), f.clock.now);
}

void ImportServiceTest::TheTwoSourcesAgreeingAnswerOnceAndNameTheRecord()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, kInLibrary);
    QCOMPARE(items.front().source, OriginSource::Sidecar);
}

void ImportServiceTest::AnItemWithNeitherSourceIsStillListedAndAsksWhereToGo()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory(kHeldInLibrary);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QVERIFY(!items.front().KnowsWhereItCameFrom());
    QCOMPARE(items.front().source, OriginSource::Unknown);

    const std::vector<RestoreCheck> checks = f.service.CheckRestore(f.profile, items);

    QCOMPARE(checks.front().result, FileResult::TheOriginIsUnknown);
    QVERIFY(checks.front().NeedsAPlace());
}

void ImportServiceTest::TheRecordBesideTheItemWinsWhenTheTwoDisagree()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, "D:/Library/Utils/somewhere-else");

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().origin, std::filesystem::path{"D:/Library/Utils/somewhere-else"});
    QCOMPARE(items.front().source, OriginSource::Sidecar);
}

void ImportServiceTest::TheGuardsRunAgainstTheSourceThatWon()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, "D:/Library/Utils/somewhere-else");
    f.fileSystem.AddDirectory("D:/Library/Utils/somewhere-else");

    const std::vector<RestoreCheck> checks = f.service.CheckRestore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(checks.size(), std::size_t{1});
    QCOMPARE(checks.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(checks.front().target, std::filesystem::path{"D:/Library/Utils/somewhere-else"});
    QCOMPARE(checks.front().occupant, std::filesystem::path{"D:/Library/Utils/somewhere-else"});
}

void ImportServiceTest::TheRestoreIsJournalledWithTheSourceItUsed()
{
    Fixture beside;
    beside.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);

    QCOMPARE(beside.service.Restore(beside.profile, beside.service.Quarantined(beside.profile)).front().result,
             FileResult::Completed);
    QCOMPARE(beside.journal.appended.back().kind, OperationKind::RestoreFromQuarantine);
    QCOMPARE(beside.journal.appended.back().originSource, OriginSource::Sidecar);

    Fixture recorded;
    recorded.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    QVERIFY(recorded.fileSystem.RemoveTree(SidecarPathFor(kHeldInLibrary)));

    QCOMPARE(recorded.service.Restore(recorded.profile, recorded.service.Quarantined(recorded.profile)).front().result,
             FileResult::Completed);
    QCOMPARE(recorded.journal.appended.back().originSource, OriginSource::Journal);
}

void ImportServiceTest::TheSwapSendsTheOccupantToQuarantineWithItsOwnOriginAndBringsTheItemBack()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "2.0.0"));

    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QVERIFY(swapped.Succeeded());
    QCOMPARE(swapped.stoppedAt, SwapStep::RestoreTheItem);
    QCOMPARE(swapped.occupant, kInLibrary);
    QCOMPARE(swapped.inTheLibrary, kInLibrary);

    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);

    const std::optional<std::string> written = f.fileSystem.ContentsOf(SidecarPathFor(kHeldInLibrary));
    QVERIFY(written.has_value());
    QCOMPARE(OriginFromText(*written)->origin, kInLibrary);

    QVERIFY(!f.fileSystem.WasRecycled(kInLibrary));
}

void ImportServiceTest::AFailedRestoreAfterTheOccupantMovedSaysNeitherIsInTheLibrary()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});

    const QuarantinedItem item = f.service.Quarantined(f.profile).front();

    f.files.MakeTheMoveFailAfter(2);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QVERIFY(!swapped.Succeeded());
    QCOMPARE(swapped.stoppedAt, SwapStep::RestoreTheItem);
    QCOMPARE(swapped.result, FileResult::CouldNotRestore);
    QCOMPARE(swapped.inTheLibrary, std::filesystem::path{});
    QVERIFY(!f.fileSystem.Exists(kInLibrary));

    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
    QCOMPARE(OriginFromText(*f.fileSystem.ContentsOf(SidecarPathFor(kHeldInLibrary)))->origin, kInLibrary);

    QCOMPARE(f.fileSystem.FileSize(SwapSlotFor(kHeldInLibrary) / "manifest.json"), kMegabyte);
}

void ImportServiceTest::AnOccupantWithoutAManifestIsNeverOfferedTheSwap()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "readme.txt", kMegabyte);

    const std::vector<RestoreCheck> checks = f.service.CheckRestore(f.profile, f.service.Quarantined(f.profile));

    QCOMPARE(checks.front().result, FileResult::TheOriginIsOccupied);
    QVERIFY(!checks.front().CanBeSwapped());

    const SwapResult refused = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QCOMPARE(refused.result, FileResult::TheOriginIsOccupied);
    QCOMPARE(refused.stoppedAt, SwapStep::QuarantineTheOccupant);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "readme.txt"));
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
}

void ImportServiceTest::TheSwapTakesDownTheLinksToTheOccupantBeforeMovingIt()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.fileSystem.AddDirectory(kDestination);
    f.fileSystem.AddLink(kInDestination, kInLibrary);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    QVERIFY(!f.fileSystem.Exists(kInDestination));

    const auto disabled = std::ranges::find_if(f.journal.appended,
                                               [](const OperationRecord& record)
                                               {
                                                   return record.kind == OperationKind::DisableAddon;
                                               });

    QVERIFY(disabled != f.journal.appended.end());
}

void ImportServiceTest::NothingIsSwappedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});

    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    const std::size_t before = f.journal.appended.size();

    f.processProbe.ReportTheSimulatorAsRunning();

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.journal.appended.size(), before);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 3 * kMegabyte);
}

void ImportServiceTest::TheSwapJustRestoresWhenTheOccupantLeftOnItsOwn()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    QCOMPARE(swapped.stoppedAt, SwapStep::RestoreTheItem);
    QCOMPARE(swapped.inTheLibrary, kInLibrary);
    QCOMPARE(f.journal.appended.back().kind, OperationKind::RestoreFromQuarantine);
}

void ImportServiceTest::ASecondHomonymFailsToQuarantineWithoutTouchingTheRecordOfTheFirst()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);

    const std::filesystem::path second = "D:/Library/Sceneries/simbridge";
    f.fileSystem.AddDirectory(second);
    f.fileSystem.AddFile(second / "manifest.json", 7 * kMegabyte);

    const FileResult refused = f.service.ResolveConflict(f.profile, f.Entries(), CopyConflict{kInDestination, second},
                                                         ConflictChoice::KeepTheProvenanceCopy);

    QVERIFY(!Succeeded(refused));
    QVERIFY(f.fileSystem.Exists(second / "manifest.json"));

    const std::optional<std::string> written = f.fileSystem.ContentsOf(SidecarPathFor(kHeldInLibrary));

    QVERIFY(written.has_value());
    QCOMPARE(OriginFromText(*written)->origin, kInLibrary);
}

namespace
{
    const std::filesystem::path kVendorQuarantine = "D:/Library/_fsorganizer-quarantine/gsx-pro";

    void ADivergentManagedExternal(Fixture& f)
    {
        AManagedExternal(f);

        f.fileSystem.AddDirectory(kVendorFolder);
        f.fileSystem.AddFile(kVendorFolder / "manifest.json", 3 * kMegabyte);
    }

    CopyConflict TheDivergence()
    {
        return CopyConflict{
            .provenancePath = kVendorFolder, .libraryPath = kVendorInLibrary, .theProvenanceIsAnotherProgram = true};
    }
}

void ImportServiceTest::KeepingTheLibraryCopyOfAnExternalSendsTheVendorFolderToTheLibraryQuarantine()
{
    Fixture f;
    ADivergentManagedExternal(f);

    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), TheDivergence(), ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::Completed);
    QCOMPARE(f.fileSystem.FileSize(kVendorQuarantine / "manifest.json"), 3 * kMegabyte);
    QVERIFY2(!f.fileSystem.Exists(StagingPathFor(kVendorQuarantine)), "the staging goes away once the copy lands");
    QVERIFY2(f.fileSystem.IsLink(kVendorFolder), "the arrangement the divergence broke is the one that comes back");
    QCOMPARE(f.fileSystem.LinkTarget(kVendorFolder).value(), kVendorInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kVendorInLibrary / "manifest.json"), kMegabyte);

    const std::optional<std::string> written = f.fileSystem.ContentsOf(SidecarPathFor(kVendorQuarantine));

    QVERIFY(written.has_value());
    QCOMPARE(OriginFromText(*written)->origin, kVendorFolder);
}

void ImportServiceTest::TheVendorFolderIsLeftWhereItIsWhenTheLibraryVolumeHasNoRoomForIt()
{
    Fixture f;
    ADivergentManagedExternal(f);
    f.fileSystem.SetFreeSpace(kLibrary, kMegabyte);

    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), TheDivergence(), ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::NotEnoughFreeSpace);
    QCOMPARE(f.fileSystem.FileSize(kVendorFolder / "manifest.json"), 3 * kMegabyte);
    QVERIFY(!f.fileSystem.IsLink(kVendorFolder));
    QVERIFY(!f.fileSystem.Exists(kVendorQuarantine));
    QVERIFY2(!f.fileSystem.Exists(SidecarPathFor(kVendorQuarantine)),
             "the origin record goes with the movement it announced");
}

void ImportServiceTest::CancellingTheCopyLeavesBothCopiesWhereTheyWere()
{
    Fixture f;
    ADivergentManagedExternal(f);

    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), TheDivergence(), ConflictChoice::KeepTheLibraryCopy,
                                  [](const CopyProgress&)
                                  {
                                      return false;
                                  });

    QCOMPARE(result, FileResult::Cancelled);
    QCOMPARE(f.fileSystem.FileSize(kVendorFolder / "manifest.json"), 3 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kVendorInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(StagingPathFor(kVendorQuarantine)));
}

void ImportServiceTest::AHalfCopiedResolutionIsFoundInsideTheQuarantineAndOnlyOfferedTheDiscard()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddFile(StagingPathFor(kVendorQuarantine) / "manifest.json", kMegabyte);

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, StagingPathFor(kVendorQuarantine));
    QCOMPARE(leftovers.front().target, kVendorQuarantine);
    QVERIFY(leftovers.front().theCopyResolvedAConflict);
    QVERIFY2(!leftovers.front().CanBeResumed(), "half a conflict resolution is not half an import");

    QCOMPARE(f.service.DiscardLeftovers(f.profile, leftovers).front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(StagingPathFor(kVendorQuarantine)));
}

void ImportServiceTest::AHalfCopiedResolutionIsNeverListedAsAQuarantinedAddon()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddFile(StagingPathFor(kVendorQuarantine) / "manifest.json", kMegabyte);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{1});
    QCOMPARE(items.front().path, kHeldInLibrary);
}

QTEST_APPLESS_MAIN(ImportServiceTest)

#include "tst_import_service.moc"
