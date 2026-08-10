#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "application/Session.h"
#include "domain/importing/ExternalSidecar.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
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
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/RecordingSessionObserver.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class SessionTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheProfileAndItsSnapshotOnlyLandTogetherWhenTheScanFinishes();
        static void AScanAskedForWhileAnotherIsRunningIsRunAgainInsteadOfLost();
        static void ChoosingAProfileRemembersTheChoiceBeforeReadingTheDisk();
        static void RegisteringALibrarySavesTheProfileAndReadsTheDiskAgain();
        static void OverridingADestinationSavesTheProfileWithoutAnotherScan();
        static void OverridingADestinationForAWholeSelectionSavesOnceInsteadOfOncePerNode();
        static void ADestinationAskedForOutsideEveryLibraryChangesNothingAndSavesNothing();
        static void RefreshingTheEntriesSeesALinkThatAppearedAfterTheScan();
        static void RefreshingTheEntriesSeesACopyThatAppearedAfterTheScan();
        static void CreatingACategoryReadsTheDiskAgainSoTheTreeShowsIt();
        static void RenamingACategorySavesTheCarriedOverridesAndReadsTheDiskAgain();
        static void ARefusedCategoryLeavesTheProfileAndTheDiskAlone();
        static void TheSimulatorWarningIsGivenOncePerSessionNoMatterWhoChangedALink();
        static void MovingAnAddonCarriesItsOverrideAndReadsTheDiskAgain();
        static void UnregisteringALibraryLeavesTheDiskUntouchedAndItsLinksBecomeThirdParty();
        static void RepointingADestinationCarriesTheOverrideAndReadsTheDiskAgain();
        static void UnregisteringALibraryDropsAnUndoThatWouldPointAtIt();
        static void RepointingADestinationDropsAnUndoThatWouldPointAtTheOldPath();
        static void ImportingALegacyLibraryRegistersItSavesTheProfileAndReadsTheDiskAgain();
        static void ImportingALegacyCategoryDeclaresTheFolderThatIsAlreadyThere();
        static void ALegacyLibraryInsideOneAlreadyRegisteredIsRefusedAndReported();
        static void ImportingNothingSavesNothingAndScansNothing();
        static void RemovingTheActiveProfileAdoptsTheOneThatIsLeftAndReadsItsDisk();
        static void RemovingAProfileThatIsNotInUseLeavesTheDiskAlone();
        static void TheLastProfileIsNeverRemovedAndNothingIsWritten();
        static void ALegacyCategoryThatIsRefusedIsReportedInsteadOfSilentlyDropped();
        static void ImportingALegacyLibraryLeavesItsTreeReadableBeforeTheCallerAsksAgain();
        static void AnOverridePointingNowhereIsReportedInsteadOfDisappearingOnItsOwn();
        static void DroppingTheOverridesThatPointNowhereWritesTheProfileWithoutAnotherScan();
        static void AnImportFromAnotherProgramIsRememberedInTheSettingsAndReadAgain();
        static void APlainImportRemembersNothingAndScansNothingAgain();
        static void MovingAnAddonCarriesTheRecordOfWhereItCameFrom();
        static void UnregisteringALibraryForgetsWhereItsAddonsCameFrom();
        static void GivingAnAddonBackForgetsWhereItCameFromAndScansAgain();
        static void ForgettingNothingNeitherSavesNorScans();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kExtraLibrary = "D:/MSFS 2024 Extra";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kSceneries = "D:/MSFS 2024/Sceneries";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode node = CategoryNode(kLibrary, {CategoryNode("D:/MSFS 2024/Aircrafts", {AddonNode(kAddon)})});
        node.kind = TreeNodeKind::Library;

        return node;
    }

    SimulatorProfile Profile(const std::string& id)
    {
        SimulatorProfile profile;
        profile.id = id;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kOtherDestination};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    void DescribeDirectory(const InMemoryFileSystem& fileSystem,
                           const std::filesystem::path& root,
                           std::vector<std::string>& into)
    {
        for (const std::filesystem::path& child : fileSystem.ChildDirectoriesOf(root))
        {
            if (fileSystem.IsLink(child))
            {
                into.push_back(ComparablePath(child) + " -> "
                               + ComparablePath(fileSystem.LinkTarget(child).value_or(std::filesystem::path{})));
                continue;
            }

            into.push_back(ComparablePath(child) + "/");
            DescribeDirectory(fileSystem, child, into);
        }
    }

    [[nodiscard]] std::vector<std::string> DescribeTheDisk(const InMemoryFileSystem& fileSystem)
    {
        std::vector<std::string> description;

        for (const auto& root : {kLibrary, kExtraLibrary, kCommunity, kOtherDestination})
        {
            if (!fileSystem.Exists(root))
            {
                continue;
            }

            description.push_back(ComparablePath(root) + "/");
            DescribeDirectory(fileSystem, root, description);

            for (const std::filesystem::path& file : fileSystem.FilesUnder(root))
            {
                description.push_back(ComparablePath(file));
            }
        }

        std::ranges::sort(description);

        return description;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile("msfs2024")};
            settings.stored.activeProfileId = "msfs2024";
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
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        RecordingSessionObserver observer;
        Session session{service, organizer, settings, processProbe, runner, observer};
    };
}

