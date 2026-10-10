#include <QtTest/QtTest>

#include <string_view>

#include "application/ArchiveInstallService.h"
#include "domain/importing/ImportEngine.h"
#include "domain/importing/ImportPaths.h"
#include "domain/importing/OriginSidecar.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/ports/ManifestParser.h"
#include "tests/doubles/FakeArchiveReader.h"
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
    class ArchiveInstallServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFailureOfTheListingEndsTheInspection();
        static void EveryReasonToRefuseTheArchiveEndsTheInspection();
        static void TheVersionOfEachPackageIsReadFromItsManifest();
        static void AManifestThatDoesNotParseLeavesTheVersionEmptyAndThePackageInstallable();
        static void AManifestThatCannotBeReadLeavesTheVersionEmptyAndThePackageInstallable();
        static void APasswordFoundWhileReadingAManifestRefusesTheWholeArchive();
        static void TheManifestIsReadByItsListedNameWithACapOfOneMebibyte();
        static void TheAdmissionIsCheckedForEachPackageUnderTheCategory();
        static void TheSimulatorRunningShowsInEveryAdmission();
        static void TheRoomIsMeasuredOnTheCategoryForTheWholeArchive();
        static void WithoutACategoryTheAdmissionAndTheRoomAreSkipped();
        static void InspectingWritesNothingAndJournalsNothing();
        static void AskingTheAdmissionAgainGivesWhatInspectReportsForTheSameCategory();
        static void TheAdmissionCanBeAskedOfAnotherLibraryWithoutReadingTheArchiveAgain();
        static void TheQuarantinedVersionTravelsWithTheAdmission();
        static void OneScanAnswersEveryCategoryOfEveryLibrary();
        static void AFolderInTheWayOfOneCategoryDoesNotBlockTheOthers();
        static void AskingForNoCategoryOrNoPackageScansNothing();

        static void ANewPackageLandsAndTheStagingIsGone();
        static void TwoPackagesOfOneArchiveBothLandInTheOrderTheyWereChosen();
        static void APackageThatReplacesGoesToQuarantineAndKeepsTheLinks();
        static void TheJournalHasTheExtractRowLabelledWithTheArchiveBeforeThePlacingRows();
        static void TheStepsAreAnnouncedInOrder();
        static void AStaleSelectionStopsBeforeTouchingAnything();
        static void AnEmptySelectionDoesNothing();
        static void TheArchiveIsListedAgainBeforeInstalling();
        static void AnArchiveThatChangedIntoARefusalIsRefused();
        static void TheSimulatorRunningExtractsNothing();
        static void APackageRefusedByTheAdmissionDoesNotStopTheOthers();
        static void WhenEveryPackageIsRefusedNothingIsExtracted();
        static void AnOccupiedQuarantineRefusesThePackageUnlessTheEarlierCopyIsDiscarded();
        static void NotEnoughRoomStopsBeforeTheStaging();
        static void TheRoomForTheWholeArchiveIsNeededEvenWhenOnlyOnePackageIsChosen();
        static void AnUnmeasurableRoomStopsBeforeTheStaging();
        static void ALeftoverStagingIsRemovedAndRecordedBeforeExtracting();
        static void ALeftoverStagingThatCannotBeRemovedStopsTheInstall();
        static void AStagingThatCannotBeCreatedStopsTheInstall();
        static void CancellingDuringTheExtractionLeavesNoStagingAndNoPackage();
        static void AnExtractionThatFailsLeavesNoStagingAndNoPackage();
        static void AnArchiveThatNeedsAPasswordIsReportedAtTheExtraction();
        static void AFileOfTheWrongSizeFailsTheVerification();
        static void AFileThatWasNotExtractedFailsTheVerification();
        static void AStagingThatCannotBeWalkedFailsTheVerification();
        static void ThePackagesAreLeftInPlaceWhenTheCallerStopsBetweenTwo();
        static void TheCallerIsAskedByTheIndexItChoseEvenWhenAnEarlierPackageWasRefused();
        static void AFailedPlacementStillRemovesTheStaging();
        static void ADiskThatNoLongerHoldsTheOccupantTheOfferShowedLeavesThePackageOut();
        static void AnOccupantTheOfferDidNotShowLeavesThePackageOutAndTheOthersGoOn();
        static void TheReplacementTheOfferShowedStillHappens();
        static void TheExtractionIsOneRowAfterTheVerificationAndWithoutAnAddonName();
        static void AFailedVerificationIsTheResultOfTheOneExtractionRow();
        static void AStagingThatCannotBeRemovedIsReportedAndRecordedAfterAnInstall();
        static void AStagingThatCannotBeRemovedIsReportedAfterAFailedVerification();
        static void AStagingThatWasRemovedIsNotReported();
        static void AnArchiveThatTimedOutWhileExtractingIsTookTooLongAndLeavesNoStaging();
        static void TheSameFolderWrittenInTwoCasesIsInstalled();
        static void AFileMissingFromAFolderWrittenInTwoCasesStillFailsTheVerification();
        static void AnArchiveInsideTheStagingIsRefusedBeforeAnyWrite();
        static void AnArchiveInsideTheStagingFolderItselfIsRefusedToo();
        static void AnArchiveInsideWhatWouldBeReplacedRefusesOnlyThatPackage();
        static void AnArchiveBesideTheOccupantIsNotInsideIt();
        static void AReplacementWithTheRootOfTheLibraryAsCategoryLandsAtTheOccupantAndLeavesNoStaging();
        static void AReplacementWithoutAnyCategoryIsRefusedAsNotInALibraryAndNothingIsTouched();
        static void ASourceIsAFolderOnlyWhenItIsAPhysicalFolder();
        static void InspectingSaysWhetherTheSourceIsAFolderWhateverTheOutcome();
        static void ALinkToAFolderThatReachesTheReadingIsReadAsAFolderSource();
        static void TwoOriginsWithDifferentContentAreInspectedEachByItsOwn();
        static void AFolderThatIsTheStagingIsRefusedBeforeAnyWrite();
        static void AFolderThatContainsTheCategoryIsRefusedBeforeAnyWrite();
        static void AFolderThatContainsTheLibraryIsRefusedBeforeAnyWrite();
        static void AFolderThatIsTheOccupantRefusesOnlyThatPackage();
        static void AFolderThatContainsTheOccupantRefusesOnlyThatPackage();
        static void AFolderBesideTheOccupantIsNotNestedWithIt();
        static void ASourceInsideTheEarlierQuarantinedCopyIsRefusedWhenThatCopyIsToBeDiscarded();
        static void ASourceThatContainsTheEarlierQuarantinedCopyIsRefusedWhenThatCopyIsToBeDiscarded();
        static void ASourceBesideTheEarlierQuarantinedCopyIsInstalledWhenThatCopyIsDiscarded();
        static void ASourceInsideTheEarlierQuarantinedCopyWaitsForTheOccupiedQuarantineWhenNothingIsDiscarded();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;
    constexpr std::uintmax_t kManifestBytes = 100;
    constexpr std::uintmax_t kSceneryBytes = 400;
    constexpr std::uintmax_t kBytesOfOnePackage = kManifestBytes + kSceneryBytes;
    constexpr std::size_t kFilesOfOnePackage = 2;
    constexpr std::string_view kArchiveBytes = "PK-the-archive-as-it-was-downloaded";

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kOtherDestination = "E:/Sim/Community2024";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kUtils = "D:/Library/Utils";
    const std::filesystem::path kInDestination = "E:/Sim/Community/simbridge";
    const std::filesystem::path kLinkedElsewhere = "E:/Sim/Community2024/simbridge";
    const std::filesystem::path kInLibrary = "D:/Library/Utils/simbridge";
    const std::filesystem::path kHeldInLibrary = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path kArchive = "D:/Downloads/pack.zip";
    const std::filesystem::path kOtherCategory = "D:/Library/Other";

    class ScriptedManifestParser final : public ManifestParser
    {
    public:
        [[nodiscard]] std::optional<Manifest> Parse(const std::string_view content) const override
        {
            constexpr std::string_view prefix = "ok:";

            if (!content.starts_with(prefix))
            {
                return std::nullopt;
            }

            return Manifest{.packageVersion = std::string{content.substr(prefix.size())}};
        }
    };

    struct Packaged
    {
        std::string folder{};
        std::string version{};
    };

    ArchiveEntry File(std::string path, const std::uintmax_t bytes = 1)
    {
        return ArchiveEntry{.path = std::move(path), .bytes = bytes, .kind = ArchiveEntryKind::File};
    }

    std::vector<ArchiveEntry> ListingOf(const std::vector<Packaged>& packages)
    {
        std::vector<ArchiveEntry> entries;

        for (const Packaged& package : packages)
        {
            entries.push_back(File(package.folder + "/manifest.json", kManifestBytes));
            entries.push_back(File(package.folder + "/scenery/a.bgl", kSceneryBytes));
        }

        return entries;
    }

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
        ImportService importing{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                                linking, log,          LinkType::Junction};
        FakeArchiveReader archives{fileSystem};
        ScriptedManifestParser manifests;
        ArchiveInstallService service{archives, manifests, filesystemProbe, files, log, importing};

        SimulatorProfile profile{.destinations = {kDestination},
                                 .defaultDestination = kDestination,
                                 .libraries = {Library{.id = "lib-1", .path = kLibrary}}};

        Fixture()
        {
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kUtils);
            fileSystem.AddFileWithContents(kArchive, std::string{kArchiveBytes});
            TheLibraryHoldsNodes({});
        }

        [[nodiscard]] std::vector<DestinationEntry> Entries() const
        {
            return classifier.Resolve(profile.destinations, {kLibrary});
        }

        void TheLibraryHoldsNodes(const std::vector<TreeNode>& addons)
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children = addons;

            catalog.SetTree(kLibrary, std::move(library));
        }

        void TheArchiveHolds(const std::vector<Packaged>& packages)
        {
            for (const Packaged& package : packages)
            {
                archives.Holds(package.folder + "/manifest.json", "ok:" + package.version);
            }

            archives.Lists(ListingOf(packages));
        }

        void TheOriginHolds(const std::filesystem::path& origin, const std::vector<Packaged>& packages)
        {
            for (const Packaged& package : packages)
            {
                archives.HoldsFor(origin, package.folder + "/manifest.json", "ok:" + package.version);
            }

            archives.ListsFor(origin, ListingOf(packages));
        }

        void ASimbridgeIsInstalledWithTwoLinks(const std::string& version = "1.0.0")
        {
            fileSystem.AddDirectory(kInLibrary);
            fileSystem.AddFile(kInLibrary / "manifest.json", kMegabyte);
            TheLibraryHoldsNodes({AddonNodeDeclaring(kInLibrary, version)});

            fileSystem.AddDirectory(kDestination);
            fileSystem.AddLink(kInDestination, kInLibrary);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddLink(kLinkedElsewhere, kInLibrary);
            profile.destinations.push_back(kOtherDestination);
        }

        void TheQuarantineHoldsAnEarlierSimbridge()
        {
            fileSystem.AddDirectory(kHeldInLibrary.parent_path());
            fileSystem.AddDirectory(kHeldInLibrary);
            fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);

            log.RecordImport(OperationKind::QuarantineFromLibrary, AddonId{.libraryId = "lib-1"}, kInLibrary,
                             kHeldInLibrary, FileResult::Completed);

            QVERIFY(
                sidecars.Write(SidecarPathFor(kHeldInLibrary),
                               TextOfTheOrigin(QuarantineOrigin{.origin = kInLibrary, .quarantinedAt = clock.now})));
        }

        void SomethingOtherThanAnAddonIsInTheWay(const std::string& folder)
        {
            fileSystem.AddFile(kUtils / folder / "readme.txt", kMegabyte);
        }

        [[nodiscard]] ArchiveInstallRequest Choosing(const std::vector<std::string>& roots,
                                                     const bool discard = false) const
        {
            ArchiveInstallRequest request{.archive = kArchive, .category = kUtils};

            for (const std::string& root : roots)
            {
                request.packages.push_back(ChosenPackage{.root = root, .discardTheEarlierQuarantinedCopy = discard});
            }

            return request;
        }

        [[nodiscard]] ArchiveInstallRequest ChoosingToReplaceTheSimbridge(const std::vector<std::string>& roots,
                                                                          const bool discard = false) const
        {
            ArchiveInstallRequest request = Choosing(roots, discard);

            for (ChosenPackage& chosen : request.packages)
            {
                if (chosen.root == "simbridge")
                {
                    chosen.replaces = kInLibrary;
                }
            }

            return request;
        }

        [[nodiscard]] std::vector<OperationKind> KindsJournalled() const
        {
            std::vector<OperationKind> kinds;
            kinds.reserve(journal.appended.size());

            for (const OperationRecord& record : journal.appended)
            {
                kinds.push_back(record.kind);
            }

            return kinds;
        }
    };

    void ExpectTheArchiveIsUntouched(const Fixture& f)
    {
        QVERIFY2(f.fileSystem.IsFile(kArchive), "the archive is still a file where it was");
        QCOMPARE(f.fileSystem.FileSize(kArchive), std::uintmax_t{kArchiveBytes.size()});
        QCOMPARE(f.fileSystem.ContentsOf(kArchive).value_or(std::string{}), std::string{kArchiveBytes});
    }

    void ExpectNothingWasExtractedOrJournalled(const Fixture& f)
    {
        QVERIFY(f.archives.extractions.empty());
        QVERIFY(f.journal.appended.empty());
        QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    }

    void ExpectTheTwoLinksStandAtTheOccupant(const Fixture& f)
    {
        QVERIFY(f.fileSystem.IsLink(kInDestination));
        QCOMPARE(f.fileSystem.LinkTarget(kInDestination).value(), kInLibrary);
        QVERIFY(f.fileSystem.IsLink(kLinkedElsewhere));
        QCOMPARE(f.fileSystem.LinkTarget(kLinkedElsewhere).value(), kInLibrary);
    }

    void ExpectAlphaLanded(const Fixture& f, const PackageInstalled& installed)
    {
        QCOMPARE(installed.root, std::string{"alpha"});
        QCOMPARE(installed.result, FileResult::Completed);
        QCOMPARE(installed.landed, kUtils / "alpha");
        QCOMPARE(f.fileSystem.FileSize(kUtils / "alpha" / "manifest.json"), kManifestBytes);
        QCOMPARE(f.fileSystem.FileSize(kUtils / "alpha" / "scenery" / "a.bgl"), kSceneryBytes);
    }
}

