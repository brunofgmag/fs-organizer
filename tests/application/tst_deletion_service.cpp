#include <QtTest/QtTest>

#include "application/DeletionService.h"
#include "domain/importing/ExternalSidecar.h"
#include "domain/model/CategoryMarker.h"
#include "domain/journal/OperationLog.h"
#include "domain/model/RecycleLimits.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class DeletionServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ASelectionTallerThanTheRecycleBinQuotaIsRefusedBeforeAnythingIsDeleted();
        static void AddonsThatFitOneByOneAreStillRefusedWhenTheirSumDoesNot();
        static void AVolumeThatNeverRecyclesIsNotOfferedTheRecycleBin();
        static void AnAddonNobodyMeasuredIsNeverCountedAsWeighingNothing();
        static void AnAddonNobodyMeasuredHasNoLengthToJudgeEither();
        static void TheWalkThatSummedTheBytesIsTheOnlyOneThePlanNeeds();
        static void AnAddonTheShellCannotReachIsRefusedInsteadOfDeletedInSilence();
        static void AnAddonEnabledInAProfileThatIsNotTheActiveOneIsFoundByThePlan();
        static void DeletingAnEnabledAddonRemovesEveryLinkBeforeTheFolder();
        static void AFailedUnlinkAbortsTheDeletionAndSaysWhichLinksWentAway();
        static void ACategoryInTheSelectionDeletesNoAddonAtAll();
        static void NothingIsDeletedWhileTheSimulatorIsRunning();
        static void TheJournalTellsTheTwoRoutesApartWithoutReadingText();
        static void TheBatchRunsToTheEndAndAnswersForEveryAddon();
        static void APermanentDeletionNeverAsksTheRecycleBinAnything();
        static void DeletingAManagedExternalAddonTakesTheRecordOfWhereItCameFromWithIt();
        static void TheRecycleBinRouteTakesTheRecordOfWhereItCameFromToo();
        static void AFailedDeletionLeavesTheRecordOfWhereItCameFromAlone();
        static void TheCategoryThatLosesItsLastAddonIsDeclaredAndTheLibraryRootIsNot();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;
    constexpr std::uintmax_t kGigabyte = 1024 * kMegabyte;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kOtherDestination = "F:/Sim2020/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kAircrafts = "D:/Library/Aircrafts";
    const std::filesystem::path kCrj = "D:/Library/Aircrafts/aerosoft-crj";
    const std::filesystem::path kAtr = "D:/Library/Aircrafts/hype-atr";
    const std::filesystem::path kCrjLink = "E:/Sim/Community/aerosoft-crj";
    const std::filesystem::path kCrjLinkElsewhere = "F:/Sim2020/Community/aerosoft-crj";
    const std::filesystem::path kOtherProgramsFolder = "C:/Addon Manager/Aircraft/aerosoft-crj";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

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
        InlineBackgroundRunner runner;
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        DeletionService service{filesystemProbe, files, sidecars, linking, classifier, processProbe, log, sizes};

        SimulatorProfile profile{.id = "msfs2024",
                                 .destinations = {kDestination},
                                 .defaultDestination = kDestination,
                                 .libraries = {Library{.id = "lib-1", .path = kLibrary}}};

        SimulatorProfile other{.id = "msfs2020",
                               .variant = SimulatorVariant::MSFS2020,
                               .destinations = {kOtherDestination},
                               .defaultDestination = kOtherDestination,
                               .libraries = {Library{.id = "lib-2", .path = kAircrafts}}};

        TreeNode tree;

        Fixture()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kOtherDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kCrj);
            fileSystem.AddDirectory(kAtr);
            fileSystem.AddFile(kCrj / "manifest.json", kMegabyte);
            fileSystem.AddFile(kAtr / "manifest.json", kMegabyte);

            fileSystem.SetRecycleBinQuota("D:", 10 * kGigabyte);

            TheLibraryHolds({AddonNode(kCrj), AddonNode(kAtr)});
        }

        void TheLibraryHolds(std::vector<TreeNode> addons)
        {
            TreeNode aircrafts;
            aircrafts.kind = TreeNodeKind::Category;
            aircrafts.path = kAircrafts;
            aircrafts.children = std::move(addons);

            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children = {std::move(aircrafts)};

            tree = std::move(library);
            catalog.SetTree(kLibrary, tree);
            catalog.SetTree(kAircrafts, tree.children.front());
        }

        [[nodiscard]] const TreeNode* Node(const std::filesystem::path& folder) const
        {
            for (const TreeNode* addon : AddonsUnder(tree))
            {
                if (addon->path == folder)
                {
                    return addon;
                }
            }

            return nullptr;
        }

        [[nodiscard]] const TreeNode* Category() const
        {
            return &tree.children.front();
        }

        void BuryAFileUnder(const std::filesystem::path& folder, const std::size_t entryLength)
        {
            const std::string name(entryLength - ComparablePath(folder).size() - 1, 'x');

            fileSystem.AddFile(folder / PathFromUtf8(name), kMegabyte);
        }

        void Measure(const std::vector<std::filesystem::path>& folders)
        {
            sizes.MeasureFolders(folders, sizes.NewCaller(), Freshness::MeasureAgain, {},
                                 [](const FolderSizeReport&) {});
        }

        [[nodiscard]] DeletionPlan PlanFor(const std::vector<const TreeNode*>& nodes) const
        {
            return service.Plan(profile, {profile, other}, nodes);
        }

        [[nodiscard]] std::vector<DeletionResult> Run(const std::vector<const TreeNode*>& nodes,
                                                      const DeletionRoute route) const
        {
            return service.Delete({profile, other}, PlanFor(nodes), route);
        }

        [[nodiscard]] std::size_t RecordsOf(const OperationKind kind) const
        {
            return static_cast<std::size_t>(std::ranges::count_if(journal.appended,
                                                                  [kind](const OperationRecord& record)
                                                                  {
                                                                      return record.kind == kind;
                                                                  }));
        }
    };
}

