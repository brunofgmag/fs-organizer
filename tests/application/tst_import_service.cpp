#include <QtTest/QtTest>

#include <algorithm>

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
        static void ALoserAnotherProgramHoldsOpenIsNamedAsSuchAndNotAsAQuarantineThatFailed();
        static void AMoveNobodyIsBlockingStillFailsAsTheQuarantineItWas();
        static void AQuarantineThatAlreadyHoldsThatNameSaysSoAndTouchesNothing();
        static void AConflictWithNowhereToPutTheLoserSaysSoAndReachesTheJournal();
        static void AnOccupantWithNowhereToGoStopsTheSwapAndReachesTheJournal();
        static void ASwapStoppedByAnotherProgramNamesItInsteadOfTheQuarantine();
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
        static void NothingIsPutBackWhileTheSimulatorIsRunning();
        static void AGiveBackThatDiedHalfwayIsFoundBesideTheOtherProgramsFolder();
        static void ResumingThrowsTheHalfCopyAwayAndImportsAgainFromTheSource();
        static void NothingIsResumedWhileTheSimulatorIsRunning();
        static void ALeftoverWhoseNameAnotherLeftoverOfTheGestureLandedElsewhereIsRefusedAsItsTwin();
        static void DiscardingALeftoverRemovesOnlyTheStaging();
        static void NothingIsDiscardedWhileTheSimulatorIsRunning();
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
        static void ASideTheWalkCouldNotReadHasNoSizeInsteadOfZero();
        static void AQuarantinedItemCarriesTheVersionItsOwnManifestDeclares();
        static void AnItemWhoseNameSitsAsAPhysicalFolderInADestinationIsMarkedAsReplaced();
        static void TheReplacementMarkIsReadFromTheDestinationsAndNeverFromTheRecordedOrigin();
        static void ALinkWearingTheSameNameIsNotAReplacement();
        static void TheOriginIsRecordedBesideTheItemAndNeverInsideIt();
        static void NothingMovesWhenTheOriginCannotBeRecorded();
        static void LeavingTheQuarantineTakesTheOriginRecordAway();
        static void TheRecordBesideTheItemAnswersWhenTheJournalIsGone();
        static void AWinnerThatCannotTakeTheLosersPlaceStillGetsTheOtherLinksBack();
        static void TheJournalAnswersWhenTheRecordBesideTheItemIsGone();
        static void TheTwoSourcesAgreeingAnswerOnceAndNameTheRecord();
        static void AnItemWithNeitherSourceIsStillListedAndAsksWhereToGo();
        static void TheRecordBesideTheItemWinsWhenTheTwoDisagree();
        static void TheGuardsRunAgainstTheSourceThatWon();
        static void TheRestoreIsJournalledWithTheSourceItUsed();
        static void TheSwapSendsTheOccupantToQuarantineWithItsOwnOriginAndBringsTheItemBack();
        static void AFailedRestoreAfterTheOccupantMovedSaysNeitherIsInTheLibrary();
        static void AFailedRestoreAfterTheOccupantMovedLeavesItsLinksDown();
        static void AnOccupantWithoutAManifestIsNeverOfferedTheSwap();
        static void TheSwapPutsTheLinksOfTheOccupantBackOnTheRestoredCopy();
        static void TheSwapPutsBothLinksOfTheOccupantBack();
        static void TheSwapOfAnOccupantWithoutLinksCreatesNone();
        static void ASwapStillSucceedsWhenTheLinksCannotBePutBack();
        static void NothingIsSwappedWhileTheSimulatorIsRunning();
        static void TheSwapJustRestoresWhenTheOccupantLeftOnItsOwn();
        static void ASecondHomonymFailsToQuarantineWithoutTouchingTheRecordOfTheFirst();
        static void KeepingTheLibraryCopyOfAnExternalSendsTheVendorFolderToTheLibraryQuarantine();
        static void TheVendorFolderIsLeftWhereItIsWhenTheLibraryVolumeHasNoRoomForIt();
        static void CancellingTheCopyLeavesBothCopiesWhereTheyWere();
        static void AHalfCopiedResolutionIsFoundInsideTheQuarantineAndOnlyOfferedTheDiscard();
        static void AHalfCopiedResolutionIsNeverListedAsAQuarantinedAddon();
        static void TakingBackWhatReplacedTheLinkPutsTheDestinationCopyInTheLibraryAndLinksItAgain();
        static void TakingItBackLeavesTheOldLibraryCopyInQuarantineInsteadOfDeletingIt();
        static void AnImportOfManyFoldersScansTheLibraryOnce();
        static void AFolderWhoseNameTheBatchAlreadyLandedElsewhereIsRefusedAsItsTwin();
        static void AFolderWhoseNameTheBatchLandedInAnotherLibraryIsNotATwin();
        static void ARequestTheCallerStopsAtAndEveryOneAfterItAreCancelledUntouched();
        static void TheSimulatorStartingBetweenTwoRequestsRefusesTheRest();
        static void AResumeStopsAtTheLeftoverTheCallerStopsAtAndCancelsEveryOneAfterIt();
        static void TheSimulatorStartingBetweenTwoLeftoversRefusesTheRest();
        static void AnItemStrandedInItsSwapSlotIsListedWithTheOriginOfItsRealName();
        static void AnItemStrandedInItsSwapSlotIsOfferedEveryPlaceUnderItsRealName();
        static void AnItemStrandedInItsSwapSlotIsMarkedAsReplacedByTheDestinationFolderOfItsRealName();
        static void OffersForManyItemsScanTheirLibraryOnce();
        static void AFreshFolderIsAdmittedWhereItWasAsked();
        static void AnInstalledPackageIsTheOccupantAndReportsItsVersionAndWhereItIs();
        static void ASamenamedAddonInAnotherLibraryIsNotTheOccupant();
        static void AnOccupantWithoutAManifestRefusesTheAdmission();
        static void SomethingThatIsNotAnAddonAtTheWantedPathRefusesTheAdmission();
        static void ACategoryOutsideEveryLibraryRefusesTheAdmission();
        static void NothingIsAdmittedWhileTheSimulatorIsRunning();
        static void AnEarlierCopyInTheQuarantineIsReportedByTheAdmissionCheck();
        static void CheckingManyPathsScansTheLibrariesOnce();
        static void AdmittingANewFolderMovesItIntoPlaceAndJournalsIt();
        static void AdmittingOverAnInstalledPackageQuarantinesItAndLinksTheNewOneInItsPlaces();
        static void AnAdmissionWhileTheSimulatorRunsTouchesNothing();
        static void AnOccupiedQuarantineRefusesTheAdmissionAndTouchesNothing();
        static void AnAdmissionOutsideEveryLibraryTouchesNothing();
        static void AnOccupantWithoutAManifestIsNeverReplacedAndTouchesNothing();
        static void AnOccupantThatAppearedAfterTheOfferIsNotReplacedAndTouchesNothing();
        static void AnOccupantOtherThanTheOneAcceptedIsNotReplacedAndTouchesNothing();
        static void AStagedFolderOnAnotherVolumeIsRefusedAndTouchesNothing();
        static void AStagedFolderThatIsGoneIsRefusedAndTouchesNothing();
        static void AnOccupantWithNowhereToGoIsRefusedAndTouchesNothing();
        static void AskingToDiscardTheEarlierCopyDiscardsItOnlyAfterTheNewFolderLanded();
        static void AnEarlierCopyComesBackWhenALinkCannotComeDown();
        static void AnEarlierCopyComesBackWhenTheOccupantCannotBeQuarantined();
        static void AnEarlierCopyComesBackWhenTheNewFolderCannotMoveIntoPlace();
        static void AnEarlierCopyThatCannotBeSetAsideStopsTheAdmissionUntouched();
        static void ARestoreThatAlsoFailsLeavesTheOccupantInQuarantineAndTheEarlierCopyBesideIt();
        static void AnEarlierCopyThatCannotBeDiscardedStaysBesideTheSlotAndTheAdmissionStillSucceeds();
        static void EveryLinkIsTriedEvenWhenTheFirstCannotBeCreated();
        static void AConflictQuarantineThatFailsPutsTheLinksThatCameDownBack();
        static void AConflictLinkThatCannotComeDownHalfwayPutsTheOnesThatDidBack();
        static void TakingBackAReplacedLinkWhoseQuarantineFailsPutsTheLinksBack();
        static void TakingBackAReplacedLinkWhoseLinksCannotComeDownHalfwayPutsTheOnesThatDidBack();
        static void TakingBackAReplacedLinkWhoseImportFailsBringsTheLibraryCopyAndItsLinksBack();
        static void ASwapWhoseLinksCannotComeDownHalfwayPutsTheOnesThatDidBack();
        static void ASwapThatCannotMoveTheItemIntoItsSlotPutsTheLinksBack();
        static void ASwapWhoseOccupantCannotBeQuarantinedPutsTheLinksBack();
        static void ASwapWhoseOccupantCannotBeQuarantinedLeavesTheItemWithItsOwnRecordOfOrigin();
        static void ASwapWhoseOccupantCannotBeQuarantinedLeavesAnItemWithoutARecordWithoutOne();
        static void TheRecordOfAnotherProgramBesideAReplacedFolderStaysBesideTheNewOne();
        static void ALeftoverInTheInstallStagingIsNeverOfferedAsResumableFromAnOldImportRecord();
        static void AMoveThatFailsAfterTheQuarantineBringsTheOccupantAndItsLinksBack();
        static void AQuarantineThatFailsLeavesTheOccupantAndRelinksIt();
        static void ALinkThatCannotBeRecreatedAfterTheLandingKeepsTheLanding();
        static void ALinkThatCannotComeDownHalfwayIsPutBackAndNothingElseMoves();
        static void AnAdmissionBatchStopsWhereTheCallerStopsIt();
        static void TheSimulatorStartingBetweenTwoAdmissionsRefusesTheRest();
        static void TwoNewFoldersOfOneNameAdmitOnlyTheFirst();
        static void AFolderOfTheNameTheBatchJustReplacedIsRefusedByIdentity();
        static void AnAdmissionBatchScansTheLibrariesOnce();
        static void AQuarantineThatRefusesTheOccupantKeepsTheLinksOfTheLibraryCopy();
        static void AQuarantineThatRefusesTakingItBackKeepsTheLinksOfTheLibraryCopy();
        static void AQuarantineThatRefusesTheSwapKeepsTheLinksOfTheOccupant();
        static void TakingBackWhatWasReplacedKeepsTheLinksInTheOtherDestinations();
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

    const std::filesystem::path kThirdDestination = "E:/Sim/Community2025";
    const std::filesystem::path kLinkedInTheThird = "E:/Sim/Community2025/simbridge";

    const std::filesystem::path kEarlierOrigin = "D:/Library/Sceneries/simbridge-before";

    const std::filesystem::path kUtils = "D:/Library/Utils";
    const std::filesystem::path kSceneries = "D:/Library/Sceneries";

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

        void AddALinkToTheLibraryCopyInAThirdDestination()
        {
            profile.destinations.push_back(kThirdDestination);
            fileSystem.AddDirectory(kThirdDestination);
            fileSystem.AddLink(kLinkedInTheThird, kInLibrary);
        }

        void AnEarlierCopyIsQuarantined()
        {
            QuarantineHolds(kHeldInLibrary, kInLibrary, kEarlierOrigin);
            fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);
            catalog.SetTree(kHeldInLibrary, AddonNodeDeclaring(kHeldInLibrary, "0.9.4"));
        }

        [[nodiscard]] std::string TheSidecarOfTheHeldCopy() const
        {
            return fileSystem.ContentsOf(SidecarPathFor(kHeldInLibrary)).value_or(std::string{});
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

        void AddASource(const std::filesystem::path& folder)
        {
            fileSystem.AddDirectory(folder.parent_path());
            fileSystem.AddDirectory(folder);
            fileSystem.AddFile(folder / "manifest.json", kMegabyte);
        }

        [[nodiscard]] std::vector<ImportRequest> ThreeSourcesIntoUtils()
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utils");

            std::vector<ImportRequest> requests;

            for (const char* name : {"alpha", "beta", "gamma"})
            {
                const std::filesystem::path source = kDestination / name;

                AddASource(source);
                requests.push_back(ImportRequest{.source = source, .category = "D:/Library/Utils"});
            }

            return requests;
        }

        [[nodiscard]] std::vector<StagingLeftover> ThreeLeftoversIntoUtils()
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory("D:/Library/Utils");

            std::vector<StagingLeftover> leftovers;

            for (const char* name : {"alpha", "beta", "gamma"})
            {
                const std::filesystem::path source = kDestination / name;
                const std::filesystem::path target = std::filesystem::path{"D:/Library/Utils"} / name;
                const std::filesystem::path staging =
                    std::filesystem::path{"D:/Library/Utils"} / (std::string{name} + ".fsorg-partial");

                AddASource(source);
                fileSystem.AddDirectory(staging);
                fileSystem.AddFile(staging / "half.bin", kMegabyte);
                leftovers.push_back(StagingLeftover{.staging = staging, .target = target, .source = source});
            }

            return leftovers;
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

        void TheLibraryHoldsNodes(const std::vector<TreeNode>& addons)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children = addons;

            catalog.SetTree(kLibrary, std::move(library));
        }

        void ASimbridgeIsInstalledWithTwoLinks(const std::string& version = "1.0.0", const bool withAManifest = true)
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kUtils);
            fileSystem.AddDirectory(kInLibrary);
            fileSystem.AddFile(kInLibrary / (withAManifest ? "manifest.json" : "readme.txt"), kMegabyte);
            TheLibraryHoldsNodes({AddonNodeDeclaring(kInLibrary, version)});

            fileSystem.AddDirectory(kDestination);
            fileSystem.AddLink(kInDestination, kInLibrary);
            AddALinkToTheLibraryCopyInAnotherDestination();
        }

        [[nodiscard]] std::filesystem::path TheNewPackageIsStagedIn(const std::filesystem::path& category,
                                                                    const std::uintmax_t size = 2 * kMegabyte)
        {
            const std::filesystem::path staged = InstallStagingIn(category) / "simbridge";

            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(category);
            fileSystem.AddDirectory(staged);
            fileSystem.AddFile(staged / "manifest.json", size);

            return staged;
        }

        [[nodiscard]] std::vector<OperationKind> KindsJournalledAfter(const std::size_t before) const
        {
            std::vector<OperationKind> kinds;

            for (std::size_t at = before; at < journal.appended.size(); ++at)
            {
                kinds.push_back(journal.appended[at].kind);
            }

            return kinds;
        }
    };

    class RefusesTheMoveFrom final : public FileOperations
    {
    public:
        RefusesTheMoveFrom(FileOperations& inner,
                           const std::vector<std::filesystem::path>& refused,
                           const std::vector<std::filesystem::path>& refusedInto = {})
            : inner_(inner)
        {
            for (const std::filesystem::path& path : refused)
            {
                refused_.push_back(ComparablePath(path));
            }

            for (const std::filesystem::path& path : refusedInto)
            {
                refusedInto_.push_back(ComparablePath(path));
            }
        }

        [[nodiscard]] CopyOutcome CopyTree(const std::filesystem::path& source,
                                           const std::filesystem::path& destination,
                                           const std::function<bool(const CopyProgress&)>& onProgress) override
        {
            return inner_.CopyTree(source, destination, onProgress);
        }

        [[nodiscard]] bool CreateFolder(const std::filesystem::path& path) override
        {
            return inner_.CreateFolder(path);
        }

        [[nodiscard]] bool WriteHiddenFile(const std::filesystem::path& path) override
        {
            return inner_.WriteHiddenFile(path);
        }

        [[nodiscard]] bool Move(const std::filesystem::path& source, const std::filesystem::path& destination) override
        {
            return std::ranges::find(refused_, ComparablePath(source)) == refused_.end()
                && std::ranges::find(refusedInto_, ComparablePath(destination)) == refusedInto_.end()
                && inner_.Move(source, destination);
        }

        [[nodiscard]] bool RemoveTree(const std::filesystem::path& path) override
        {
            return inner_.RemoveTree(path);
        }

        [[nodiscard]] bool Recycle(const std::filesystem::path& path) override
        {
            return inner_.Recycle(path);
        }

        [[nodiscard]] bool RemoveEmptyFolder(const std::filesystem::path& path) override
        {
            return inner_.RemoveEmptyFolder(path);
        }

    private:
        FileOperations& inner_;
        std::vector<std::string> refused_;
        std::vector<std::string> refusedInto_;
    };

    struct ServiceThatRefusesTheMoveFrom
    {
        ServiceThatRefusesTheMoveFrom(Fixture& f,
                                      const std::vector<std::filesystem::path>& refused,
                                      const std::vector<std::filesystem::path>& refusedInto = {})
            : files(f.files, refused, refusedInto),
              engine(f.filesystemProbe,
                     files,
                     f.sidecars,
                     f.linking,
                     f.log,
                     LinkType::Junction,
                     Verification::ByStructure),
              service(engine,
                      f.processProbe,
                      f.filesystemProbe,
                      f.catalog,
                      files,
                      f.sidecars,
                      f.linking,
                      f.log,
                      LinkType::Junction)
        {
        }

        ServiceThatRefusesTheMoveFrom(Fixture& f, const std::filesystem::path& refused)
            : ServiceThatRefusesTheMoveFrom(f, std::vector<std::filesystem::path>{refused})
        {
        }

        RefusesTheMoveFrom files;
        ImportEngine engine;
        ImportService service;
    };

    void ExpectTheTwoLinksStandAtTheOccupant(const Fixture& f)
    {
        QVERIFY(f.fileSystem.IsLink(kInDestination));
        QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
        QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
        QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
    }

    void ExpectTheEarlierCopyIsBack(const Fixture& f, const std::string& sidecarBefore)
    {
        QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
        QVERIFY(!f.fileSystem.Exists(SwapSlotFor(kHeldInLibrary)));
        QCOMPARE(f.TheSidecarOfTheHeldCopy(), sidecarBefore);
        QCOMPARE(OriginFromText(f.TheSidecarOfTheHeldCopy())->origin, kEarlierOrigin);
    }

    void ExpectNothingWasTouched(const Fixture& f, const std::filesystem::path& staged, const std::size_t journalBefore)
    {
        QVERIFY2(f.fileSystem.Exists(staged / "manifest.json"), "the staged folder stays where it was");
        QVERIFY(f.fileSystem.Exists(kInLibrary));
        ExpectTheTwoLinksStandAtTheOccupant(f);
        QCOMPARE(f.journal.appended.size(), journalBefore);
    }
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
    QCOMPARE(f.catalog.scanned, std::size_t{0});
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