void SessionTest::TheProfileAndItsSnapshotOnlyLandTogetherWhenTheScanFinishes()
{
    Fixture f;
    f.runner.defer = true;

    f.session.ShowActiveProfile();

    QVERIFY(f.session.Scanning());
    QVERIFY(f.session.Profile().id.empty());
    QVERIFY(f.session.Snapshot().libraries.empty());
    QCOMPARE(f.observer.started, 1);
    QCOMPARE(f.observer.finished, 0);

    f.runner.Finish();

    QVERIFY(!f.session.Scanning());
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2024"));
    QCOMPARE(f.session.Snapshot().libraries.size(), std::size_t{1});
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::AScanAskedForWhileAnotherIsRunningIsRunAgainInsteadOfLost()
{
    Fixture f;
    f.runner.defer = true;

    f.session.ShowActiveProfile();
    f.session.ShowActiveProfile();

    QCOMPARE(f.runner.runs, 1);
    QCOMPARE(f.observer.started, 1);

    f.runner.Finish();

    QCOMPARE(f.runner.runs, 2);
    QCOMPARE(f.observer.started, 2);
    QCOMPARE(f.observer.finished, 0);
    QVERIFY(f.session.Scanning());

    f.runner.Finish();

    QCOMPARE(f.observer.finished, 1);
    QVERIFY(!f.session.Scanning());
    QCOMPARE(f.session.Snapshot().libraries.size(), std::size_t{1});
}

void SessionTest::ChoosingAProfileRemembersTheChoiceBeforeReadingTheDisk()
{
    Fixture f;
    f.settings.stored.profiles.push_back(Profile("msfs2020"));

    f.session.ChooseProfile("msfs2020");

    QCOMPARE(QString::fromStdString(f.settings.stored.activeProfileId), QStringLiteral("msfs2020"));
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2020"));
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::RegisteringALibrarySavesTheProfileAndReadsTheDiskAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();
    f.fileSystem.AddDirectory(kExtraLibrary);
    f.catalog.SetTree(kExtraLibrary, TreeNode{});

    const LibraryReport report = f.session.RegisterLibrary(kExtraLibrary);

    QVERIFY(report.Accepted());
    QCOMPARE(f.session.Profile().libraries.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().libraries.size(), std::size_t{2});
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::OverridingADestinationSavesTheProfileWithoutAnotherScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);

    f.session.OverrideDestination({&addon}, kOtherDestination);

    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.observer.refreshed, 1);
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::OverridingADestinationForAWholeSelectionSavesOnceInsteadOfOncePerNode()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode addon = AddonNode(kAddon);
    const TreeNode category = AddonNode(kSceneries);

    f.session.OverrideDestination({&addon, &category}, kOtherDestination);

    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{2});
    QCOMPARE(f.observer.refreshed, 1);
}

void SessionTest::ADestinationAskedForOutsideEveryLibraryChangesNothingAndSavesNothing()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const TreeNode stranger = AddonNode("E:/Somewhere Else/addon");

    f.session.OverrideDestination({&stranger}, kOtherDestination);

    QVERIFY(f.session.Profile().destinationOverrides.empty());
    QCOMPARE(f.observer.refreshed, 0);
}