void DeletionServiceTest::ASelectionTallerThanTheRecycleBinQuotaIsRefusedBeforeAnythingIsDeleted()
{
    Fixture f;
    f.fileSystem.AddFile(kCrj / "big.bin", 20 * kGigabyte);
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin);

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheRecycleBinIsTooSmall);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QCOMPARE(f.journal.appended.size(), std::size_t{0});
}

void DeletionServiceTest::AddonsThatFitOneByOneAreStillRefusedWhenTheirSumDoesNot()
{
    Fixture f;
    f.fileSystem.AddFile(kCrj / "big.bin", 6 * kGigabyte);
    f.fileSystem.AddFile(kAtr / "big.bin", 6 * kGigabyte);
    f.Measure({kCrj, kAtr});

    QVERIFY(TheRecycleBinCanTake(f.PlanFor({f.Node(kCrj)})));
    QVERIFY(TheRecycleBinCanTake(f.PlanFor({f.Node(kAtr)})));
    QVERIFY(!TheRecycleBinCanTake(f.PlanFor({f.Node(kCrj), f.Node(kAtr)})));
}

void DeletionServiceTest::AVolumeThatNeverRecyclesIsNotOfferedTheRecycleBin()
{
    Fixture f;
    f.fileSystem.MakeTheVolumeDeletePermanently("D:");
    f.Measure({kCrj});

    QVERIFY(!TheRecycleBinCanTake(f.PlanFor({f.Node(kCrj)})));

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin);

    QCOMPARE(results.front().result, FileResult::TheRecycleBinIsTooSmall);
    QVERIFY(f.fileSystem.Exists(kCrj));
}

void DeletionServiceTest::AnAddonNobodyMeasuredIsNeverCountedAsWeighingNothing()
{
    Fixture f;

    const DeletionPlan plan = f.PlanFor({f.Node(kCrj)});

    QCOMPARE(plan.addons.size(), std::size_t{1});
    QVERIFY(!plan.addons.front().bytes.has_value());
    QVERIFY(!TheRecycleBinCanTake(plan));

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin);

    QCOMPARE(results.front().result, FileResult::TheAddonWasNeverMeasured);
    QVERIFY(f.fileSystem.Exists(kCrj));
}