void ImportServiceTest::ALoserAnotherProgramHoldsOpenIsNamedAsSuchAndNotAsAQuarantineThatFailed()
{
    Fixture f;
    f.AddBothCopies();
    f.files.MakeTheMoveFail();
    f.filesystemProbe.LetAnotherProgramHold(kInDestination / "service");

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::AnotherProgramIsHoldingIt);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::AnotherProgramIsHoldingIt);
    QVERIFY2(f.fileSystem.IsDirectory(kInDestination), "the losing copy stays where it is");
    QVERIFY2(!f.fileSystem.Exists(SidecarPathFor("E:/Sim/_fsorganizer-quarantine/simbridge")),
             "the record of the origin goes away with the move that never happened");
}

void ImportServiceTest::AMoveNobodyIsBlockingStillFailsAsTheQuarantineItWas()
{
    Fixture f;
    f.AddBothCopies();
    f.files.MakeTheMoveFail();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::CouldNotQuarantine);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::CouldNotQuarantine);
}

void ImportServiceTest::AQuarantineThatAlreadyHoldsThatNameSaysSoAndTouchesNothing()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("E:/Sim/_fsorganizer-quarantine");
    f.fileSystem.AddDirectory("E:/Sim/_fsorganizer-quarantine/simbridge");
    f.fileSystem.AddFile("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json", kMegabyte);

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::TheQuarantineIsOccupied);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::TheQuarantineIsOccupied);
    QCOMPARE(f.fileSystem.FileSize("E:/Sim/_fsorganizer-quarantine/simbridge/manifest.json"), kMegabyte);
    QVERIFY2(f.fileSystem.Exists(kInDestination / "manifest.json"), "the losing copy stays where it is");
}