void ArchiveInstallServiceTest::EveryFailureOfTheListingEndsTheInspection()
{
    for (const ArchiveFailure failure :
         {ArchiveFailure::TheToolIsMissing, ArchiveFailure::CouldNotOpen, ArchiveFailure::NeedsAPassword,
          ArchiveFailure::CouldNotExtract, ArchiveFailure::TookTooLong, ArchiveFailure::Cancelled})
    {
        Fixture f;
        f.TheArchiveHolds({{"alpha", "1.0.0"}});
        f.archives.FailsToList(failure);

        const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

        QCOMPARE(found.failure, failure);
        QCOMPARE(found.refusal, NothingToInstall::None);
        QVERIFY(found.packages.empty());
        QCOMPARE(f.archives.read.size(), std::size_t{0});
    }
}

void ArchiveInstallServiceTest::EveryReasonToRefuseTheArchiveEndsTheInspection()
{
    const std::vector<std::pair<std::vector<ArchiveEntry>, NothingToInstall>> cases{
        {{File("readme.txt")}, NothingToInstall::NoPackage},
        {{File("Setup/Install.exe")}, NothingToInstall::OnlyAnInstaller},
        {{File("pkg/manifest.json"), File("../escape.txt")}, NothingToInstall::AnEntryIsNotSafe},
        {{File("manifest.json")}, NothingToInstall::ThePackageHasNoFolder},
        {{File("a/pack/manifest.json"), File("b/pack/manifest.json")}, NothingToInstall::TwoPackagesShareAName},
    };

    for (const auto& [entries, refusal] : cases)
    {
        Fixture f;
        f.archives.Lists(entries);

        const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

        QCOMPARE(found.refusal, refusal);
        QCOMPARE(found.failure, ArchiveFailure::None);
        QVERIFY(found.packages.empty());
        QCOMPARE(f.archives.read.size(), std::size_t{0});
        QCOMPARE(f.catalog.scanned, std::size_t{0});
    }
}