void DeletionServiceTest::AnAddonNobodyMeasuredHasNoLengthToJudgeEither()
{
    Fixture f;
    f.BuryAFileUnder(kCrj, kTheRecycleBinStopsAt);

    const DeletionPlan plan = f.PlanFor({f.Node(kCrj)});

    QVERIFY(!plan.addons.front().longestEntry.has_value());
    QVERIFY(!TheRecycleBinCanTake(plan));
}

void DeletionServiceTest::TheWalkThatSummedTheBytesIsTheOnlyOneThePlanNeeds()
{
    Fixture f;
    f.Measure({kCrj});

    const DeletionPlan plan = f.PlanFor({f.Node(kCrj)});

    QVERIFY(plan.addons.front().bytes.has_value());
    QVERIFY(plan.addons.front().longestEntry.has_value());
    QCOMPARE(f.filesystemProbe.TimesWalked(kCrj), std::size_t{1});
}

void DeletionServiceTest::AnAddonTheShellCannotReachIsRefusedInsteadOfDeletedInSilence()
{
    Fixture f;
    f.BuryAFileUnder(kCrj, kTheRecycleBinStopsAt);
    f.Measure({kCrj});

    const DeletionPlan plan = f.PlanFor({f.Node(kCrj)});

    QCOMPARE(plan.addons.front().longestEntry, std::optional<std::size_t>{kTheRecycleBinStopsAt});
    QVERIFY(!TheRecycleBinCanTake(plan));

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin);

    QCOMPARE(results.front().result, FileResult::TheRecycleBinCannotReachIt);
    QVERIFY(f.fileSystem.Exists(kCrj));
}

void DeletionServiceTest::AnAddonEnabledInAProfileThatIsNotTheActiveOneIsFoundByThePlan()
{
    Fixture f;
    f.fileSystem.AddLink(kCrjLinkElsewhere, kCrj);

    const DeletionPlan plan = f.PlanFor({f.Node(kCrj)});

    QCOMPARE(plan.addons.front().enabled.size(), std::size_t{1});
    QCOMPARE(plan.addons.front().enabled.front().profileId, std::string{"msfs2020"});
    QCOMPARE(plan.addons.front().enabled.front().linkPath, kCrjLinkElsewhere);
}

void DeletionServiceTest::DeletingAnEnabledAddonRemovesEveryLinkBeforeTheFolder()
{
    Fixture f;
    f.fileSystem.AddLink(kCrjLink, kCrj);
    f.fileSystem.AddLink(kCrjLinkElsewhere, kCrj);
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.front().linksRemoved.size(), std::size_t{2});
    QVERIFY(!f.fileSystem.Exists(kCrjLink));
    QVERIFY(!f.fileSystem.Exists(kCrjLinkElsewhere));
    QVERIFY(!f.fileSystem.Exists(kCrj));
}

void DeletionServiceTest::AFailedUnlinkAbortsTheDeletionAndSaysWhichLinksWentAway()
{
    Fixture f;
    f.fileSystem.AddLink(kCrjLink, kCrj);
    f.fileSystem.AddLink(kCrjLinkElsewhere, kCrj);
    f.linkService.MakeTheRemovalFailFor(kCrjLinkElsewhere);
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::CouldNotRemoveTheLink);
    QCOMPARE(results.front().linksRemoved.size(), std::size_t{1});
    QCOMPARE(results.front().linksRemoved.front(), kCrjLink);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QCOMPARE(f.RecordsOf(OperationKind::DeleteFromLibrary), std::size_t{0});
}

void DeletionServiceTest::ACategoryInTheSelectionDeletesNoAddonAtAll()
{
    Fixture f;
    f.Measure({kCrj, kAtr});

    const DeletionPlan plan = f.PlanFor({f.Category()});

    QCOMPARE(plan.addons.size(), std::size_t{0});
    QCOMPARE(plan.nodesThatAreNotAddons, std::size_t{1});

    const std::vector<DeletionResult> results = f.Run({f.Category()}, DeletionRoute::Permanently);

    QCOMPARE(results.size(), std::size_t{0});
    QVERIFY(f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(kAtr));
    QVERIFY(f.fileSystem.Exists(kAircrafts));
}

void DeletionServiceTest::NothingIsDeletedWhileTheSimulatorIsRunning()
{
    Fixture f;
    f.Measure({kCrj});
    f.processProbe.ReportTheSimulatorAsRunning();

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::TheSimulatorIsRunning);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QCOMPARE(f.journal.appended.size(), std::size_t{0});
}