void ImportServiceTest::AConflictWithNowhereToPutTheLoserSaysSoAndReachesTheJournal()
{
    Fixture f;
    f.AddBothCopies();

    const std::filesystem::path outside = "F:/Elsewhere/simbridge";
    f.fileSystem.AddDirectory("F:/Elsewhere");
    f.fileSystem.AddDirectory(outside);
    f.fileSystem.AddFile(outside / "manifest.json", kMegabyte);

    const CopyConflict conflict{.provenancePath = outside, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::ThereIsNowhereToQuarantineIt);
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::ThereIsNowhereToQuarantineIt);
    QCOMPARE(f.journal.appended.back().source, outside);
    QVERIFY2(f.fileSystem.Exists(outside / "manifest.json"), "the copy with nowhere to go stays where it is");
}

void ImportServiceTest::AnOccupantWithNowhereToGoStopsTheSwapAndReachesTheJournal()
{
    Fixture f;

    const std::filesystem::path outside = "F:/Elsewhere/simbridge";
    f.QuarantineHolds(kHeldInLibrary, outside, outside);
    f.fileSystem.AddDirectory("F:/Elsewhere");
    f.fileSystem.AddDirectory(outside);
    f.fileSystem.AddFile(outside / "manifest.json", 3 * kMegabyte);
    f.catalog.SetTree(outside, AddonNodeDeclaring(outside, "2.0.0"));

    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::ThereIsNowhereToQuarantineIt);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::ThereIsNowhereToQuarantineIt);
    QVERIFY2(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"), "the quarantined item never moved");
    QVERIFY2(f.fileSystem.Exists(outside / "manifest.json"), "neither did the occupant");
}

void ImportServiceTest::ASwapStoppedByAnotherProgramNamesItInsteadOfTheQuarantine()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "2.0.0"));
    f.files.MakeTheMoveFail();
    f.filesystemProbe.LetAnotherProgramHold(kHeldInLibrary);

    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::AnotherProgramIsHoldingIt);
    QVERIFY2(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"), "the item stays in the quarantine");
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

void ImportServiceTest::NothingIsPutBackWhileTheSimulatorIsRunning()
{
    Fixture f;
    AManagedExternal(f);

    const std::filesystem::path otherFolder = kVendorFolder.parent_path() / "other-addon";

    f.fileSystem.RemoveTree(kVendorFolder);
    f.fileSystem.AddDirectory(SwapSlotFor(kVendorFolder));
    f.fileSystem.AddDirectory(SwapSlotFor(otherFolder));

    const std::vector<InterruptedSwap> swaps{
        InterruptedSwap{.room = SwapSlotFor(kVendorFolder), .folder = kVendorFolder, .libraryCopy = kVendorInLibrary},
        InterruptedSwap{.room = SwapSlotFor(otherFolder), .folder = otherFolder, .libraryCopy = kVendorInLibrary}};

    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<FileOperationResult> results = f.service.UndoInterruptedSwaps(f.profile, swaps);

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results.back().result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results.front().path, kVendorFolder);
    QCOMPARE(results.back().path, otherFolder);
    QVERIFY(f.fileSystem.Exists(SwapSlotFor(kVendorFolder)));
    QVERIFY(f.fileSystem.Exists(SwapSlotFor(otherFolder)));
    QVERIFY(!f.fileSystem.Exists(kVendorFolder));
    QVERIFY(!f.fileSystem.Exists(otherFolder));
    QVERIFY(f.journal.appended.empty());
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

void ImportServiceTest::NothingIsResumedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory("D:/Library/Sceneries/orbx-yssy.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Sceneries/orbx-yssy.fsorg-partial/half.bin", kMegabyte);
    f.processProbe.ReportTheSimulatorAsRunning();

    const StagingLeftover leftover{.staging = "D:/Library/Sceneries/orbx-yssy.fsorg-partial",
                                   .target = "D:/Library/Sceneries/orbx-yssy",
                                   .source = kInDestination};

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, {leftover, leftover}, {});

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results.back().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists("D:/Library/Sceneries/orbx-yssy.fsorg-partial/half.bin"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/orbx-yssy"));
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::ALeftoverWhoseNameAnotherLeftoverOfTheGestureLandedElsewhereIsRefusedAsItsTwin()
{
    Fixture f;
    const std::filesystem::path first = "E:/Sim/Community/simbridge";
    const std::filesystem::path second = kLinkedElsewhere;
    const std::filesystem::path firstStaging = "D:/Library/Utils/simbridge.fsorg-partial";
    const std::filesystem::path secondStaging = "D:/Library/Sceneries/simbridge.fsorg-partial";

    f.profile.destinations.push_back(kOtherDestination);
    f.AddASource(first);
    f.AddASource(second);
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory("D:/Library/Utils");
    f.fileSystem.AddDirectory("D:/Library/Sceneries");
    f.fileSystem.AddDirectory(firstStaging);
    f.fileSystem.AddFile(firstStaging / "half.bin", kMegabyte);
    f.fileSystem.AddDirectory(secondStaging);
    f.fileSystem.AddFile(secondStaging / "half.bin", kMegabyte);

    const std::vector<ImportOperationResult> results = f.service.Resume(
        f.profile,
        {StagingLeftover{.staging = firstStaging, .target = "D:/Library/Utils/simbridge", .source = first},
         StagingLeftover{.staging = secondStaging, .target = "D:/Library/Sceneries/simbridge", .source = second}},
        {});

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(results.back().occupant, std::filesystem::path{"D:/Library/Utils/simbridge"});
    QVERIFY(f.fileSystem.Exists(secondStaging / "half.bin"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/simbridge"));
    QVERIFY(f.fileSystem.Exists(second / "manifest.json"));
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

void ImportServiceTest::NothingIsDiscardedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory("D:/Library/Utils/imported.fsorg-partial");
    f.fileSystem.AddFile("D:/Library/Utils/imported.fsorg-partial/manifest.json", kMegabyte);
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<FileOperationResult> results =
        f.service.DiscardLeftovers(f.profile, f.service.Leftovers(f.profile));

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists("D:/Library/Utils/imported.fsorg-partial/manifest.json"));
    QVERIFY(f.journal.appended.empty());
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

void ImportServiceTest::ASideTheWalkCouldNotReadHasNoSizeInsteadOfZero()
{
    Fixture f;
    f.AddBothCopies();
    f.filesystemProbe.RefuseToWalk(kInDestination);

    const ConflictDetails details = f.service.DetailsOf(f.Entries(), CopyConflict{kInDestination, kInLibrary});

    QVERIFY2(!details.provenance.sizeBytes.has_value(), "a tree nobody could walk was not measured, it is not empty");
    QCOMPARE(details.library.sizeBytes, std::optional<std::uintmax_t>{kMegabyte});
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

void ImportServiceTest::AFailedRestoreAfterTheOccupantMovedLeavesItsLinksDown()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    const std::size_t journalBefore = f.journal.appended.size();

    f.files.MakeTheMoveFailAfter(2);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QCOMPARE(swapped.result, FileResult::CouldNotRestore);
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
    QVERIFY(!f.fileSystem.Exists(kInDestination));
    QVERIFY(!f.fileSystem.Exists(kLinkedElsewhere));
    QCOMPARE(std::ranges::count(f.KindsJournalledAfter(journalBefore), OperationKind::EnableAddon), 0);
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

void ImportServiceTest::TheSwapPutsTheLinksOfTheOccupantBackOnTheRestoredCopy()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.fileSystem.AddDirectory(kDestination);
    f.fileSystem.AddLink(kInDestination, kInLibrary);
    const std::size_t journalBefore = f.journal.appended.size();

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    QVERIFY(f.fileSystem.IsLink(kInDestination));
    QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);

    const std::vector<OperationKind> kinds = f.KindsJournalledAfter(journalBefore);
    const auto kindAt = [&kinds](const OperationKind kind)
    {
        return std::ranges::find(kinds, kind) - kinds.begin();
    };

    QVERIFY(kindAt(OperationKind::DisableAddon) < kindAt(OperationKind::QuarantineFromLibrary));
    QVERIFY(kindAt(OperationKind::QuarantineFromLibrary) < kindAt(OperationKind::RestoreOverTheOccupant));
    QVERIFY(kindAt(OperationKind::RestoreOverTheOccupant) < kindAt(OperationKind::EnableAddon));
    QCOMPARE(kinds.back(), OperationKind::EnableAddon);
}

void ImportServiceTest::TheSwapPutsBothLinksOfTheOccupantBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::TheSwapOfAnOccupantWithoutLinksCreatesNone()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.fileSystem.AddDirectory(kDestination);
    const std::size_t journalBefore = f.journal.appended.size();

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    QVERIFY(!f.fileSystem.Exists(kInDestination));

    const std::vector<OperationKind> kinds = f.KindsJournalledAfter(journalBefore);

    QCOMPARE(std::ranges::count(kinds, OperationKind::EnableAddon), 0);
    QCOMPARE(std::ranges::count(kinds, OperationKind::DisableAddon), 0);
}

void ImportServiceTest::ASwapStillSucceedsWhenTheLinksCannotBePutBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);
    f.linkService.MakeLinkCreationFail();

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QVERIFY(swapped.Succeeded());
    QCOMPARE(swapped.inTheLibrary, kInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 3 * kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kInDestination));
    QVERIFY(!f.fileSystem.Exists(kLinkedElsewhere));

    const auto relinks =
        std::ranges::count_if(f.journal.appended,
                              [](const OperationRecord& record)
                              {
                                  return record.kind == OperationKind::EnableAddon && !Succeeded(record.outcome);
                              });

    QCOMPARE(relinks, 2);
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

    QCOMPARE(refused, FileResult::TheQuarantineIsOccupied);
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