void ArchiveInstallServiceTest::TheVersionOfEachPackageIsReadFromItsManifest()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.2.3"}, {"beta", "4.5.6"}});

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.refusal, NothingToInstall::None);
    QCOMPARE(found.packages.size(), std::size_t{2});
    QCOMPARE(found.packages[0].versionInTheArchive, std::string{"1.2.3"});
    QCOMPARE(EntryPathOf(found.packages[0].package), std::string{"alpha"});
    QCOMPARE(found.packages[1].versionInTheArchive, std::string{"4.5.6"});
    QCOMPARE(EntryPathOf(found.packages[1].package), std::string{"beta"});
    QCOMPARE(found.bytes, 2 * (kManifestBytes + kSceneryBytes));
}

void ArchiveInstallServiceTest::AManifestThatDoesNotParseLeavesTheVersionEmptyAndThePackageInstallable()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});
    f.archives.Holds("alpha/manifest.json", "{ not what the parser takes");

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.packages.size(), std::size_t{2});
    QCOMPARE(found.packages[0].versionInTheArchive, std::string{});
    QCOMPARE(found.packages[0].admission.result, FileResult::Completed);
    QCOMPARE(found.packages[1].versionInTheArchive, std::string{"2.0.0"});
}

void ArchiveInstallServiceTest::AManifestThatCannotBeReadLeavesTheVersionEmptyAndThePackageInstallable()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.FailsToRead("alpha/manifest.json", ArchiveFailure::CouldNotExtract);

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{});
    QCOMPARE(found.packages.front().admission.result, FileResult::Completed);
}

void ArchiveInstallServiceTest::APasswordFoundWhileReadingAManifestRefusesTheWholeArchive()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});
    f.archives.FailsToRead("beta/manifest.json", ArchiveFailure::NeedsAPassword);

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.failure, ArchiveFailure::NeedsAPassword);
    QVERIFY(found.packages.empty());
    QCOMPARE(found.refusal, NothingToInstall::None);
    QCOMPARE(f.catalog.scanned, std::size_t{0});
}

void ArchiveInstallServiceTest::TheManifestIsReadByItsListedNameWithACapOfOneMebibyte()
{
    Fixture f;
    f.archives.Lists({File("Pacote/Manifest.JSON"), File("Pacote/layout.json")});
    f.archives.Holds("Pacote/Manifest.JSON", "ok:7.0");

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{"7.0"});
    QCOMPARE(f.archives.read.size(), std::size_t{1});
    QCOMPARE(f.archives.read.front().entry, std::string{"Pacote/Manifest.JSON"});
    QCOMPARE(f.archives.read.front().most, std::size_t{1024 * 1024});
}

void ArchiveInstallServiceTest::TheAdmissionIsCheckedForEachPackageUnderTheCategory()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.fileSystem.AddDirectory(kUtils / "quarantined");
    f.fileSystem.AddFile(kUtils / "quarantined" / "manifest.json", kMegabyte);
    f.fileSystem.AddDirectory("D:/Library/_fsorganizer-quarantine/quarantined");
    f.TheLibraryHoldsNodes(
        {AddonNodeDeclaring(kInLibrary, "1.0.0"), AddonNodeDeclaring(kUtils / "quarantined", "3.0.0")});
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}, {"quarantined", "3.1.0"}, {"blocked", "1.0.0"}});

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.packages.size(), std::size_t{4});

    const AdmissionCheck& replaces = found.packages[0].admission;
    QCOMPARE(replaces.result, FileResult::Completed);
    QCOMPARE(replaces.target, kInLibrary);
    QCOMPARE(replaces.occupant, kInLibrary);
    QCOMPARE(replaces.occupantVersion, std::string{"1.0.0"});
    QVERIFY(!replaces.theQuarantineHoldsAnEarlierCopy);
    QCOMPARE(found.packages[0].versionInTheArchive, std::string{"2.0.0"});

    const AdmissionCheck& isNew = found.packages[1].admission;
    QCOMPARE(isNew.result, FileResult::Completed);
    QCOMPARE(isNew.target, kUtils / "fresh");
    QVERIFY(isNew.occupant.empty());

    const AdmissionCheck& held = found.packages[2].admission;
    QCOMPARE(held.result, FileResult::Completed);
    QCOMPARE(held.occupantVersion, std::string{"3.0.0"});
    QVERIFY(held.theQuarantineHoldsAnEarlierCopy);
    QVERIFY(held.quarantinedVersion.empty());

    const AdmissionCheck& inTheWay = found.packages[3].admission;
    QCOMPARE(inTheWay.result, FileResult::TheOriginIsOccupied);
    QCOMPARE(inTheWay.occupant, kUtils / "blocked");

    const std::size_t oneScanPerLibraryAndOnePerQuarantinedCopy = f.profile.libraries.size() + 1;

    QCOMPARE(f.catalog.scanned, oneScanPerLibraryAndOnePerQuarantinedCopy);
}