void SessionTest::RefreshingTheEntriesSeesALinkThatAppearedAfterTheScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QVERIFY(!f.session.Snapshot().enabled.Contains(kAddon));

    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.RefreshEntries();

    QVERIFY(f.session.Snapshot().enabled.Contains(kAddon));
    QCOMPARE(f.observer.refreshed, 1);
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::RefreshingTheEntriesSeesACopyThatAppearedAfterTheScan()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QCOMPARE(f.session.Snapshot().conflicts.Count(), std::size_t{0});

    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w");
    f.session.RefreshEntries();

    QCOMPARE(f.session.Snapshot().conflicts.Count(), std::size_t{1});
}

void SessionTest::CreatingACategoryReadsTheDiskAgainSoTheTreeShowsIt()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const FileOperationResult result = f.session.CreateCategory(kLibrary, "Sceneries");

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(std::filesystem::path(kLibrary) / "Sceneries"));
    QCOMPARE(f.observer.started, 2);
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::RenamingACategorySavesTheCarriedOverridesAndReadsTheDiskAgain()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kOtherDestination}};
    f.session.ShowActiveProfile();

    const FileOperationResult result = f.session.RenameCategory(kAircrafts, "Airplanes");

    QCOMPARE(result.result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Airplanes"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Airplanes"));
    QCOMPARE(f.observer.started, 2);
}

void SessionTest::ARefusedCategoryLeavesTheProfileAndTheDiskAlone()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kOtherDestination}};
    f.session.ShowActiveProfile();
    f.processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(f.session.RenameCategory(kAircrafts, "Airplanes").result, FileResult::TheSimulatorIsRunning);
    QCOMPARE(f.session.CreateCategory(kLibrary, "Sceneries").result, FileResult::TheSimulatorIsRunning);

    QVERIFY(!f.fileSystem.Exists("D:/MSFS 2024/Airplanes"));
    QVERIFY(!f.fileSystem.Exists("D:/MSFS 2024/Sceneries"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Aircrafts"));
    QCOMPARE(f.observer.started, 1);
}

void SessionTest::MovingAnAddonCarriesItsOverrideAndReadsTheDiskAgain()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.fileSystem.AddDirectory(kSceneries);
    f.settings.stored.profiles.front().destinationOverrides = {DestinationOverride{
        .libraryId = "library-1", .relativePath = "Aircrafts/pmdg-aircraft-77w", .destination = kOtherDestination}};
    f.session.ShowActiveProfile();

    const std::vector<FileOperationResult> results =
        f.session.MoveAddons({AddonMove{.addonFolder = kAddon, .category = kSceneries}});

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Sceneries/pmdg-aircraft-77w"));
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().destinationOverrides.front().relativePath),
             ComparablePath("Sceneries/pmdg-aircraft-77w"));
    QCOMPARE(f.observer.started, 2);
}

void SessionTest::TheSimulatorWarningIsGivenOncePerSessionNoMatterWhoChangedALink()
{
    Fixture f;
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<LinkOperationResult> changed = {
        LinkOperationResult{.addonId = AddonId{.libraryId = "library-1", .folderName = "pmdg-aircraft-77w"},
                            .addonFolder = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
                            .linkPath = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                            .kind = OperationKind::EnableAddon,
                            .outcome = LinkOutcome::Success()}};

    f.session.NoteLinkResults(changed);
    f.session.NoteLinkResults(changed);

    QCOMPARE(f.observer.simulatorWarnings, 1);
    QCOMPARE(f.observer.restartReports, 1);
    QVERIFY(f.observer.restartPending);
}

void SessionTest::UnregisteringALibraryLeavesTheDiskUntouchedAndItsLinksBecomeThirdParty()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/manifest.json");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kAddon);
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kOtherDestination}};

    f.session.ShowActiveProfile();

    QCOMPARE(f.session.Snapshot().entries.size(), std::size_t{1});
    QCOMPARE(f.session.Snapshot().entries.front().classification, EntryClassification::Managed);

    const std::vector<std::string> before = DescribeTheDisk(f.fileSystem);
    QVERIFY2(std::ranges::find(before,
                               std::string("e:/flight simulator 2024/community/pmdg-aircraft-77w -> "
                                           "d:/msfs 2024/aircrafts/pmdg-aircraft-77w"))
                 != before.end(),
             "the disk description did not see the link, so comparing before with after proves nothing");

    f.session.UnregisterLibrary("library-1");

    QCOMPARE(DescribeTheDisk(f.fileSystem), before);
    QVERIFY(f.session.Profile().libraries.empty());
    QVERIFY(f.session.Profile().destinationOverrides.empty());
    QVERIFY(f.settings.stored.profiles.front().libraries.empty());

    QCOMPARE(f.session.Snapshot().entries.size(), std::size_t{1});
    QCOMPARE(f.session.Snapshot().entries.front().classification, EntryClassification::External);
}