void ImportServiceTest::AWinnerThatCannotTakeTheLosersPlaceStillGetsTheOtherLinksBack()
{
    Fixture f;
    ADivergentManagedExternal(f);
    const std::filesystem::path linkedToTheVendorFolder = kOtherDestination / "gsx-pro";
    f.profile.destinations.push_back(kOtherDestination);
    f.fileSystem.AddDirectory(kOtherDestination);
    f.fileSystem.AddLink(linkedToTheVendorFolder, kVendorFolder);
    f.linkService.MakeLinkCreationFail();

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve(f.profile.destinations, {kLibrary},
                             {ExternalAddon{.addonFolder = kVendorInLibrary, .externalPath = kVendorFolder}});

    QCOMPARE(LinksPointingAt(entries, kVendorFolder), (std::vector<std::filesystem::path>{linkedToTheVendorFolder}));

    const FileResult result =
        f.service.ResolveConflict(f.profile, entries, TheDivergence(), ConflictChoice::KeepTheLibraryCopy);

    QCOMPARE(result, FileResult::CouldNotCreateLink);

    const auto triedToLinkBackAt = [&f](const std::filesystem::path& place)
    {
        return static_cast<int>(std::ranges::count_if(f.journal.appended,
                                                      [&place](const OperationRecord& record)
                                                      {
                                                          return record.kind == OperationKind::EnableAddon
                                                              && record.target == place
                                                              && record.source == kVendorInLibrary;
                                                      }));
    };

    QCOMPARE(triedToLinkBackAt(linkedToTheVendorFolder), 1);
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

void ImportServiceTest::TakingBackWhatReplacedTheLinkPutsTheDestinationCopyInTheLibraryAndLinksItAgain()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};

    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY2(f.fileSystem.IsLink(kInDestination), "the link the installer wrote over is back where it was");
    QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
}

void ImportServiceTest::TakingItBackLeavesTheOldLibraryCopyInQuarantineInsteadOfDeletingIt()
{
    Fixture f;
    f.AddBothCopies();

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};

    QCOMPARE(f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy),
             FileResult::Completed);

    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 1 * kMegabyte);
    QVERIFY2(f.fileSystem.FileSize(kInLibrary / "manifest.json") == 2 * kMegabyte,
             "the copy that took the place is the one from the destination, and the old one is only set aside");
}

void ImportServiceTest::AnImportOfManyFoldersScansTheLibraryOnce()
{
    Fixture f;
    const std::vector<ImportRequest> requests = f.ThreeSourcesIntoUtils();

    const std::vector<ImportOperationResult> results = f.service.Import(f.profile, requests, {});

    QCOMPARE(results.size(), std::size_t{3});
    for (const ImportOperationResult& result : results)
    {
        QCOMPARE(result.result, FileResult::Completed);
    }
    QCOMPARE(f.catalog.scanned, f.profile.libraries.size());
}

void ImportServiceTest::AFolderWhoseNameTheBatchAlreadyLandedElsewhereIsRefusedAsItsTwin()
{
    Fixture f;
    const std::filesystem::path first = "E:/Sim/Community/simbridge";
    const std::filesystem::path second = kLinkedElsewhere;

    f.profile.destinations.push_back(kOtherDestination);
    f.AddASource(first);
    f.AddASource(second);
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory("D:/Library/Utils");
    f.fileSystem.AddDirectory("D:/Library/Sceneries");

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile,
                         {ImportRequest{.source = first, .category = "D:/Library/Utils"},
                          ImportRequest{.source = second, .category = "D:/Library/Sceneries"}},
                         {});

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::TheIdentityIsTaken);
    QCOMPARE(results.back().occupant, std::filesystem::path{"D:/Library/Utils/simbridge"});
    QVERIFY(f.fileSystem.Exists(second / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/simbridge"));
    QVERIFY(!f.fileSystem.Exists("D:/Library/Sceneries/simbridge.fsorg-partial"));
    QVERIFY(std::ranges::none_of(f.journal.appended,
                                 [&second](const OperationRecord& record)
                                 {
                                     return record.source == second;
                                 }));
}

void ImportServiceTest::AFolderWhoseNameTheBatchLandedInAnotherLibraryIsNotATwin()
{
    Fixture f;
    const std::filesystem::path first = "E:/Sim/Community/simbridge";
    const std::filesystem::path second = "E:/Sim/Community2024/simbridge";

    f.profile.libraries.push_back(Library{.id = "lib-2", .path = "F:/Spare"});
    f.profile.destinations.push_back(kOtherDestination);
    f.AddASource(first);
    f.AddASource(second);
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory("D:/Library/Utils");
    f.fileSystem.AddDirectory("F:/Spare");
    f.fileSystem.AddDirectory("F:/Spare/Utils");

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile,
                         {ImportRequest{.source = first, .category = "D:/Library/Utils"},
                          ImportRequest{.source = second, .category = "F:/Spare/Utils"}},
                         {});

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::Completed);
}