void ArchiveInstallServiceTest::TheSimulatorRunningShowsInEveryAdmission()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});
    f.processProbe.ReportTheSimulatorAsRunning();

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.packages.size(), std::size_t{2});
    QCOMPARE(found.packages[0].admission.result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(found.packages[1].admission.result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(found.room, FileResult::Completed);
}

void ArchiveInstallServiceTest::TheRoomIsMeasuredOnTheCategoryForTheWholeArchive()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});

    QCOMPARE(f.service.Inspect(f.profile, kArchive, kUtils).room, FileResult::Completed);

    f.fileSystem.SetFreeSpace(kUtils, 1);
    QCOMPARE(f.service.Inspect(f.profile, kArchive, kUtils).room, FileResult::NotEnoughFreeSpace);

    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "1.0.0"}});

    f.fileSystem.SetFreeSpace(kUtils, FreeSpaceNeededFor(kBytesOfOnePackage, kFilesOfOnePackage));
    QCOMPARE(f.service.Inspect(f.profile, kArchive, kUtils).room, FileResult::NotEnoughFreeSpace);

    f.fileSystem.SetFreeSpace(kUtils, FreeSpaceNeededFor(2 * kBytesOfOnePackage, 2 * kFilesOfOnePackage));
    QCOMPARE(f.service.Inspect(f.profile, kArchive, kUtils).room, FileResult::Completed);

    f.fileSystem.MarkFreeSpaceUnknown(kUtils);
    QCOMPARE(f.service.Inspect(f.profile, kArchive, kUtils).room, FileResult::CouldNotCheckFreeSpace);
}

void ArchiveInstallServiceTest::WithoutACategoryTheAdmissionAndTheRoomAreSkipped()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.fileSystem.SetFreeSpace(kUtils, 1);

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, {});

    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{"1.0.0"});
    QCOMPARE(found.packages.front().admission.result, FileResult::TheOutcomeIsUnknown);
    QCOMPARE(found.room, FileResult::Completed);
    QCOMPARE(f.catalog.scanned, std::size_t{0});
}

void ArchiveInstallServiceTest::InspectingWritesNothingAndJournalsNothing()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}});

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QCOMPARE(found.packages.size(), std::size_t{2});
    QVERIFY(f.journal.appended.empty());
    QVERIFY(f.archives.extractions.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "fresh"));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AskingTheAdmissionAgainGivesWhatInspectReportsForTheSameCategory()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}, {"blocked", "1.0.0"}});

    const WhatIsInTheArchive inspected = f.service.Inspect(f.profile, kArchive, kUtils);
    const WhatIsInTheArchive withoutCategory = f.service.Inspect(f.profile, kArchive, {});
    const std::vector<AdmissionCheck> asked =
        f.service.AdmissionIn(f.profile, withoutCategory.packages, {kUtils}).front();

    QCOMPARE(asked.size(), inspected.packages.size());
    for (std::size_t index = 0; index < asked.size(); ++index)
    {
        const AdmissionCheck& expected = inspected.packages[index].admission;

        QCOMPARE(asked[index].result, expected.result);
        QCOMPARE(asked[index].target, expected.target);
        QCOMPARE(asked[index].occupant, expected.occupant);
        QCOMPARE(asked[index].occupantVersion, expected.occupantVersion);
        QCOMPARE(asked[index].theQuarantineHoldsAnEarlierCopy, expected.theQuarantineHoldsAnEarlierCopy);
    }
    QCOMPARE(asked[0].occupantVersion, std::string{"1.0.0"});
    QCOMPARE(asked[2].result, FileResult::TheOriginIsOccupied);
}

void ArchiveInstallServiceTest::TheAdmissionCanBeAskedOfAnotherLibraryWithoutReadingTheArchiveAgain()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.profile.libraries.push_back(Library{.id = "lib-2", .path = "F:/Spare"});
    f.fileSystem.AddDirectory("F:/Spare");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, {});
    const std::size_t listed = f.archives.listed.size();
    const std::size_t read = f.archives.read.size();

    const std::vector<AdmissionCheck> inTheFirst = f.service.AdmissionIn(f.profile, found.packages, {kLibrary}).front();
    const std::vector<AdmissionCheck> inTheSecond =
        f.service.AdmissionIn(f.profile, found.packages, {"F:/Spare"}).front();

    QCOMPARE(inTheFirst.front().result, FileResult::Completed);
    QCOMPARE(inTheFirst.front().occupant, kInLibrary);
    QCOMPARE(inTheSecond.front().result, FileResult::Completed);
    QCOMPARE(inTheSecond.front().target, std::filesystem::path{"F:/Spare/simbridge"});
    QVERIFY(inTheSecond.front().occupant.empty());
    QCOMPARE(f.archives.listed.size(), listed);
    QCOMPARE(f.archives.read.size(), read);
}

void ArchiveInstallServiceTest::TheQuarantinedVersionTravelsWithTheAdmission()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.catalog.SetTree(kHeldInLibrary, AddonNodeDeclaring(kHeldInLibrary, "0.8.0"));
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});

    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, kUtils);

    QVERIFY(found.packages.front().admission.theQuarantineHoldsAnEarlierCopy);
    QCOMPARE(found.packages.front().admission.quarantinedVersion, std::string{"0.8.0"});
    QCOMPARE(found.packages.front().admission.occupantVersion, std::string{"1.0.0"});
}

void ArchiveInstallServiceTest::OneScanAnswersEveryCategoryOfEveryLibrary()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.profile.libraries.push_back(Library{.id = "lib-2", .path = "F:/Spare"});
    f.fileSystem.AddDirectory("F:/Spare");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}});
    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, {});
    const std::vector<std::filesystem::path> categories{kLibrary, kUtils, "D:/Library/Sceneries", "F:/Spare",
                                                        "F:/Spare/Aircraft"};
    const std::size_t scannedBefore = f.catalog.scanned;

    const std::vector<std::vector<AdmissionCheck>> asked = f.service.AdmissionIn(f.profile, found.packages, categories);

    QCOMPARE(f.catalog.scanned - scannedBefore, f.profile.libraries.size());
    QCOMPARE(asked.size(), categories.size());
    for (std::size_t category = 0; category < categories.size(); ++category)
    {
        QCOMPARE(asked[category].size(), std::size_t{2});
        QCOMPARE(asked[category][1].target, categories[category] / "fresh");
        QVERIFY(asked[category][1].occupant.empty());
    }
    for (const std::size_t inTheFirstLibrary : {std::size_t{0}, std::size_t{1}, std::size_t{2}})
    {
        QCOMPARE(asked[inTheFirstLibrary][0].occupant, kInLibrary);
    }
    for (const std::size_t inTheSecondLibrary : {std::size_t{3}, std::size_t{4}})
    {
        QVERIFY(asked[inTheSecondLibrary][0].occupant.empty());
        QCOMPARE(asked[inTheSecondLibrary][0].target, categories[inTheSecondLibrary] / "simbridge");
    }
}

void ArchiveInstallServiceTest::AFolderInTheWayOfOneCategoryDoesNotBlockTheOthers()
{
    Fixture f;
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"blocked", "1.0.0"}});
    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, {});

    const std::vector<std::vector<AdmissionCheck>> asked =
        f.service.AdmissionIn(f.profile, found.packages, {kLibrary, kUtils});

    QCOMPARE(asked.size(), std::size_t{2});
    QCOMPARE(asked[0].front().result, FileResult::Completed);
    QCOMPARE(asked[1].front().result, FileResult::TheOriginIsOccupied);
}