void SessionTest::RepointingADestinationCarriesTheOverrideAndReadsTheDiskAgain()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community2025");
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kOtherDestination}};

    f.session.ShowActiveProfile();
    const int scansBefore = f.observer.started;

    f.session.RepointDestination(kOtherDestination, "E:/Flight Simulator 2024/Community2025");

    QCOMPARE(f.session.Profile().destinations[1], std::filesystem::path("E:/Flight Simulator 2024/Community2025"));
    QCOMPARE(f.session.Profile().destinationOverrides.front().destination,
             std::filesystem::path("E:/Flight Simulator 2024/Community2025"));
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.front().destination,
             std::filesystem::path("E:/Flight Simulator 2024/Community2025"));
    QCOMPARE(f.observer.started, scansBefore + 1);
}

void SessionTest::UnregisteringALibraryDropsAnUndoThatWouldPointAtIt()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/manifest.json");
    f.session.ShowActiveProfile();

    const TreeNode* addon = AddonNamed(f.session.Snapshot().libraries, "pmdg-aircraft-77w");
    QVERIFY(addon != nullptr);
    static_cast<void>(f.service.SetEnabled(f.session.Profile(), f.session.Snapshot(), {addon}, true));
    QVERIFY2(f.service.CanUndo(), "nothing entered the undo stack, so demanding it empty afterwards proves nothing");

    f.session.UnregisterLibrary("library-1");

    QVERIFY2(!f.service.CanUndo(), "unregistering kept an undo that points at a library that left");
}

void SessionTest::RepointingADestinationDropsAnUndoThatWouldPointAtTheOldPath()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/manifest.json");
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community2025");
    f.session.ShowActiveProfile();

    const TreeNode* addon = AddonNamed(f.session.Snapshot().libraries, "pmdg-aircraft-77w");
    QVERIFY(addon != nullptr);
    static_cast<void>(f.service.SetEnabled(f.session.Profile(), f.session.Snapshot(), {addon}, true));
    QVERIFY2(f.service.CanUndo(), "nothing entered the undo stack, so demanding it empty afterwards proves nothing");

    f.session.RepointDestination(kCommunity, "E:/Flight Simulator 2024/Community2025");

    QVERIFY2(!f.service.CanUndo(), "repointing kept an undo that points at the old path");
}

void SessionTest::ImportingALegacyLibraryRegistersItSavesTheProfileAndReadsTheDiskAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();
    f.fileSystem.AddDirectory(kExtraLibrary);
    f.catalog.SetTree(kExtraLibrary, TreeNode{});

    const LegacyImportReport report =
        f.session.ImportLegacy(LegacyImportRequest{.libraryRoots = {kExtraLibrary}, .categories = {}});

    QCOMPARE(report.librariesRegistered, std::size_t{1});
    QVERIFY(report.refused.empty());
    QCOMPARE(f.session.Profile().libraries.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().libraries.size(), std::size_t{2});
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::ImportingALegacyCategoryDeclaresTheFolderThatIsAlreadyThere()
{
    Fixture f;
    f.session.ShowActiveProfile();
    f.fileSystem.AddDirectory(kSceneries);
    const std::vector<std::string> before = DescribeTheDisk(f.fileSystem);

    const LegacyImportReport report =
        f.session.ImportLegacy(LegacyImportRequest{.libraryRoots = {}, .categories = {kSceneries}});

    QCOMPARE(report.categoriesDeclared, std::size_t{1});
    QVERIFY(f.fileSystem.IsFile(std::filesystem::path(kSceneries) / ".fsorg-category"));
    QVERIFY(f.fileSystem.IsDirectory(kAddon));
    QVERIFY(before.size() < DescribeTheDisk(f.fileSystem).size());
}