void ImportServiceTest::ARequestTheCallerStopsAtAndEveryOneAfterItAreCancelledUntouched()
{
    Fixture f;
    const std::vector<ImportRequest> requests = f.ThreeSourcesIntoUtils();
    std::vector<std::size_t> asked;

    const std::vector<ImportOperationResult> results = f.service.Import(f.profile, requests, {}, {},
                                                                        [&asked](const std::size_t request)
                                                                        {
                                                                            asked.push_back(request);

                                                                            return request < 1;
                                                                        });

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::Cancelled);
    QCOMPARE(results[2].result, FileResult::Cancelled);
    QCOMPARE(asked, (std::vector<std::size_t>{0, 1}));
    QVERIFY(f.fileSystem.Exists(requests[1].source / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(requests[2].source / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(requests[1].Target()));
    QVERIFY(!f.fileSystem.Exists(requests[2].Target()));
}

void ImportServiceTest::TheSimulatorStartingBetweenTwoRequestsRefusesTheRest()
{
    Fixture f;
    const std::vector<ImportRequest> requests = f.ThreeSourcesIntoUtils();

    const std::vector<ImportOperationResult> results =
        f.service.Import(f.profile, requests, {}, {},
                         [&f](const std::size_t request)
                         {
                             if (request == 1)
                             {
                                 f.processProbe.ReportTheSimulatorAsRunning();
                             }

                             return true;
                         });

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results[2].result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(requests[1].source / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(requests[1].Target()));
    QVERIFY(!f.fileSystem.Exists(requests[2].Target()));
}

void ImportServiceTest::AResumeStopsAtTheLeftoverTheCallerStopsAtAndCancelsEveryOneAfterIt()
{
    Fixture f;
    const std::vector<StagingLeftover> leftovers = f.ThreeLeftoversIntoUtils();
    std::vector<std::size_t> asked;

    const std::vector<ImportOperationResult> results = f.service.Resume(f.profile, leftovers, {}, {},
                                                                        [&asked](const std::size_t request)
                                                                        {
                                                                            asked.push_back(request);

                                                                            return request < 1;
                                                                        });

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::Cancelled);
    QCOMPARE(results[2].result, FileResult::Cancelled);
    QCOMPARE(asked, (std::vector<std::size_t>{0, 1}));
    QVERIFY(f.fileSystem.Exists(leftovers[1].staging / "half.bin"));
    QVERIFY(f.fileSystem.Exists(leftovers[2].staging / "half.bin"));
    QVERIFY(!f.fileSystem.Exists(leftovers[1].target));
    QVERIFY(!f.fileSystem.Exists(leftovers[2].target));
}

void ImportServiceTest::TheSimulatorStartingBetweenTwoLeftoversRefusesTheRest()
{
    Fixture f;
    const std::vector<StagingLeftover> leftovers = f.ThreeLeftoversIntoUtils();

    const std::vector<ImportOperationResult> results =
        f.service.Resume(f.profile, leftovers, {}, {},
                         [&f](const std::size_t request)
                         {
                             if (request == 1)
                             {
                                 f.processProbe.ReportTheSimulatorAsRunning();
                             }

                             return true;
                         });

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results[2].result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(leftovers[1].staging / "half.bin"));
    QVERIFY(f.fileSystem.Exists(leftovers[2].staging / "half.bin"));
    QVERIFY(!f.fileSystem.Exists(leftovers[1].target));
    QVERIFY(!f.fileSystem.Exists(leftovers[2].target));
}

void ImportServiceTest::AnItemStrandedInItsSwapSlotIsListedWithTheOriginOfItsRealName()
{
    Fixture f;
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(SwapSlotFor(kHeldInLibrary));
    f.fileSystem.AddFile(SwapSlotFor(kHeldInLibrary) / "manifest.json", kMegabyte);

    const std::vector<QuarantinedItem> items = f.service.Quarantined(f.profile);

    QCOMPARE(items.size(), std::size_t{2});

    const auto stranded = std::ranges::find_if(items,
                                               [](const QuarantinedItem& item)
                                               {
                                                   return item.path == SwapSlotFor(kHeldInLibrary);
                                               });
    QVERIFY(stranded != items.end());
    QCOMPARE(stranded->origin, kInLibrary);
    QCOMPARE(stranded->source, OriginSource::Sidecar);
    QVERIFY(stranded->quarantinedAt.has_value());
}

void ImportServiceTest::AnItemStrandedInItsSwapSlotIsOfferedEveryPlaceUnderItsRealName()
{
    Fixture f;
    f.AddBothCopies();
    f.fileSystem.AddDirectory(SwapSlotFor(kHeldInLibrary));
    f.TheLibraryIsShapedLike({"D:/Library/Utils"});

    const std::vector<RestorePlace> places =
        f.service.PlacesFor(f.profile, QuarantinedItem{.path = SwapSlotFor(kHeldInLibrary)});

    QCOMPARE(places.size(), std::size_t{2});
    QCOMPARE(places.front().target, std::filesystem::path{"D:/Library/simbridge"});
    QCOMPARE(places.back().target, std::filesystem::path{"D:/Library/Utils/simbridge"});
}

void ImportServiceTest::AnItemStrandedInItsSwapSlotIsMarkedAsReplacedByTheDestinationFolderOfItsRealName()
{
    Fixture f;
    const std::filesystem::path stranded = SwapSlotFor("E:/Sim/_fsorganizer-quarantine/simbridge");

    f.fileSystem.AddDirectory(stranded);

    const std::vector<QuarantineDetail> details = f.service.Describe(
        {DestinationEntry{.path = kInDestination, .target = {}, .classification = EntryClassification::Unmanaged}},
        {QuarantinedItem{.path = stranded}});

    QCOMPARE(details.front().path, stranded);
    QVERIFY(details.front().WasReplaced());
    QCOMPARE(details.front().replacedBy, kInDestination);
}

void ImportServiceTest::OffersForManyItemsScanTheirLibraryOnce()
{
    Fixture f;
    const std::filesystem::path quarantine = "D:/Library/_fsorganizer-quarantine";

    f.AddBothCopies();
    f.fileSystem.AddDirectory(quarantine);
    f.TheLibraryIsShapedLike({"D:/Library/Sceneries", "D:/Library/Utils"});

    std::vector<QuarantinedItem> items;
    for (const char* name : {"first", "second", "third"})
    {
        items.push_back(QuarantinedItem{.path = quarantine / name});
    }
    items.push_back(QuarantinedItem{.path = quarantine / "kept", .origin = "D:/Library/Utils/kept"});

    f.catalog.scanned = 0;

    const std::vector<RestoreOffer> offers = f.service.OffersFor(f.profile, items);

    QCOMPARE(offers.size(), std::size_t{4});
    QCOMPARE(f.catalog.scanned, f.profile.libraries.size());
    for (std::size_t item = 0; item < 3; ++item)
    {
        QCOMPARE(offers[item].check.result, FileResult::TheOriginIsUnknown);
        QCOMPARE(offers[item].places.size(), std::size_t{3});
        QCOMPARE(offers[item].places[1].target, "D:/Library/Sceneries" / items[item].path.filename());
    }
    QVERIFY(offers[3].check.CanProceed());
    QVERIFY(offers[3].places.empty());
}

void ImportServiceTest::AFreshFolderIsAdmittedWhereItWasAsked()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory(kUtils);
    f.TheLibraryHoldsNodes({});

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(f.profile, {kUtils / "simbridge"});

    QCOMPARE(checks.size(), std::size_t{1});
    QCOMPARE(checks.front().result, FileResult::Completed);
    QCOMPARE(checks.front().target, kInLibrary);
    QVERIFY(checks.front().occupant.empty());
    QVERIFY(checks.front().occupantVersion.empty());
    QVERIFY(!checks.front().theQuarantineHoldsAnEarlierCopy);
}

void ImportServiceTest::AnInstalledPackageIsTheOccupantAndReportsItsVersionAndWhereItIs()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.2.3");
    f.fileSystem.AddDirectory(kSceneries);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(f.profile, {kSceneries / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::Completed);
    QCOMPARE(checks.front().target, kInLibrary);
    QCOMPARE(checks.front().occupant, kInLibrary);
    QCOMPARE(checks.front().occupantVersion, std::string{"1.2.3"});
    QVERIFY(!checks.front().theQuarantineHoldsAnEarlierCopy);
    QVERIFY(checks.front().quarantinedVersion.empty());
    QVERIFY(checks.front().quarantinedCopy.empty());
    QCOMPARE(f.journal.appended.size(), journalBefore);
    QVERIFY(f.fileSystem.IsLink(kInDestination));
}

void ImportServiceTest::ASamenamedAddonInAnotherLibraryIsNotTheOccupant()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.profile.libraries.push_back(Library{.id = "lib-2", .path = "F:/Spare"});
    f.fileSystem.AddDirectory("F:/Spare");
    f.fileSystem.AddDirectory("F:/Spare/Utils");

    const std::vector<AdmissionCheck> checks =
        f.service.CheckAdmission(f.profile, {std::filesystem::path{"F:/Spare/Utils"} / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::Completed);
    QVERIFY(checks.front().occupant.empty());
    QCOMPARE(checks.front().target, std::filesystem::path{"F:/Spare/Utils/simbridge"});
}