void ArchiveInstallServiceTest::AskingForNoCategoryOrNoPackageScansNothing()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    const WhatIsInTheArchive found = f.service.Inspect(f.profile, kArchive, {});

    QVERIFY(f.service.AdmissionIn(f.profile, found.packages, {}).empty());

    const std::vector<std::vector<AdmissionCheck>> noPackage = f.service.AdmissionIn(f.profile, {}, {kUtils, kLibrary});

    QCOMPARE(noPackage.size(), std::size_t{2});
    QVERIFY(noPackage.front().empty());
    QVERIFY(noPackage.back().empty());
    QCOMPARE(f.catalog.scanned, std::size_t{0});
}

void ArchiveInstallServiceTest::ANewPackageLandsAndTheStagingIsGone()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.failure, ArchiveFailure::None);
    QCOMPARE(did.refusal, NothingToInstall::None);
    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    ExpectAlphaLanded(f, did.packages.front());
    QVERIFY(!did.packages.front().replaced);
    QVERIFY(!did.theStagingStayed);
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QCOMPARE(f.archives.extractions.size(), std::size_t{1});
    QCOMPARE(f.archives.extractions.front().archive, kArchive);
    QCOMPARE(f.archives.extractions.front().into, InstallStagingIn(kUtils));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TwoPackagesOfOneArchiveBothLandInTheOrderTheyWereChosen()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"beta", "alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].root, std::string{"beta"});
    QCOMPARE(did.packages[0].result, FileResult::Completed);
    QCOMPARE(did.packages[0].landed, kUtils / "beta");
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(f.archives.extractions.size(), std::size_t{1});
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::APackageThatReplacesGoesToQuarantineAndKeepsTheLinks()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});

    const WhatInstallingDid did =
        f.service.Install(f.profile, f.Entries(), f.ChoosingToReplaceTheSimbridge({"simbridge"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QCOMPARE(did.packages.front().landed, kInLibrary);
    QVERIFY(did.packages.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kManifestBytes);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QCOMPARE(f.KindsJournalled(),
             (std::vector<OperationKind>{OperationKind::InstallExtractToStaging, OperationKind::DisableAddon,
                                         OperationKind::DisableAddon, OperationKind::QuarantineFromLibrary,
                                         OperationKind::InstallMoveIntoPlace, OperationKind::EnableAddon,
                                         OperationKind::EnableAddon}));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheJournalHasTheExtractRowLabelledWithTheArchiveBeforeThePlacingRows()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha", "beta"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(f.KindsJournalled(),
             (std::vector<OperationKind>{OperationKind::InstallExtractToStaging, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::InstallMoveIntoPlace}));

    const OperationRecord& extract = f.journal.appended.front();
    QCOMPARE(extract.source, kArchive);
    QCOMPARE(extract.target, InstallStagingIn(kUtils));
    QCOMPARE(extract.label, std::string{"pack.zip"});
    QCOMPARE(std::get<FileResult>(extract.outcome), FileResult::Completed);
    QCOMPARE(f.journal.appended[1].target, kUtils / "alpha");
    QCOMPARE(f.journal.appended[2].target, kUtils / "beta");
}

void ArchiveInstallServiceTest::TheStepsAreAnnouncedInOrder()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    std::vector<OperationKind> steps;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}), {},
                                                    [&steps](const OperationKind kind)
                                                    {
                                                        steps.push_back(kind);
                                                    });

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(steps,
             (std::vector<OperationKind>{OperationKind::InstallExtractToStaging, OperationKind::ImportVerifyStaging,
                                         OperationKind::InstallMoveIntoPlace}));
}

void ArchiveInstallServiceTest::AStaleSelectionStopsBeforeTouchingAnything()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha", "ghost"}));

    QCOMPARE(did.stopped, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AnEmptySelectionDoesNothing()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.failure, ArchiveFailure::None);
    QCOMPARE(did.refusal, NothingToInstall::None);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheArchiveIsListedAgainBeforeInstalling()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.FailsToList(ArchiveFailure::CouldNotOpen);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(did.stopped, FileResult::Completed);
    QVERIFY(did.packages.empty());
    QCOMPARE(f.archives.listed.size(), std::size_t{1});
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AnArchiveThatChangedIntoARefusalIsRefused()
{
    Fixture f;
    f.archives.Lists({File("readme.txt")});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.refusal, NothingToInstall::NoPackage);
    QCOMPARE(did.failure, ArchiveFailure::None);
    QCOMPARE(did.stopped, FileResult::Completed);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheSimulatorRunningExtractsNothing()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.processProbe.ReportTheSimulatorAsRunning();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::TheSimulatorIsRunning);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::APackageRefusedByTheAdmissionDoesNotStopTheOthers()
{
    Fixture f;
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"blocked", "1.0.0"}, {"alpha", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"blocked", "alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].root, std::string{"blocked"});
    QCOMPARE(did.packages[0].result, FileResult::TheOriginIsOccupied);
    QVERIFY(did.packages[0].landed.empty());
    ExpectAlphaLanded(f, did.packages[1]);
    QVERIFY(f.fileSystem.Exists(kUtils / "blocked" / "readme.txt"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "blocked" / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::WhenEveryPackageIsRefusedNothingIsExtracted()
{
    Fixture f;
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"blocked", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"blocked"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheOriginIsOccupied);
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AnOccupiedQuarantineRefusesThePackageUnlessTheEarlierCopyIsDiscarded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks();
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    const std::size_t journalBefore = f.journal.appended.size();

    const WhatInstallingDid refused =
        f.service.Install(f.profile, f.Entries(), f.ChoosingToReplaceTheSimbridge({"simbridge"}));

    QCOMPARE(refused.stopped, FileResult::Completed);
    QCOMPARE(refused.packages.size(), std::size_t{1});
    QCOMPARE(refused.packages.front().result, FileResult::TheQuarantineIsOccupied);
    QVERIFY(!refused.packages.front().replaced);
    QVERIFY(f.archives.extractions.empty());
    QCOMPARE(f.journal.appended.size(), journalBefore);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);

    const WhatInstallingDid discarding =
        f.service.Install(f.profile, f.Entries(), f.ChoosingToReplaceTheSimbridge({"simbridge"}, true));

    QCOMPARE(discarding.packages.size(), std::size_t{1});
    QCOMPARE(discarding.packages.front().result, FileResult::Completed);
    QVERIFY(discarding.packages.front().replaced);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kManifestBytes);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::NotEnoughRoomStopsBeforeTheStaging()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.fileSystem.SetFreeSpace(kUtils, 1);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::NotEnoughFreeSpace);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheRoomForTheWholeArchiveIsNeededEvenWhenOnlyOnePackageIsChosen()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    f.fileSystem.SetFreeSpace(kUtils, FreeSpaceNeededFor(kBytesOfOnePackage, kFilesOfOnePackage));

    const WhatInstallingDid refused = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(refused.stopped, FileResult::NotEnoughFreeSpace);
    QVERIFY(refused.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);

    f.fileSystem.SetFreeSpace(kUtils, FreeSpaceNeededFor(2 * kBytesOfOnePackage, 2 * kFilesOfOnePackage));

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    ExpectAlphaLanded(f, did.packages.front());
}