void DeletionServiceTest::TheJournalTellsTheTwoRoutesApartWithoutReadingText()
{
    Fixture f;
    f.Measure({kCrj, kAtr});

    static_cast<void>(f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin));
    static_cast<void>(f.Run({f.Node(kAtr)}, DeletionRoute::Permanently));

    QCOMPARE(f.RecordsOf(OperationKind::RecycleFromLibrary), std::size_t{1});
    QCOMPARE(f.RecordsOf(OperationKind::DeleteFromLibrary), std::size_t{1});
    QCOMPARE(f.journal.appended.front().addonId, IdentityOf(f.profile, kCrj));
    QVERIFY(f.fileSystem.WasRecycled(kCrj));
    QVERIFY(!f.fileSystem.WasRecycled(kAtr));
}

void DeletionServiceTest::TheBatchRunsToTheEndAndAnswersForEveryAddon()
{
    Fixture f;
    f.Measure({kCrj, kAtr});
    f.files.MakeTheRemovalFailFor(kCrj);

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj), f.Node(kAtr)}, DeletionRoute::Permanently);

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::CouldNotDelete);
    QCOMPARE(results.back().result, FileResult::Completed);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QVERIFY(!f.fileSystem.Exists(kAtr));
}

void DeletionServiceTest::APermanentDeletionNeverAsksTheRecycleBinAnything()
{
    Fixture f;
    f.fileSystem.MakeTheVolumeDeletePermanently("D:");
    f.BuryAFileUnder(kCrj, 2 * kTheRecycleBinStopsAt);

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kCrj));
}

void DeletionServiceTest::DeletingAManagedExternalAddonTakesTheRecordOfWhereItCameFromWithIt()
{
    Fixture f;
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kCrj), TextOfTheExternalOrigin(kOtherProgramsFolder));
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kCrj));
    QVERIFY(!f.fileSystem.Exists(ExternalSidecarPathFor(kCrj)));
}

void DeletionServiceTest::TheRecycleBinRouteTakesTheRecordOfWhereItCameFromToo()
{
    Fixture f;
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kCrj), TextOfTheExternalOrigin(kOtherProgramsFolder));
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::RecycleBin);

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY(!f.fileSystem.Exists(kCrj));
    QVERIFY(!f.fileSystem.Exists(ExternalSidecarPathFor(kCrj)));
}

void DeletionServiceTest::AFailedDeletionLeavesTheRecordOfWhereItCameFromAlone()
{
    Fixture f;
    f.fileSystem.AddFileWithContents(ExternalSidecarPathFor(kCrj), TextOfTheExternalOrigin(kOtherProgramsFolder));
    f.files.MakeTheRemovalFailFor(kCrj);
    f.Measure({kCrj});

    const std::vector<DeletionResult> results = f.Run({f.Node(kCrj)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::CouldNotDelete);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(ExternalSidecarPathFor(kCrj)));
}

void DeletionServiceTest::TheCategoryThatLosesItsLastAddonIsDeclaredAndTheLibraryRootIsNot()
{
    const std::filesystem::path airbus = kAircrafts / "Airbus";
    const std::filesystem::path fenix = airbus / "fenix-a320";

    Fixture f;
    f.fileSystem.AddDirectory(airbus);
    f.fileSystem.AddDirectory(fenix);
    f.fileSystem.AddFile(fenix / "manifest.json", kMegabyte);

    TreeNode category;
    category.kind = TreeNodeKind::Category;
    category.path = airbus;
    category.children = {AddonNode(fenix)};
    f.TheLibraryHolds({std::move(category)});

    f.Measure({fenix});

    const std::vector<DeletionResult> results = f.Run({f.Node(fenix)}, DeletionRoute::Permanently);

    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY2(f.fileSystem.Exists(CategoryMarkerPathIn(airbus)),
             "the emptied category was left to read as an addon on the next scan");
    QVERIFY(!f.fileSystem.Exists(CategoryMarkerPathIn(kLibrary)));
}

QTEST_MAIN(DeletionServiceTest)

#include "tst_deletion_service.moc"