void ImportServiceTest::AnOccupantWithoutAManifestRefusesTheAdmission()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("", false);

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(f.profile, {kUtils / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(checks.front().occupant, kInLibrary);
}

void ImportServiceTest::SomethingThatIsNotAnAddonAtTheWantedPathRefusesTheAdmission()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory(kUtils);
    f.fileSystem.AddFile(kInLibrary / "readme.txt", kMegabyte);
    f.TheLibraryHoldsNodes({});

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(f.profile, {kUtils / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(checks.front().occupant, kInLibrary);
}

void ImportServiceTest::ACategoryOutsideEveryLibraryRefusesTheAdmission()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();

    const std::vector<AdmissionCheck> checks =
        f.service.CheckAdmission(f.profile, {std::filesystem::path{"F:/Elsewhere"} / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::TheTargetIsNotInALibrary);
    QVERIFY(checks.front().occupant.empty());
}

void ImportServiceTest::NothingIsAdmittedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<AdmissionCheck> checks =
        f.service.CheckAdmission(f.profile, {kUtils / "simbridge", std::filesystem::path{"F:/Elsewhere"} / "other"});

    QCOMPARE(checks.size(), std::size_t{2});
    QCOMPARE(checks[0].result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(checks[1].result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.catalog.scanned, std::size_t{0});
}

void ImportServiceTest::AnEarlierCopyInTheQuarantineIsReportedByTheAdmissionCheck()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kHeldInLibrary, AddonNodeDeclaring(kHeldInLibrary, "0.9.4"));

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(f.profile, {kUtils / "simbridge"});

    QCOMPARE(checks.front().result, FileResult::Completed);
    QVERIFY(checks.front().theQuarantineHoldsAnEarlierCopy);
    QCOMPARE(checks.front().quarantinedVersion, std::string{"0.9.4"});
    QCOMPARE(checks.front().quarantinedCopy, kHeldInLibrary);
    QCOMPARE(checks.front().occupantVersion, std::string{"1.0.0"});
}

void ImportServiceTest::CheckingManyPathsScansTheLibrariesOnce()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();

    const std::vector<AdmissionCheck> checks = f.service.CheckAdmission(
        f.profile, {kUtils / "simbridge", kUtils / "alpha", kSceneries / "beta", kSceneries / "simbridge"});

    QCOMPARE(checks.size(), std::size_t{4});
    QCOMPARE(f.catalog.scanned, f.profile.libraries.size());
}

void ImportServiceTest::AdmittingANewFolderMovesItIntoPlaceAndJournalsIt()
{
    Fixture f;
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    f.TheLibraryHoldsNodes({});

    const std::vector<AdmitResult> results =
        f.service.Admit(f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.front().landed, kInLibrary);
    QVERIFY(!results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QVERIFY(!f.fileSystem.Exists(staged));
    QCOMPARE(f.journal.appended.size(), std::size_t{1});
    QCOMPARE(f.journal.appended.front().kind, OperationKind::InstallMoveIntoPlace);
    QCOMPARE(f.journal.appended.front().source, staged);
    QCOMPARE(f.journal.appended.front().target, kInLibrary);
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::Completed);
}

void ImportServiceTest::AdmittingOverAnInstalledPackageQuarantinesItAndLinksTheNewOneInItsPlaces()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.front().landed, kInLibrary);
    QVERIFY(results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QVERIFY(!f.fileSystem.Exists(staged));
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);

    const std::vector<QuarantinedItem> quarantined = f.service.Quarantined(f.profile);
    QCOMPARE(quarantined.size(), std::size_t{1});
    QCOMPARE(quarantined.front().path, kHeldInLibrary);
    QCOMPARE(quarantined.front().origin, kInLibrary);

    ExpectTheTwoLinksStandAtTheOccupant(f);
    QCOMPARE(f.KindsJournalledAfter(journalBefore),
             (std::vector<OperationKind>{OperationKind::DisableAddon, OperationKind::DisableAddon,
                                         OperationKind::QuarantineFromLibrary, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::EnableAddon, OperationKind::EnableAddon}));
}

void ImportServiceTest::AnAdmissionWhileTheSimulatorRunsTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    f.processProbe.ReportTheSimulatorAsRunning();
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(results.front().landed.empty());
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AnOccupiedQuarantineRefusesTheAdmissionAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::TheQuarantineIsOccupied);
    QVERIFY(!results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AnAdmissionOutsideEveryLibraryTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn("F:/Elsewhere");
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results =
        f.service.Admit(f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = "F:/Elsewhere"}});

    QCOMPARE(results.front().result, FileResult::TheTargetIsNotInALibrary);
    QVERIFY(!f.fileSystem.Exists("F:/Elsewhere/simbridge"));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AnOccupantWithoutAManifestIsNeverReplacedAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("", false);
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::TheOriginIsOccupied);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "readme.txt"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kInLibrary / "manifest.json"));
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AnOccupantThatAppearedAfterTheOfferIsNotReplacedAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results =
        f.service.Admit(f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils}});

    QCOMPARE(results.front().result, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(results.front().landed.empty());
    QVERIFY(!results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AnOccupantOtherThanTheOneAcceptedIsNotReplacedAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kLibrary / "Elsewhere" / "simbridge"}});

    QCOMPARE(results.front().result, FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AStagedFolderOnAnotherVolumeIsRefusedAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = std::filesystem::path{"F:/Stage"} / "simbridge";
    f.fileSystem.AddFile(staged / "manifest.json", 2 * kMegabyte);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotMoveIntoPlace);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectNothingWasTouched(f, staged, journalBefore);
}

void ImportServiceTest::AStagedFolderThatIsGoneIsRefusedAndTouchesNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    QVERIFY(f.files.RemoveTree(staged));
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotMoveIntoPlace);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
    QCOMPARE(f.journal.appended.size(), journalBefore);
}

void ImportServiceTest::AnOccupantWithNowhereToGoIsRefusedAndTouchesNothing()
{
    Fixture f;
    const std::filesystem::path outside = std::filesystem::path{"F:/Elsewhere"} / "simbridge";
    f.fileSystem.AddFile(outside / "manifest.json", kMegabyte);
    f.fileSystem.AddDirectory(kLibrary);
    f.fileSystem.AddDirectory(kUtils);
    f.TheLibraryHoldsNodes({AddonNodeDeclaring(outside, "1.0.0")});
    const std::filesystem::path staged = std::filesystem::path{"F:/Stage"} / "simbridge";
    f.fileSystem.AddFile(staged / "manifest.json", 2 * kMegabyte);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = outside}});

    QCOMPARE(results.front().result, FileResult::ThereIsNowhereToQuarantineIt);
    QCOMPARE(f.fileSystem.FileSize(outside / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    QVERIFY(f.journal.appended.empty());
}

void ImportServiceTest::AskingToDiscardTheEarlierCopyDiscardsItOnlyAfterTheNewFolderLanded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(SwapSlotFor(kHeldInLibrary)));
    QCOMPARE(OriginFromText(f.TheSidecarOfTheHeldCopy())->origin, kInLibrary);

    const std::vector<QuarantinedItem> quarantined = f.service.Quarantined(f.profile);
    QCOMPARE(quarantined.size(), std::size_t{1});
    QCOMPARE(quarantined.front().origin, kInLibrary);
    QCOMPARE(f.KindsJournalledAfter(journalBefore),
             (std::vector<OperationKind>{OperationKind::DisableAddon, OperationKind::DisableAddon,
                                         OperationKind::QuarantineFromLibrary, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::EnableAddon, OperationKind::EnableAddon,
                                         OperationKind::DiscardFromQuarantine}));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::AnEarlierCopyComesBackWhenALinkCannotComeDown()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::string sidecarBefore = f.TheSidecarOfTheHeldCopy();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    f.linkService.MakeTheRemovalFailFor(kLinkedElsewhere);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::CouldNotRemoveTheLink);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    ExpectTheEarlierCopyIsBack(f, sidecarBefore);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::AnEarlierCopyComesBackWhenTheOccupantCannotBeQuarantined()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::string sidecarBefore = f.TheSidecarOfTheHeldCopy();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    ServiceThatRefusesTheMoveFrom refusing(f, kInLibrary);

    const std::vector<AdmitResult> results = refusing.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::CouldNotQuarantine);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    ExpectTheEarlierCopyIsBack(f, sidecarBefore);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::AnEarlierCopyComesBackWhenTheNewFolderCannotMoveIntoPlace()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::string sidecarBefore = f.TheSidecarOfTheHeldCopy();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    ServiceThatRefusesTheMoveFrom refusing(f, staged);

    const std::vector<AdmitResult> results = refusing.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::CouldNotMoveIntoPlace);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    ExpectTheEarlierCopyIsBack(f, sidecarBefore);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::AnEarlierCopyThatCannotBeSetAsideStopsTheAdmissionUntouched()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::string sidecarBefore = f.TheSidecarOfTheHeldCopy();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    ServiceThatRefusesTheMoveFrom refusing(f, kHeldInLibrary);

    const std::vector<AdmitResult> results = refusing.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::CouldNotQuarantine);
    QVERIFY(results.front().landed.empty());
    ExpectNothingWasTouched(f, staged, journalBefore);
    ExpectTheEarlierCopyIsBack(f, sidecarBefore);
}