void ArchiveInstallServiceTest::AnUnmeasurableRoomStopsBeforeTheStaging()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.fileSystem.MarkFreeSpaceUnknown(kUtils);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::CouldNotCheckFreeSpace);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
}

void ArchiveInstallServiceTest::ALeftoverStagingIsRemovedAndRecordedBeforeExtracting()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    const std::filesystem::path staging = InstallStagingIn(kUtils);
    f.fileSystem.AddFile(staging / "half-of-something.bin", kMegabyte);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    ExpectAlphaLanded(f, did.packages.front());
    QVERIFY(!f.fileSystem.Exists(staging));
    QCOMPARE(f.KindsJournalled(),
             (std::vector<OperationKind>{OperationKind::DiscardStaging, OperationKind::InstallExtractToStaging,
                                         OperationKind::InstallMoveIntoPlace}));
    QCOMPARE(f.journal.appended.front().source, staging);
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::Completed);
    QVERIFY2(f.journal.appended.front().addonId.folderName.empty(), "the staging folder is no addon");
    QCOMPARE(f.journal.appended.front().addonId.libraryId, std::string{"lib-1"});
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::ALeftoverStagingThatCannotBeRemovedStopsTheInstall()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    const std::filesystem::path staging = InstallStagingIn(kUtils);
    f.fileSystem.AddFile(staging / "half-of-something.bin", kMegabyte);
    f.files.MakeTheRemovalFailFor(staging);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::CouldNotDiscard);
    QVERIFY(did.packages.empty());
    QVERIFY(f.archives.extractions.empty());
    QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::DiscardStaging}));
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::CouldNotDiscard);
    QVERIFY(f.fileSystem.Exists(staging / "half-of-something.bin"));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AStagingThatCannotBeCreatedStopsTheInstall()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.files.MakeTheCreationFail();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::CouldNotExtract);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::CancellingDuringTheExtractionLeavesNoStagingAndNoPackage()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});
    std::size_t asked = 0;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha", "beta"}),
                                                    [&asked](const CopyProgress&)
                                                    {
                                                        return ++asked < 3;
                                                    });

    QCOMPARE(did.stopped, FileResult::Cancelled);
    QVERIFY(did.packages.empty());
    QCOMPARE(asked, std::size_t{3});
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
    QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::InstallExtractToStaging}));
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::Cancelled);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AnExtractionThatFailsLeavesNoStagingAndNoPackage()
{
    for (const ArchiveFailure failure :
         {ArchiveFailure::TheToolIsMissing, ArchiveFailure::CouldNotOpen, ArchiveFailure::CouldNotExtract})
    {
        Fixture f;
        f.TheArchiveHolds({{"alpha", "1.0.0"}});
        f.archives.FailsAfterWritingEverything(failure);

        const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

        QCOMPARE(did.stopped, FileResult::CouldNotExtract);
        QVERIFY(did.packages.empty());
        QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
        QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
        QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::InstallExtractToStaging}));
        QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::CouldNotExtract);
        ExpectTheArchiveIsUntouched(f);
    }
}

void ArchiveInstallServiceTest::AnArchiveThatNeedsAPasswordIsReportedAtTheExtraction()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.FailsToExtract(ArchiveFailure::NeedsAPassword);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::TheArchiveNeedsAPassword);
    QVERIFY(did.packages.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::TheArchiveNeedsAPassword);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AFileOfTheWrongSizeFailsTheVerification()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.WritesTheWrongSizeFor("alpha/scenery/a.bgl");

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::InstallExtractToStaging}));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::AFileThatWasNotExtractedFailsTheVerification()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.LeavesOut("alpha/scenery/a.bgl");

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

void ArchiveInstallServiceTest::AStagingThatCannotBeWalkedFailsTheVerification()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.filesystemProbe.RefuseToWalk(InstallStagingIn(kUtils));

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

void ArchiveInstallServiceTest::ThePackagesAreLeftInPlaceWhenTheCallerStopsBetweenTwo()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}, {"beta", "2.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha", "beta"}), {}, {},
                                                    [](const std::size_t package)
                                                    {
                                                        return package < 1;
                                                    });

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    ExpectAlphaLanded(f, did.packages[0]);
    QCOMPARE(did.packages[1].root, std::string{"beta"});
    QCOMPARE(did.packages[1].result, FileResult::Cancelled);
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheCallerIsAskedByTheIndexItChoseEvenWhenAnEarlierPackageWasRefused()
{
    Fixture f;
    f.SomethingOtherThanAnAddonIsInTheWay("blocked");
    f.TheArchiveHolds({{"blocked", "1.0.0"}, {"alpha", "1.0.0"}, {"beta", "2.0.0"}});
    std::vector<std::size_t> asked;

    const WhatInstallingDid did =
        f.service.Install(f.profile, f.Entries(), f.Choosing({"blocked", "alpha", "beta"}), {}, {},
                          [&asked](const std::size_t package)
                          {
                              asked.push_back(package);

                              return package != 2;
                          });

    QCOMPARE(did.packages.size(), std::size_t{3});
    QCOMPARE(did.packages[0].result, FileResult::TheOriginIsOccupied);
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(did.packages[2].result, FileResult::Cancelled);
    QCOMPARE(asked, (std::vector<std::size_t>{1, 2}));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
}

void ArchiveInstallServiceTest::AFailedPlacementStillRemovesTheStaging()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.files.MakeTheMoveFail();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::CouldNotMoveIntoPlace);
    QVERIFY(did.packages.front().landed.empty());
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::ADiskThatNoLongerHoldsTheOccupantTheOfferShowedLeavesThePackageOut()
{
    Fixture f;
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});

    const WhatInstallingDid did =
        f.service.Install(f.profile, f.Entries(), f.ChoosingToReplaceTheSimbridge({"simbridge"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(did.packages.front().landed.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    QVERIFY(!f.fileSystem.Exists(kInLibrary));
}

void ArchiveInstallServiceTest::AnOccupantTheOfferDidNotShowLeavesThePackageOutAndTheOthersGoOn()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"alpha", "1.0.0"}});

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"simbridge", "alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].root, std::string{"simbridge"});
    QCOMPARE(did.packages[0].result, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(!did.packages[0].replaced);
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
    QCOMPARE(f.archives.extractions.size(), std::size_t{1});
}

void ArchiveInstallServiceTest::TheReplacementTheOfferShowedStillHappens()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"alpha", "1.0.0"}});

    const WhatInstallingDid did =
        f.service.Install(f.profile, f.Entries(), f.ChoosingToReplaceTheSimbridge({"simbridge", "alpha"}));

    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].result, FileResult::Completed);
    QVERIFY(did.packages[0].replaced);
    ExpectAlphaLanded(f, did.packages[1]);
    QVERIFY(!did.packages[1].replaced);
}