void SessionTest::ALegacyLibraryInsideOneAlreadyRegisteredIsRefusedAndReported()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const LegacyImportReport report =
        f.session.ImportLegacy(LegacyImportRequest{.libraryRoots = {kAircrafts}, .categories = {}});

    QCOMPARE(report.librariesRegistered, std::size_t{0});
    QCOMPARE(report.refused.size(), std::size_t{1});
    QCOMPARE(ComparablePath(report.refused.front()), ComparablePath(kAircrafts));
    QCOMPARE(f.session.Profile().libraries.size(), std::size_t{1});
}

void SessionTest::ImportingNothingSavesNothingAndScansNothing()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const LegacyImportReport report = f.session.ImportLegacy({});

    QCOMPARE(report.librariesRegistered, std::size_t{0});
    QCOMPARE(report.categoriesDeclared, std::size_t{0});
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::RemovingTheActiveProfileAdoptsTheOneThatIsLeftAndReadsItsDisk()
{
    Fixture f;
    f.settings.stored.profiles.push_back(Profile("msfs2020"));
    f.session.ShowActiveProfile();

    QVERIFY(f.session.RemoveProfile("msfs2024"));

    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(f.settings.stored.activeProfileId), QStringLiteral("msfs2020"));
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2020"));
    QCOMPARE(f.observer.finished, 2);
}

void SessionTest::RemovingAProfileThatIsNotInUseLeavesTheDiskAlone()
{
    Fixture f;
    f.settings.stored.profiles.push_back(Profile("msfs2020"));
    f.session.ShowActiveProfile();

    QVERIFY(f.session.RemoveProfile("msfs2020"));

    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2024"));
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::ALegacyCategoryThatIsRefusedIsReportedInsteadOfSilentlyDropped()
{
    Fixture f;
    f.session.ShowActiveProfile();

    const LegacyImportReport report =
        f.session.ImportLegacy(LegacyImportRequest{.libraryRoots = {}, .categories = {kCommunity}});

    QCOMPARE(report.categoriesDeclared, std::size_t{0});
    QCOMPARE(report.refused.size(), std::size_t{1});
    QCOMPARE(ComparablePath(report.refused.front()), ComparablePath(kCommunity));
}

void SessionTest::ImportingALegacyLibraryLeavesItsTreeReadableBeforeTheCallerAsksAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();
    f.fileSystem.AddDirectory(kExtraLibrary);
    f.catalog.SetTree(kExtraLibrary, TreeNode{});
    f.runner.defer = true;

    const LegacyImportReport report =
        f.session.ImportLegacy(LegacyImportRequest{.libraryRoots = {kExtraLibrary}, .categories = {}});

    QCOMPARE(report.librariesRegistered, std::size_t{1});
    QVERIFY2(!f.runner.Pending(), "the import left the scan hanging on the runner");
    QCOMPARE(f.session.Snapshot().libraries.size(), std::size_t{2});
}

void SessionTest::TheLastProfileIsNeverRemovedAndNothingIsWritten()
{
    Fixture f;
    f.session.ShowActiveProfile();

    QVERIFY(!f.session.RemoveProfile("msfs2024"));

    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(f.session.Profile().id), QStringLiteral("msfs2024"));
    QCOMPARE(f.observer.finished, 1);
}

void SessionTest::AnOverridePointingNowhereIsReportedInsteadOfDisappearingOnItsOwn()
{
    Fixture f;
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{
            .libraryId = "library-1", .relativePath = "Aircrafts", .destination = "E:/Flight Simulator 2024/Retired"},
        DestinationOverride{.libraryId = "library-1", .relativePath = "Sceneries", .destination = kOtherDestination}};

    f.session.ShowActiveProfile();

    QCOMPARE(f.session.OverridesPointingNowhere().size(), std::size_t{1});
    QCOMPARE(f.session.Profile().destinationOverrides.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{2});
}