void ImportServiceTest::ARestoreThatAlsoFailsLeavesTheOccupantInQuarantineAndTheEarlierCopyBesideIt()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    ServiceThatRefusesTheMoveFrom refusing(f, std::vector<std::filesystem::path>{}, {kInLibrary});

    const std::vector<AdmitResult> results = refusing.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::CouldNotMoveIntoPlace);
    QVERIFY(results.front().landed.empty());
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    QVERIFY2(!f.fileSystem.IsLink(kInDestination) && !f.fileSystem.IsLink(kLinkedElsewhere),
             "no link is recreated over a path that holds nothing");
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(SwapSlotFor(kHeldInLibrary) / "manifest.json"), 3 * kMegabyte);
    QCOMPARE(f.KindsJournalledAfter(journalBefore),
             (std::vector<OperationKind>{OperationKind::DisableAddon, OperationKind::DisableAddon,
                                         OperationKind::QuarantineFromLibrary, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::RestoreFromQuarantine}));
    QCOMPARE(std::get<FileResult>(f.journal.appended[journalBefore + 3].outcome), FileResult::CouldNotMoveIntoPlace);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::CouldNotRestore);
}

void ImportServiceTest::AnEarlierCopyThatCannotBeDiscardedStaysBesideTheSlotAndTheAdmissionStillSucceeds()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.AnEarlierCopyIsQuarantined();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    f.files.MakeTheRemovalFailFor(SwapSlotFor(kHeldInLibrary));

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{
            .staged = staged, .category = kUtils, .replaces = kInLibrary, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(SwapSlotFor(kHeldInLibrary) / "manifest.json"), 3 * kMegabyte);
    QCOMPARE(f.journal.appended.back().kind, OperationKind::DiscardFromQuarantine);
    QCOMPARE(std::get<FileResult>(f.journal.appended.back().outcome), FileResult::CouldNotDiscard);
    QCOMPARE(OriginFromText(f.TheSidecarOfTheHeldCopy())->origin, kInLibrary);

    const std::vector<QuarantinedItem> quarantined = f.service.Quarantined(f.profile);
    QCOMPARE(quarantined.size(), std::size_t{2});
    QVERIFY(std::ranges::any_of(quarantined,
                                [](const QuarantinedItem& item)
                                {
                                    return item.path == SwapSlotFor(kHeldInLibrary);
                                }));
}

void ImportServiceTest::EveryLinkIsTriedEvenWhenTheFirstCannotBeCreated()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    f.linkService.MakeLinkCreationFail();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotCreateLink);

    const std::vector<OperationKind> kinds = f.KindsJournalledAfter(journalBefore);
    QCOMPARE(std::ranges::count(kinds, OperationKind::EnableAddon), 2);
}

void ImportServiceTest::AConflictQuarantineThatFailsPutsTheLinksThatCameDownBack()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.files.MakeTheMoveFail();

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::CouldNotQuarantine);
    QVERIFY(f.fileSystem.Exists(kInLibrary / "manifest.json"));
    QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
}

void ImportServiceTest::AConflictLinkThatCannotComeDownHalfwayPutsTheOnesThatDidBack()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.AddALinkToTheLibraryCopyInAThirdDestination();
    f.linkService.MakeTheRemovalFailFor(kLinkedInTheThird);

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::CouldNotRemoveTheLink);
    QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
    QVERIFY(f.fileSystem.IsLink(kLinkedInTheThird));
}

void ImportServiceTest::TakingBackAReplacedLinkWhoseQuarantineFailsPutsTheLinksBack()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.files.MakeTheMoveFail();

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::CouldNotQuarantine);
    QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
}

void ImportServiceTest::TakingBackAReplacedLinkWhoseLinksCannotComeDownHalfwayPutsTheOnesThatDidBack()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.AddALinkToTheLibraryCopyInAThirdDestination();
    f.linkService.MakeTheRemovalFailFor(kLinkedInTheThird);

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::CouldNotRemoveTheLink);
    QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
    QVERIFY(f.fileSystem.IsLink(kLinkedInTheThird));
}

void ImportServiceTest::TakingBackAReplacedLinkWhoseImportFailsBringsTheLibraryCopyAndItsLinksBack()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.files.MakeTheCopyFailPartWayThrough();

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QVERIFY(!Succeeded(result));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    QVERIFY(f.service.Quarantined(f.profile).empty());
    QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
    QVERIFY(f.fileSystem.Exists(kInDestination / "manifest.json"));
}

void ImportServiceTest::ASwapWhoseLinksCannotComeDownHalfwayPutsTheOnesThatDidBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    f.linkService.MakeTheRemovalFailFor(kLinkedElsewhere);

    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), f.service.Quarantined(f.profile).front());

    QCOMPARE(swapped.result, FileResult::CouldNotRemoveTheLink);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::ASwapThatCannotMoveTheItemIntoItsSlotPutsTheLinksBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    ServiceThatRefusesTheMoveFrom refusing(f, kHeldInLibrary);

    const SwapResult swapped = refusing.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::CouldNotQuarantine);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::ASwapWhoseOccupantCannotBeQuarantinedPutsTheLinksBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    ServiceThatRefusesTheMoveFrom refusing(f, kInLibrary);

    const SwapResult swapped = refusing.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::CouldNotQuarantine);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(SwapSlotFor(kHeldInLibrary)));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::ASwapWhoseOccupantCannotBeQuarantinedLeavesTheItemWithItsOwnRecordOfOrigin()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    const std::string sidecarBefore = f.TheSidecarOfTheHeldCopy();
    ServiceThatRefusesTheMoveFrom refusing(f, kInLibrary);

    const SwapResult swapped = refusing.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::CouldNotQuarantine);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(SwapSlotFor(kHeldInLibrary)));
    QVERIFY(!sidecarBefore.empty());
    QCOMPARE(f.TheSidecarOfTheHeldCopy(), sidecarBefore);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ImportServiceTest::ASwapWhoseOccupantCannotBeQuarantinedLeavesAnItemWithoutARecordWithoutOne()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.QuarantineHolds(kHeldInLibrary, kInLibrary, kInLibrary);
    f.catalog.SetTree(kInLibrary, AddonNodeDeclaring(kInLibrary, "1.0.0"));
    const QuarantinedItem item = f.service.Quarantined(f.profile).front();
    QVERIFY(f.sidecars.Forget(SidecarPathFor(kHeldInLibrary)));
    ServiceThatRefusesTheMoveFrom refusing(f, kInLibrary);

    const SwapResult swapped = refusing.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::CouldNotQuarantine);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(SidecarPathFor(kHeldInLibrary)));
}

void ImportServiceTest::TheRecordOfAnotherProgramBesideAReplacedFolderStaysBesideTheNewOne()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path vendorFolder = "C:/Addon Manager/MSFS/simbridge";
    const std::string recorded = TextOfTheExternalOrigin(vendorFolder);
    QVERIFY(f.sidecars.Write(ExternalSidecarPathFor(kInLibrary), recorded));
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QCOMPARE(f.sidecars.Read(ExternalSidecarPathFor(kInLibrary)).value_or(std::string{}), recorded);
    QVERIFY(!f.sidecars.Read(ExternalSidecarPathFor(kHeldInLibrary)).has_value());
}

void ImportServiceTest::ALeftoverInTheInstallStagingIsNeverOfferedAsResumableFromAnOldImportRecord()
{
    Fixture f;
    f.AddBothCopies();
    const std::filesystem::path staging = InstallStagingIn(kUtils);
    f.fileSystem.AddDirectory(staging);
    f.fileSystem.AddFile(staging / "simbridge" / "manifest.json", kMegabyte);

    f.journal.Append(OperationRecord::OfImport(f.clock.now, OperationKind::ImportCopyToStaging,
                                               AddonId{.libraryId = "lib-1", .folderName = "_install"},
                                               "E:/Sim/Community/_install", staging, FileResult::Completed));

    const std::vector<StagingLeftover> leftovers = f.service.Leftovers(f.profile);

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, staging);
    QVERIFY(leftovers.front().source.empty());
    QVERIFY2(!leftovers.front().CanBeResumed(), "the install staging is only ever discardable");
    QCOMPARE(f.service.DiscardLeftovers(f.profile, leftovers).front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(staging));
}

void ImportServiceTest::AMoveThatFailsAfterTheQuarantineBringsTheOccupantAndItsLinksBack()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    ServiceThatRefusesTheMoveFrom refusing(f, staged);

    const std::vector<AdmitResult> results = refusing.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotMoveIntoPlace);
    QVERIFY(results.front().landed.empty());
    QVERIFY(!results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    QVERIFY(f.service.Quarantined(f.profile).empty());
    ExpectTheTwoLinksStandAtTheOccupant(f);
    QCOMPARE(f.KindsJournalledAfter(journalBefore),
             (std::vector<OperationKind>{OperationKind::DisableAddon, OperationKind::DisableAddon,
                                         OperationKind::QuarantineFromLibrary, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::RestoreFromQuarantine, OperationKind::EnableAddon,
                                         OperationKind::EnableAddon}));
}