void ArchiveInstallServiceTest::TheExtractionIsOneRowAfterTheVerificationAndWithoutAnAddonName()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    std::size_t rowsWhenTheVerificationStarted = 99;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}), {},
                                                    [&f, &rowsWhenTheVerificationStarted](const OperationKind kind)
                                                    {
                                                        if (kind == OperationKind::ImportVerifyStaging)
                                                        {
                                                            rowsWhenTheVerificationStarted = f.journal.appended.size();
                                                        }
                                                    });

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(rowsWhenTheVerificationStarted, std::size_t{0});
    QCOMPARE(static_cast<int>(std::ranges::count(f.KindsJournalled(), OperationKind::InstallExtractToStaging)), 1);

    const OperationRecord& extract = f.journal.appended.front();
    QCOMPARE(extract.kind, OperationKind::InstallExtractToStaging);
    QCOMPARE(extract.label, std::string{"pack.zip"});
    QVERIFY(extract.addonId.folderName.empty());
    QCOMPARE(extract.addonId.libraryId, std::string{"lib-1"});
    QCOMPARE(std::get<FileResult>(extract.outcome), FileResult::Completed);
}

void ArchiveInstallServiceTest::AFailedVerificationIsTheResultOfTheOneExtractionRow()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.LeavesOut("alpha/scenery/a.bgl");

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::InstallExtractToStaging}));
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::VerificationFailed);
    QCOMPARE(f.journal.appended.front().label, std::string{"pack.zip"});
}

void ArchiveInstallServiceTest::AStagingThatCannotBeRemovedIsReportedAndRecordedAfterAnInstall()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    ExpectAlphaLanded(f, did.packages.front());
    QVERIFY(did.theStagingStayed);
    QVERIFY(f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QCOMPARE(f.KindsJournalled(),
             (std::vector<OperationKind>{OperationKind::InstallExtractToStaging, OperationKind::InstallMoveIntoPlace,
                                         OperationKind::DiscardStaging}));

    const OperationRecord& discard = f.journal.appended.back();
    QCOMPARE(discard.source, InstallStagingIn(kUtils));
    QCOMPARE(std::get<FileResult>(discard.outcome), FileResult::CouldNotDiscard);
    QVERIFY2(discard.addonId.folderName.empty(), "the staging folder is no addon");
    QCOMPARE(discard.addonId.libraryId, std::string{"lib-1"});
}

void ArchiveInstallServiceTest::AStagingThatCannotBeRemovedIsReportedAfterAFailedVerification()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.LeavesOut("alpha/scenery/a.bgl");
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(did.theStagingStayed);
    QCOMPARE(f.KindsJournalled(),
             (std::vector<OperationKind>{OperationKind::InstallExtractToStaging, OperationKind::DiscardStaging}));
}

void ArchiveInstallServiceTest::AStagingThatWasRemovedIsNotReported()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.FailsAfterWritingEverything(ArchiveFailure::CouldNotExtract);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.stopped, FileResult::CouldNotExtract);
    QVERIFY(!did.theStagingStayed);
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
}

void ArchiveInstallServiceTest::AnArchiveThatTimedOutWhileExtractingIsTookTooLongAndLeavesNoStaging()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    f.archives.FailsAfterWritingEverything(ArchiveFailure::TookTooLong);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"alpha"}));

    QCOMPARE(did.failure, ArchiveFailure::TookTooLong);
    QCOMPARE(did.stopped, FileResult::Completed);
    QVERIFY(did.packages.empty());
    QVERIFY(!did.theStagingStayed);
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    QCOMPARE(f.KindsJournalled(), (std::vector<OperationKind>{OperationKind::InstallExtractToStaging}));
    QCOMPARE(std::get<FileResult>(f.journal.appended.front().outcome), FileResult::CouldNotExtract);
    ExpectTheArchiveIsUntouched(f);
}

void ArchiveInstallServiceTest::TheSameFolderWrittenInTwoCasesIsInstalled()
{
    Fixture f;
    f.archives.Lists({File("Pkg/manifest.json", kManifestBytes), File("Pkg/a.txt", 11), File("pkg/b.txt", 13)});
    f.archives.Holds("Pkg/manifest.json", "ok:1.0.0");
    f.archives.MergesFoldersThatDifferOnlyInCase();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"Pkg"}));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QCOMPARE(f.fileSystem.FileSize(kUtils / "Pkg" / "a.txt"), std::uintmax_t{11});
    QCOMPARE(f.fileSystem.FileSize(kUtils / "Pkg" / "b.txt"), std::uintmax_t{13});
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kUtils)));
}

void ArchiveInstallServiceTest::AFileMissingFromAFolderWrittenInTwoCasesStillFailsTheVerification()
{
    Fixture f;
    f.archives.Lists({File("Pkg/manifest.json", kManifestBytes), File("Pkg/a.txt", 11), File("pkg/b.txt", 13)});
    f.archives.Holds("Pkg/manifest.json", "ok:1.0.0");
    f.archives.MergesFoldersThatDifferOnlyInCase();
    f.archives.LeavesOut("pkg/b.txt");

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), f.Choosing({"Pkg"}));

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(!f.fileSystem.Exists(kUtils / "Pkg"));
}

void ArchiveInstallServiceTest::AnArchiveInsideTheStagingIsRefusedBeforeAnyWrite()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.Choosing({"alpha"});
    request.archive = InstallStagingIn(kUtils) / "pack.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

void ArchiveInstallServiceTest::AnArchiveInsideTheStagingFolderItselfIsRefusedToo()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.Choosing({"alpha"});
    request.archive = InstallStagingIn(kUtils) / "deeper" / "still" / "pack.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    ExpectNothingWasExtractedOrJournalled(f);
}

void ArchiveInstallServiceTest::AnArchiveInsideWhatWouldBeReplacedRefusesOnlyThatPackage()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge", "alpha"});
    request.archive = kInLibrary / "downloads" / "pack.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(!did.packages[0].replaced);
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::AnArchiveBesideTheOccupantIsNotInsideIt()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"});
    request.archive = "D:/Library/Utils/simbridge-2.0.0.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(did.packages.front().replaced);
}

void ArchiveInstallServiceTest::AReplacementWithTheRootOfTheLibraryAsCategoryLandsAtTheOccupantAndLeavesNoStaging()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"});
    request.category = kLibrary;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(did.packages.front().replaced);
    QCOMPARE(did.packages.front().landed, kInLibrary);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kManifestBytes);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(InstallStagingIn(kLibrary)));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::AReplacementWithoutAnyCategoryIsRefusedAsNotInALibraryAndNothingIsTouched()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"});
    request.category = std::filesystem::path{};

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheTargetIsNotInALibrary);
    QVERIFY(f.archives.extractions.empty());
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::ASourceIsAFolderOnlyWhenItIsAPhysicalFolder()
{
    Fixture f;
    const std::filesystem::path folder = "D:/Downloads/Meu Addon";
    f.fileSystem.AddDirectory(folder);
    f.fileSystem.AddLink("D:/Downloads/Shortcut", folder);

    QVERIFY(f.service.IsAFolder(folder));
    QVERIFY(f.service.IsAFolder(kLibrary));
    QVERIFY(!f.service.IsAFolder(kArchive));
    QVERIFY(!f.service.IsAFolder("D:/Downloads/Shortcut"));
    QVERIFY(!f.service.IsAFolder("D:/Downloads/nothing"));
}