void SessionTest::DroppingTheOverridesThatPointNowhereWritesTheProfileWithoutAnotherScan()
{
    Fixture f;
    f.settings.stored.profiles.front().destinationOverrides = {
        DestinationOverride{
            .libraryId = "library-1", .relativePath = "Aircrafts", .destination = "E:/Flight Simulator 2024/Retired"},
        DestinationOverride{.libraryId = "library-1", .relativePath = "Sceneries", .destination = kOtherDestination}};

    f.session.ShowActiveProfile();
    f.session.DropOverridesPointingNowhere();

    QVERIFY(f.session.OverridesPointingNowhere().empty());
    QCOMPARE(f.settings.stored.profiles.front().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(f.observer.finished, 1);
    QCOMPARE(f.observer.refreshed, 1);
}

namespace
{
    constexpr auto kVendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";

    [[nodiscard]] ImportOperationResult LandedFromAnotherProgram()
    {
        return ImportOperationResult{.request = ImportRequest{.source = std::filesystem::path(kCommunity) / "gsx-pro",
                                                              .category = kAircrafts,
                                                              .externalSource = kVendorFolder},
                                     .result = FileResult::Completed};
    }
}

void SessionTest::AnImportFromAnotherProgramIsRememberedInTheSettingsAndReadAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();

    f.session.RememberWhatCameFromAnotherProgram({LandedFromAnotherProgram()});

    const std::vector<ExternalOrigin>& remembered = f.settings.stored.profiles.front().externalOrigins;
    QCOMPARE(remembered.size(), std::size_t{1});
    QCOMPARE(remembered.front().libraryId, LibraryId{"library-1"});
    QCOMPARE(ComparablePath(remembered.front().relativePath), ComparablePath("Aircrafts/gsx-pro"));
    QCOMPARE(remembered.front().externalPath, std::filesystem::path{kVendorFolder});
    QCOMPARE(f.observer.started, 2);
}

void SessionTest::APlainImportRemembersNothingAndScansNothingAgain()
{
    Fixture f;
    f.session.ShowActiveProfile();

    ImportOperationResult plain = LandedFromAnotherProgram();
    plain.request.externalSource.clear();

    f.session.RememberWhatCameFromAnotherProgram({plain});

    QVERIFY(f.settings.stored.profiles.front().externalOrigins.empty());
    QCOMPARE(f.observer.started, 1);
}

void SessionTest::MovingAnAddonCarriesTheRecordOfWhereItCameFrom()
{
    Fixture f;
    f.fileSystem.AddDirectory(kAircrafts);
    f.fileSystem.AddDirectory(kSceneries);
    f.settings.stored.profiles.front().externalOrigins = {ExternalOrigin{
        .libraryId = "library-1", .relativePath = "Aircrafts/pmdg-aircraft-77w", .externalPath = kVendorFolder}};
    f.session.ShowActiveProfile();

    const std::vector<FileOperationResult> results =
        f.session.MoveAddons({AddonMove{.addonFolder = kAddon, .category = kSceneries}});

    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(ComparablePath(f.settings.stored.profiles.front().externalOrigins.front().relativePath),
             ComparablePath("Sceneries/pmdg-aircraft-77w"));
}

void SessionTest::UnregisteringALibraryForgetsWhereItsAddonsCameFrom()
{
    Fixture f;
    f.settings.stored.profiles.front().externalOrigins = {ExternalOrigin{
        .libraryId = "library-1", .relativePath = "Aircrafts/pmdg-aircraft-77w", .externalPath = kVendorFolder}};
    f.session.ShowActiveProfile();

    f.session.UnregisterLibrary("library-1");

    QVERIFY(f.settings.stored.profiles.front().externalOrigins.empty());
}

void SessionTest::GivingAnAddonBackForgetsWhereItCameFromAndScansAgain()
{
    Fixture f;
    f.settings.stored.profiles.front().externalOrigins = {ExternalOrigin{
        .libraryId = "library-1", .relativePath = "Aircrafts/pmdg-aircraft-77w", .externalPath = kVendorFolder}};
    f.session.ShowActiveProfile();

    f.session.ForgetWhatCameFromAnotherProgram({kAddon});

    QVERIFY(f.settings.stored.profiles.front().externalOrigins.empty());
    QCOMPARE(f.observer.started, 2);
}

void SessionTest::ForgettingNothingNeitherSavesNorScans()
{
    Fixture f;
    f.settings.stored.profiles.front().externalOrigins = {ExternalOrigin{
        .libraryId = "library-1", .relativePath = "Aircrafts/pmdg-aircraft-77w", .externalPath = kVendorFolder}};
    f.session.ShowActiveProfile();

    f.session.ForgetWhatCameFromAnotherProgram({});

    QCOMPARE(f.settings.stored.profiles.front().externalOrigins.size(), std::size_t{1});
    QCOMPARE(f.observer.started, 1);
}

QTEST_MAIN(SessionTest)

#include "tst_session.moc"