void ImportServiceTest::AQuarantineThatFailsLeavesTheOccupantAndRelinksIt()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    f.files.MakeTheMoveFail();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotQuarantine);
    QVERIFY(!results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);

    const std::vector<OperationKind> kinds = f.KindsJournalledAfter(journalBefore);
    QCOMPARE(kinds.back(), OperationKind::EnableAddon);
    QVERIFY(std::ranges::find(kinds, OperationKind::InstallMoveIntoPlace) == kinds.end());
}

void ImportServiceTest::ALinkThatCannotBeRecreatedAfterTheLandingKeepsTheLanding()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    f.linkService.MakeLinkCreationFail();

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotCreateLink);
    QVERIFY(TheFolderLanded(results.front().result));
    QCOMPARE(results.front().landed, kInLibrary);
    QVERIFY(results.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
}

void ImportServiceTest::ALinkThatCannotComeDownHalfwayIsPutBackAndNothingElseMoves()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path staged = f.TheNewPackageIsStagedIn(kUtils);
    const std::size_t journalBefore = f.journal.appended.size();
    f.linkService.MakeTheRemovalFailFor(kLinkedElsewhere);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(), {AdmitRequest{.staged = staged, .category = kUtils, .replaces = kInLibrary}});

    QCOMPARE(results.front().result, FileResult::CouldNotRemoveTheLink);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(f.fileSystem.Exists(staged / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
    QCOMPARE(f.KindsJournalledAfter(journalBefore),
             (std::vector<OperationKind>{OperationKind::DisableAddon, OperationKind::DisableAddon,
                                         OperationKind::EnableAddon}));
}

void ImportServiceTest::AnAdmissionBatchStopsWhereTheCallerStopsIt()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.TheLibraryHoldsNodes({});

    std::vector<AdmitRequest> requests;
    for (const char* name : {"alpha", "beta", "gamma"})
    {
        const std::filesystem::path staged = InstallStagingIn(kUtils) / name;
        f.fileSystem.AddFile(staged / "manifest.json", kMegabyte);
        requests.push_back(AdmitRequest{.staged = staged, .category = kUtils});
    }

    std::vector<std::size_t> asked;
    const std::vector<AdmitResult> results = f.service.Admit(f.profile, f.Entries(), requests, {}, {},
                                                             [&asked](const std::size_t request)
                                                             {
                                                                 asked.push_back(request);

                                                                 return request < 1;
                                                             });

    QCOMPARE(results.size(), std::size_t{3});
    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::Cancelled);
    QCOMPARE(results[2].result, FileResult::Cancelled);
    QCOMPARE(asked, (std::vector<std::size_t>{0, 1}));
    QVERIFY(f.fileSystem.Exists(requests[1].staged / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(requests[2].staged / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
}

void ImportServiceTest::TheSimulatorStartingBetweenTwoAdmissionsRefusesTheRest()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.TheLibraryHoldsNodes({});

    std::vector<AdmitRequest> requests;
    for (const char* name : {"alpha", "beta", "gamma"})
    {
        const std::filesystem::path staged = InstallStagingIn(kUtils) / name;
        f.fileSystem.AddFile(staged / "manifest.json", kMegabyte);
        requests.push_back(AdmitRequest{.staged = staged, .category = kUtils});
    }

    const std::vector<AdmitResult> results = f.service.Admit(f.profile, f.Entries(), requests, {}, {},
                                                             [&f](const std::size_t request)
                                                             {
                                                                 if (request == 1)
                                                                 {
                                                                     f.processProbe.ReportTheSimulatorAsRunning();
                                                                 }

                                                                 return true;
                                                             });

    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(results[2].result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(requests[1].staged / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
}

void ImportServiceTest::TwoNewFoldersOfOneNameAdmitOnlyTheFirst()
{
    Fixture f;
    f.TheLibraryHoldsNodes({});
    const std::filesystem::path first = f.TheNewPackageIsStagedIn(kUtils, kMegabyte);
    const std::filesystem::path second = f.TheNewPackageIsStagedIn(kSceneries, 2 * kMegabyte);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{.staged = first, .category = kUtils}, AdmitRequest{.staged = second, .category = kSceneries}});

    QCOMPARE(results[0].result, FileResult::Completed);
    QCOMPARE(results[1].result, FileResult::TheIdentityIsTaken);
    QVERIFY(!results[1].replaced);
    QVERIFY(f.fileSystem.Exists(second / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kSceneries / "simbridge"));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
}

void ImportServiceTest::AFolderOfTheNameTheBatchJustReplacedIsRefusedByIdentity()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    const std::filesystem::path first = f.TheNewPackageIsStagedIn(kUtils, 2 * kMegabyte);
    const std::filesystem::path second = f.TheNewPackageIsStagedIn(kSceneries, 3 * kMegabyte);

    const std::vector<AdmitResult> results = f.service.Admit(
        f.profile, f.Entries(),
        {AdmitRequest{.staged = first, .category = kUtils, .replaces = kInLibrary},
         AdmitRequest{.staged = second, .category = kSceneries, .discardTheEarlierQuarantinedCopy = true}});

    QCOMPARE(results[0].result, FileResult::Completed);
    QVERIFY(results[0].replaced);
    QCOMPARE(results[1].result, FileResult::TheIdentityIsTaken);
    QVERIFY(f.fileSystem.Exists(second / "manifest.json"));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), 2 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
}

void ImportServiceTest::AnAdmissionBatchScansTheLibrariesOnce()
{
    Fixture f;
    f.fileSystem.AddDirectory(kLibrary);
    f.TheLibraryHoldsNodes({});

    std::vector<AdmitRequest> requests;
    for (const char* name : {"alpha", "beta", "gamma"})
    {
        const std::filesystem::path staged = InstallStagingIn(kUtils) / name;
        f.fileSystem.AddFile(staged / "manifest.json", kMegabyte);
        requests.push_back(AdmitRequest{.staged = staged, .category = kUtils});
    }

    const std::vector<AdmitResult> results = f.service.Admit(f.profile, f.Entries(), requests);

    QCOMPARE(results.size(), std::size_t{3});
    for (const AdmitResult& result : results)
    {
        QCOMPARE(result.result, FileResult::Completed);
    }
    QCOMPARE(f.catalog.scanned, f.profile.libraries.size());
}

void ImportServiceTest::AQuarantineThatRefusesTheOccupantKeepsTheLinksOfTheLibraryCopy()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.fileSystem.AddDirectory(kHeldInLibrary);
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);

    const CopyConflict conflict{.provenancePath = kInDestination, .libraryPath = kInLibrary};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::TheQuarantineIsOccupied);
    QVERIFY2(f.fileSystem.IsLink(kLinkedElsewhere), "a refused quarantine leaves the addon enabled");
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
}

void ImportServiceTest::AQuarantineThatRefusesTakingItBackKeepsTheLinksOfTheLibraryCopy()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();
    f.fileSystem.AddDirectory(kHeldInLibrary);
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::TheQuarantineIsOccupied);
    QVERIFY2(f.fileSystem.IsLink(kLinkedElsewhere), "a refused quarantine leaves the addon enabled");
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
}

void ImportServiceTest::AQuarantineThatRefusesTheSwapKeepsTheLinksOfTheOccupant()
{
    Fixture f;
    const std::filesystem::path itemPath = "E:/Sim/_fsorganizer-quarantine/simbridge";
    f.QuarantineHolds(itemPath, kInLibrary, kInLibrary);
    f.fileSystem.AddDirectory(kInLibrary);
    f.fileSystem.AddFile(kInLibrary / "manifest.json", 3 * kMegabyte);
    f.TheLibraryHolds({kInLibrary});
    f.fileSystem.AddDirectory(kDestination);
    f.fileSystem.AddLink(kInDestination, kInLibrary);
    f.fileSystem.AddDirectory(kHeldInLibrary);
    f.fileSystem.AddFile(kHeldInLibrary / "manifest.json", 4 * kMegabyte);

    const QuarantinedItem item{.path = itemPath, .origin = kInLibrary, .source = OriginSource::Sidecar};
    const SwapResult swapped = f.service.Swap(f.profile, f.Entries(), item);

    QCOMPARE(swapped.result, FileResult::TheQuarantineIsOccupied);
    QVERIFY2(f.fileSystem.IsLink(kInDestination), "a refused quarantine leaves the occupant enabled");
    QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
}

void ImportServiceTest::TakingBackWhatWasReplacedKeepsTheLinksInTheOtherDestinations()
{
    Fixture f;
    f.AddBothCopies();
    f.AddALinkToTheLibraryCopyInAnotherDestination();

    const CopyConflict conflict{
        .provenancePath = kInDestination, .libraryPath = kInLibrary, .ourLinkWasReplaced = true};
    const FileResult result =
        f.service.ResolveConflict(f.profile, f.Entries(), conflict, ConflictChoice::KeepTheProvenanceCopy);

    QCOMPARE(result, FileResult::Completed);
    QVERIFY(f.fileSystem.IsLink(kInDestination));
    QVERIFY2(f.fileSystem.IsLink(kLinkedElsewhere), "the link in the other destination comes back too");
    QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
}

QTEST_APPLESS_MAIN(ImportServiceTest)

#include "tst_import_service.moc"