void ArchiveInstallServiceTest::InspectingSaysWhetherTheSourceIsAFolderWhateverTheOutcome()
{
    Fixture f;
    const std::filesystem::path folder = "D:/Downloads/Meu Addon";
    f.fileSystem.AddDirectory(folder);
    f.TheArchiveHolds({{"alpha", "1.0.0"}});

    const WhatIsInTheArchive fromAFolder = f.service.Inspect(f.profile, folder, kUtils);
    const WhatIsInTheArchive fromAnArchive = f.service.Inspect(f.profile, kArchive, kUtils);

    QVERIFY(fromAFolder.theSourceIsAFolder);
    QVERIFY(!fromAnArchive.theSourceIsAFolder);
    QCOMPARE(fromAFolder.packages.size(), std::size_t{1});

    f.archives.FailsToList(ArchiveFailure::CouldNotOpen);

    QVERIFY(f.service.Inspect(f.profile, folder, kUtils).theSourceIsAFolder);
    QVERIFY(!f.service.Inspect(f.profile, kArchive, kUtils).theSourceIsAFolder);

    f.archives.FailsToList(ArchiveFailure::None);
    f.archives.Lists({File("readme.txt")});

    QVERIFY(f.service.Inspect(f.profile, folder, kUtils).theSourceIsAFolder);
    QCOMPARE(f.service.Inspect(f.profile, folder, kUtils).refusal, NothingToInstall::NoPackage);
}

void ArchiveInstallServiceTest::ALinkToAFolderThatReachesTheReadingIsReadAsAFolderSource()
{
    Fixture f;
    const std::filesystem::path folder = "D:/Downloads/Meu Addon";
    const std::filesystem::path junction = "D:/Downloads/Shortcut";
    const std::filesystem::path linkToTheArchive = "D:/Downloads/ShortcutToTheArchive";
    f.fileSystem.AddDirectory(folder);
    f.fileSystem.AddLink(junction, folder);
    f.fileSystem.AddLink(linkToTheArchive, kArchive);
    f.archives.FailsToList(ArchiveFailure::CouldNotOpen);

    const WhatIsInTheArchive throughTheJunction = f.service.Inspect(f.profile, junction, kUtils);

    QCOMPARE(throughTheJunction.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY2(throughTheJunction.theSourceIsAFolder, "the reader takes a junction to a folder for a folder");
    QVERIFY2(!f.service.IsAFolder(junction), "the drag still refuses a folder that is itself a link");
    QVERIFY(!f.service.Inspect(f.profile, linkToTheArchive, kUtils).theSourceIsAFolder);
    QVERIFY(!f.service.Inspect(f.profile, "D:/Downloads/nothing", kUtils).theSourceIsAFolder);
}

void ArchiveInstallServiceTest::TwoOriginsWithDifferentContentAreInspectedEachByItsOwn()
{
    Fixture f;
    const std::filesystem::path other = "D:/Downloads/other.zip";
    f.fileSystem.AddFileWithContents(other, "another archive");
    f.TheOriginHolds(kArchive, {{"alpha", "1.0.0"}});
    f.TheOriginHolds(other, {{"alpha", "2.0.0"}, {"beta", "3.0.0"}});

    const WhatIsInTheArchive first = f.service.Inspect(f.profile, kArchive, kUtils);
    const WhatIsInTheArchive second = f.service.Inspect(f.profile, other, kUtils);

    QCOMPARE(first.packages.size(), std::size_t{1});
    QCOMPARE(first.packages.front().versionInTheArchive, std::string{"1.0.0"});
    QCOMPARE(second.packages.size(), std::size_t{2});
    QCOMPARE(second.packages.front().versionInTheArchive, std::string{"2.0.0"});
    QCOMPARE(second.packages.back().versionInTheArchive, std::string{"3.0.0"});
}

void ArchiveInstallServiceTest::AFolderThatIsTheStagingIsRefusedBeforeAnyWrite()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.Choosing({"alpha"});
    request.archive = InstallStagingIn(kUtils);

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
}

void ArchiveInstallServiceTest::AFolderThatContainsTheCategoryIsRefusedBeforeAnyWrite()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.Choosing({"alpha"});
    request.archive = kUtils;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(did.packages.empty());
    ExpectNothingWasExtractedOrJournalled(f);
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

void ArchiveInstallServiceTest::AFolderThatContainsTheLibraryIsRefusedBeforeAnyWrite()
{
    Fixture f;
    f.TheArchiveHolds({{"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.Choosing({"alpha"});
    request.archive = kLibrary.parent_path();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    ExpectNothingWasExtractedOrJournalled(f);
}

void ArchiveInstallServiceTest::AFolderThatIsTheOccupantRefusesOnlyThatPackage()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge", "alpha"});
    request.archive = kInLibrary;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(!did.packages[0].replaced);
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::AFolderThatContainsTheOccupantRefusesOnlyThatPackage()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.fileSystem.AddDirectory(kOtherCategory);
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"});
    request.category = kOtherCategory;
    request.archive = kUtils;

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(f.archives.extractions.empty());
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::AFolderBesideTheOccupantIsNotNestedWithIt()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"});
    request.archive = "D:/Library/Utils/simbridge-2.0.0";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(did.packages.front().replaced);
}

void ArchiveInstallServiceTest::ASourceInsideTheEarlierQuarantinedCopyIsRefusedWhenThatCopyIsToBeDiscarded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}, {"alpha", "1.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge", "alpha"}, true);
    request.archive = kHeldInLibrary / "downloads" / "pack.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{2});
    QCOMPARE(did.packages[0].result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(!did.packages[0].replaced);
    ExpectAlphaLanded(f, did.packages[1]);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    ExpectTheTwoLinksStandAtTheOccupant(f);
}

void ArchiveInstallServiceTest::ASourceThatContainsTheEarlierQuarantinedCopyIsRefusedWhenThatCopyIsToBeDiscarded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"}, true);
    request.archive = kHeldInLibrary.parent_path();

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QVERIFY(f.archives.extractions.empty());
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
}

void ArchiveInstallServiceTest::ASourceBesideTheEarlierQuarantinedCopyIsInstalledWhenThatCopyIsDiscarded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"}, true);
    request.archive = "D:/Library/_fsorganizer-quarantine/simbridge-2.0.0";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(did.packages.front().replaced);
}

void ArchiveInstallServiceTest::
    ASourceInsideTheEarlierQuarantinedCopyWaitsForTheOccupiedQuarantineWhenNothingIsDiscarded()
{
    Fixture f;
    f.ASimbridgeIsInstalledWithTwoLinks("1.0.0");
    f.TheQuarantineHoldsAnEarlierSimbridge();
    f.TheArchiveHolds({{"simbridge", "2.0.0"}});
    ArchiveInstallRequest request = f.ChoosingToReplaceTheSimbridge({"simbridge"}, false);
    request.archive = kHeldInLibrary / "downloads" / "pack.zip";

    const WhatInstallingDid did = f.service.Install(f.profile, f.Entries(), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheQuarantineIsOccupied);
    QCOMPARE(f.fileSystem.FileSize(kHeldInLibrary / "manifest.json"), 3 * kMegabyte);
}

QTEST_APPLESS_MAIN(ArchiveInstallServiceTest)

#include "tst_archive_install_service.moc"
