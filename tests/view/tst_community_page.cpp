#include <algorithm>

#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "application/LibraryOrganizer.h"
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
#include "tests/support/InstalledCatalogue.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/community/CommunityPage.h"
#include "view/community/ImportDialog.h"
#include "view/community/RepairDialog.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/FoldersOutsideNotice.h"
#include "view/shell/TriageStrip.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/SessionNotifier.h"
#include "tests/support/PageFloor.h"
#include "tests/support/PhysicalRows.h"

namespace
{
    class CommunityPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void TheTriageStripLeavesTheRowBelowItItsOwnTopMarginAndNothingMore();
        static void ThePanelStartsLevelWithTheTable();
        static void TheColumnsFitTheViewportWithThePanelOpenAt1140Pixels();
        static void ThePanelTitleStripEndsWhereTheColumnHeaderEnds();
        static void ThePanelTitleStripLineLandsOnTheRowsOfTheColumnHeaderLine();
        static void ThePanelLeftLineRunsFromTheTitleStripToTheBottom();
        static void TheScrollBarCapGoesWhenTheScrollBarDoes();
        static void TheBarsSpanTheWholePageOverThePanel();
        static void TheNoticeAppearsWithFoldersOutsideTheLibraryAndGoesWithNone();
        static void TheImportButtonOfTheNoticeAsksForTheImport();
        static void TheImportButtonOfTheNoticeIsOfNormalSize();
        static void TheNoticeSpeaksPortugueseWhenTheCatalogueIsLoaded();
        static void TheNoticeSitsBetweenTheActionsAndTheSearchWhichEndsTheLine_data();
        static void TheNoticeSitsBetweenTheActionsAndTheSearchWhichEndsTheLine();
        static void WithFourDigitsOutsideThePageFitsTheNarrowestWindow_data();
        static void WithFourDigitsOutsideThePageFitsTheNarrowestWindow();
        static void TheTriageConflictActionLeavesEveryConflictedRowSelected();
        static void TheTriageImportActionLeavesEveryUnmanagedFolderSelected();
        static void TheImportButtonCountsTheWholeSelectionAndNotTheFirstRow();
        static void AMixedSelectionOffersBothActionsEachWithItsOwnCount();
        static void OnlyTheActionThatUnblocksCarriesTheAccent();
        static void NothingConflictedMeansNoResolveButtonAtAll();
        static void ARescanThatEmptiesTheTableAlsoEmptiesThePanel();
        static void AFilterThatRanOutHandsTheTableBackInsteadOfLeavingItBlank();
        static void TheTabReadsTheDestinationsAgainInsteadOfRedrawingThePortrait();
        static void AdoptingAFolderAnotherProgramOwnsSaysTheUpdateCanBreakIt();
        static void AFolderNoProgramOwnsGetsNoSuchWarning();
        static void TypingInTheSearchFieldKeepsOnlyTheRowsThatHoldTheText();
        static void TheSearchFieldWorksTogetherWithTheChipThatIsChosen();
        static void AHiddenPageDoesNotRebuildItsTableAndShowsTheNewRowsTheMomentItIsShown();
        static void ShowingThePageRebuildsNothingWhenNothingChangedAndStillSaysItsAside();
        static void AHiddenAgainPageStopsRebuildingItsTable();
        static void RepairingRightAfterTheTabIsSelectedPlansFromTheCurrentEntries();
        static void ARepairWhereEveryLinkHadChangedSaysNothingChangedAndRefreshesTheList();
        static void ARepairWhereOneLinkHadChangedSaysHowManyWereRepairedAndHowManyWereLeftAlone();
        static void ARepairWithAFailureAndADriftedLinkAddsTheDriftToTheStatusLine();
        static void ARepairWhereNothingHadChangedSaysWhatItAlwaysSaid();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kShared = "pmdg-aircraft-77w";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = {AddonNode(std::filesystem::path(kLibrary) / "Aircrafts" / kShared)};

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(std::filesystem::path(kLibrary) / "Aircrafts" / kShared);
            catalog.SetTree(kLibrary, LibraryTree());

            fileSystem.AddDirectory(std::filesystem::path(kCommunity) / kShared);
            fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-one");
            fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-two");

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
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
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService importService{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                                    linking, log,          LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        CommunityModel model;
        CommunityViewModel viewModel{service, session, notifier, model, sizes, runner};
        ImportViewModel importViewModel{importService, service, processProbe, session, runner};
    };

    int ConflictedAmong(const CommunityPage& page, const CommunityModel& model)
    {
        const auto* table = page.findChild<QTableView*>();
        int conflicted = 0;

        for (const QModelIndex& position : table->selectionModel()->selectedRows())
        {
            const auto filter = qobject_cast<const QAbstractProxyModel*>(table->model());
            conflicted += model.data(filter->mapToSource(position), CommunityModel::ConflictRole).toBool() ? 1 : 0;
        }

        return conflicted;
    }

    int RowOf(const CommunityPage& page, const QString& name)
    {
        const auto* table = page.findChild<QTableView*>();
        const QAbstractItemModel* shown = table->model();

        for (int row = 0; row < shown->rowCount(); ++row)
        {
            if (shown->data(shown->index(row, 0), Qt::DisplayRole).toString() == name)
            {
                return row;
            }
        }

        return -1;
    }
}

void CommunityPageTest::TheTriageConflictActionLeavesEveryConflictedRowSelected()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterByConflicted();
    page.SelectEverythingShown();

    const auto* table = page.findChild<QTableView*>();
    const int selected = static_cast<int>(table->selectionModel()->selectedRows().size());

    QVERIFY(selected > 0);
    QCOMPARE(ConflictedAmong(page, f.model), selected);
}

void CommunityPageTest::TheTriageImportActionLeavesEveryUnmanagedFolderSelected()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* table = page.findChild<QTableView*>();

    QVERIFY(!table->selectionModel()->selectedRows().isEmpty());
}

void CommunityPageTest::TheImportButtonCountsTheWholeSelectionAndNotTheFirstRow()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* table = page.findChild<QTableView*>();
    const auto selected = static_cast<int>(table->selectionModel()->selectedRows().size());

    QCOMPARE(selected, 3);
    QCOMPARE(ConflictedAmong(page, f.model), 1);

    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));

    QVERIFY(importChosen != nullptr);
    QVERIFY(importChosen->isEnabled());
    QVERIFY(importChosen->text().contains(QStringLiteral("2")));
    QVERIFY(!importChosen->text().contains(QStringLiteral("3")));
}

void CommunityPageTest::AMixedSelectionOffersBothActionsEachWithItsOwnCount()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));
    const auto* resolveChosen = page.findChild<QPushButton*>(QStringLiteral("ResolveChosen"));

    QVERIFY(resolveChosen != nullptr);
    QVERIFY(resolveChosen->isVisibleTo(&page));
    QVERIFY(importChosen->isEnabled());

    QVERIFY(importChosen->text().contains(QStringLiteral("2")));
    QCOMPARE(resolveChosen->text(), QStringLiteral("Resolve the conflict…"));
}

void CommunityPageTest::OnlyTheActionThatUnblocksCarriesTheAccent()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));
    const auto* resolveChosen = page.findChild<QPushButton*>(QStringLiteral("ResolveChosen"));

    QCOMPARE(resolveChosen->property("role").toString(), QStringLiteral("primary"));
    QCOMPARE(importChosen->property("role").toString(), QStringLiteral("secondary"));
}

void CommunityPageTest::NothingConflictedMeansNoResolveButtonAtAll()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    auto* table = page.findChild<QTableView*>();
    const int loose = RowOf(page, QStringLiteral("loose-one"));

    QVERIFY(loose >= 0);
    table->selectRow(loose);
    QCOMPARE(table->selectionModel()->selectedRows().size(), 1);

    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));
    const auto* resolveChosen = page.findChild<QPushButton*>(QStringLiteral("ResolveChosen"));

    QVERIFY(!resolveChosen->isVisibleTo(&page));
    QVERIFY(importChosen->isEnabled());
    QCOMPARE(importChosen->property("role").toString(), QStringLiteral("primary"));
}

void CommunityPageTest::ARescanThatEmptiesTheTableAlsoEmptiesThePanel()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.show();
    f.viewModel.Show();

    page.SelectEverythingShown();

    const auto* panel = page.findChild<ContextPanel*>();
    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));
    const auto* resolveChosen = page.findChild<QPushButton*>(QStringLiteral("ResolveChosen"));

    QVERIFY(panel != nullptr);
    QVERIFY(panel->isVisibleTo(&page));
    QVERIFY(resolveChosen->isVisibleTo(&page));

    QVERIFY(f.fileSystem.RemoveTree(std::filesystem::path(kCommunity) / kShared));
    QVERIFY(f.fileSystem.RemoveTree(std::filesystem::path(kCommunity) / "loose-one"));
    QVERIFY(f.fileSystem.RemoveTree(std::filesystem::path(kCommunity) / "loose-two"));
    f.session.ShowActiveProfile();

    QCOMPARE(f.model.rowCount({}), 0);
    QVERIFY(!panel->isVisibleTo(&page));
    QVERIFY(!importChosen->isEnabled());
    QVERIFY(!resolveChosen->isVisibleTo(&page));
}

void CommunityPageTest::AFilterThatRanOutHandsTheTableBackInsteadOfLeavingItBlank()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.show();
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    QVERIFY(page.findChild<QTableView*>()->model()->rowCount() > 0);

    QVERIFY(f.fileSystem.RemoveTree(std::filesystem::path(kCommunity) / "loose-one"));
    QVERIFY(f.fileSystem.RemoveTree(std::filesystem::path(kCommunity) / "loose-two"));
    f.viewModel.ReadTheDestinationsAgain();

    QVERIFY2(page.findChild<QTableView*>()->model()->rowCount() > 0,
             "the filter emptied, and a blank table with a chip reading zero says nothing about why");
}

void CommunityPageTest::TheTabReadsTheDestinationsAgainInsteadOfRedrawingThePortrait()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.show();
    f.viewModel.Show();

    const int before = f.model.rowCount({});

    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-three");

    f.viewModel.Show();
    QCOMPARE(f.model.rowCount({}), before);

    auto* reread = page.findChild<QPushButton*>(QStringLiteral("ReadDestinationsAgain"));
    QVERIFY(reread != nullptr);
    reread->click();

    QCOMPARE(f.model.rowCount({}), before + 1);
}

namespace
{
    [[nodiscard]] QStringList TheParagraphsOf(const ImportDialog& dialog)
    {
        QStringList said;

        for (const QLabel* line : dialog.findChildren<QLabel*>())
        {
            said.append(line->text());
        }

        return said;
    }

    [[nodiscard]] bool AnyOfThemWarnsAboutTheOtherProgram(const QStringList& said)
    {
        return std::ranges::any_of(said,
                                   [](const QString& line)
                                   {
                                       return line.contains(QStringLiteral("will not know about the link"));
                                   });
    }
}

void CommunityPageTest::AdoptingAFolderAnotherProgramOwnsSaysTheUpdateCanBreakIt()
{
    SimulatorProfile profile;
    profile.libraries.push_back({.path = "D:/Library", .label = "Library"});

    ImportRequest owned;
    owned.source = "C:/Community/fsdreamteam-gsx-pro";
    owned.externalSource = "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    const std::vector<TreeNode> libraries;
    ImportDialog dialog({owned}, libraries, profile, 1024);

    QVERIFY2(AnyOfThemWarnsAboutTheOtherProgram(TheParagraphsOf(dialog)),
             "adopting a folder another program installed has to say the next update can break it");
}

void CommunityPageTest::AFolderNoProgramOwnsGetsNoSuchWarning()
{
    SimulatorProfile profile;
    profile.libraries.push_back({.path = "D:/Library", .label = "Library"});

    ImportRequest loose;
    loose.source = "C:/Community/some-loose-addon";

    const std::vector<TreeNode> libraries;
    ImportDialog dialog({loose}, libraries, profile, 1024);

    QVERIFY2(!AnyOfThemWarnsAboutTheOtherProgram(TheParagraphsOf(dialog)),
             "a folder no other program installed cannot be broken by that program, so the warning would be a lie");
}

void CommunityPageTest::TypingInTheSearchFieldKeepsOnlyTheRowsThatHoldTheText()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);

    auto* search = page.findChild<QLineEdit*>();
    const QAbstractItemModel* shown = page.findChild<QTableView*>()->model();

    QVERIFY(search != nullptr);
    QCOMPARE(shown->rowCount(), 3);

    search->setText(QStringLiteral("LOOSE"));
    QCOMPARE(shown->rowCount(), 2);
    QVERIFY(RowOf(page, QStringLiteral("loose-one")) >= 0);
    QVERIFY(RowOf(page, QStringLiteral("loose-two")) >= 0);
    QCOMPARE(RowOf(page, QString::fromUtf8(kShared)), -1);

    search->setText(QStringLiteral("loose-one"));
    QCOMPARE(shown->rowCount(), 1);

    search->setText(QStringLiteral("community"));
    QCOMPARE(shown->rowCount(), 3);

    search->setText(QStringLiteral("in conflict"));
    QCOMPARE(shown->rowCount(), 1);
    QVERIFY(RowOf(page, QString::fromUtf8(kShared)) >= 0);

    search->setText(QStringLiteral("not in a library"));
    QCOMPARE(shown->rowCount(), 3);

    search->setText(QStringLiteral("nothing is called this"));
    QCOMPARE(shown->rowCount(), 0);

    search->clear();
    QCOMPARE(shown->rowCount(), 3);
}

void CommunityPageTest::TheSearchFieldWorksTogetherWithTheChipThatIsChosen()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);

    auto* search = page.findChild<QLineEdit*>();
    const QAbstractItemModel* shown = page.findChild<QTableView*>()->model();

    QVERIFY(search != nullptr);

    page.FilterByConflicted();
    QCOMPARE(shown->rowCount(), 1);

    search->setText(QStringLiteral("loose"));
    QCOMPARE(shown->rowCount(), 0);

    search->setText(QString::fromUtf8(kShared));
    QCOMPARE(shown->rowCount(), 1);

    page.FilterBy(EntryClassification::Unmanaged);
    QCOMPARE(shown->rowCount(), 1);

    search->setText(QStringLiteral("loose"));
    QCOMPARE(shown->rowCount(), 2);

    page.FilterBy(EntryClassification::Broken);
    QCOMPARE(shown->rowCount(), 0);
}

void CommunityPageTest::AHiddenPageDoesNotRebuildItsTableAndShowsTheNewRowsTheMomentItIsShown()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);

    const int before = f.model.rowCount({});
    const QSignalSpy resets(&f.model, &QAbstractItemModel::modelReset);

    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-three");
    f.session.RefreshEntries();

    QCOMPARE(resets.size(), 0);
    QCOMPARE(f.model.rowCount({}), before);

    page.show();

    QCOMPARE(resets.size(), 1);
    QCOMPARE(f.model.rowCount({}), before + 1);
    QCOMPARE(page.findChild<QTableView*>()->model()->rowCount(), before + 1);
}

void CommunityPageTest::ShowingThePageRebuildsNothingWhenNothingChangedAndStillSaysItsAside()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);

    const QSignalSpy resets(&f.model, &QAbstractItemModel::modelReset);
    const QSignalSpy aside(&page, &CommunityPage::AsideChanged);

    page.show();

    QCOMPARE(resets.size(), 0);
    QCOMPARE(aside.size(), 1);
}

void CommunityPageTest::AHiddenAgainPageStopsRebuildingItsTable()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.show();

    const int before = f.model.rowCount({});
    const QSignalSpy resets(&f.model, &QAbstractItemModel::modelReset);

    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-three");
    f.session.RefreshEntries();

    QCOMPARE(resets.size(), 1);
    QCOMPARE(f.model.rowCount({}), before + 1);

    page.hide();
    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "loose-four");
    f.session.RefreshEntries();

    QCOMPARE(resets.size(), 1);
    QCOMPARE(f.model.rowCount({}), before + 1);

    page.show();

    QCOMPARE(resets.size(), 2);
    QCOMPARE(f.model.rowCount({}), before + 2);
}

void CommunityPageTest::RepairingRightAfterTheTabIsSelectedPlansFromTheCurrentEntries()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);

    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "gone", "D:/Removed/gone");
    f.session.RefreshEntries();

    page.show();

    bool theDialogOpened = false;
    QTimer::singleShot(0, &page,
                       [&theDialogOpened]
                       {
                           if (auto* dialog = qobject_cast<RepairDialog*>(QApplication::activeModalWidget()))
                           {
                               theDialogOpened = true;
                               dialog->reject();
                           }
                       });

    page.StartRepair();

    QVERIFY2(theDialogOpened, "the broken link read while the page was hidden has to reach the repair plan");
    QVERIFY(RowOf(page, QStringLiteral("gone")) >= 0);
}

namespace
{
    const std::filesystem::path kGone = std::filesystem::path(kCommunity) / "gone";
    const std::filesystem::path kLost = std::filesystem::path(kCommunity) / "lost";
    const std::filesystem::path kKept = std::filesystem::path(kCommunity) / "kept";

    void LeaveDeadLinksAt(Fixture& f, const std::vector<std::filesystem::path>& links)
    {
        for (const std::filesystem::path& link : links)
        {
            f.fileSystem.AddLink(link, std::filesystem::path("D:/Removed") / link.filename());
        }

        f.session.RefreshEntries();
    }

    void BringTheTargetBack(Fixture& f, const std::filesystem::path& link)
    {
        f.fileSystem.AddDirectory(std::filesystem::path("D:/Removed") / link.filename());
    }

    std::vector<RepairRequest> RemovalsOfWhatTheListShows(const Fixture& f)
    {
        std::vector<RepairRequest> requests;
        for (const RepairCandidate& candidate : f.viewModel.PlanRepairs())
        {
            requests.push_back({.candidate = candidate, .action = RepairAction::RemoveDeadNode});
        }

        return requests;
    }

    QString TheStatusAfterRepairing(Fixture& f, const CommunityPage& page, const std::vector<RepairRequest>& requests)
    {
        const QSignalSpy status(&page, &CommunityPage::StatusChanged);

        QTimer::singleShot(0, &page,
                           []
                           {
                               if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                               {
                                   box->accept();
                               }
                           });

        f.viewModel.Repair(requests);

        return status.isEmpty() ? QString() : status.back().front().toString();
    }
}

void CommunityPageTest::ARepairWhereEveryLinkHadChangedSaysNothingChangedAndRefreshesTheList()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    LeaveDeadLinksAt(f, {kGone});

    const std::vector<RepairRequest> requests = RemovalsOfWhatTheListShows(f);
    BringTheTargetBack(f, kGone);

    QCOMPARE(TheStatusAfterRepairing(f, page, requests),
             QStringLiteral("Nothing changed: 1 link had changed on the disk. The list has been refreshed."));
    QVERIFY(f.fileSystem.IsLink(kGone));
}

void CommunityPageTest::ARepairWhereOneLinkHadChangedSaysHowManyWereRepairedAndHowManyWereLeftAlone()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    LeaveDeadLinksAt(f, {kGone, kLost});

    const std::vector<RepairRequest> requests = RemovalsOfWhatTheListShows(f);
    BringTheTargetBack(f, kLost);

    QCOMPARE(TheStatusAfterRepairing(f, page, requests), QStringLiteral("1 repaired · 1 had changed on the disk"));
    QVERIFY(!f.fileSystem.Exists(kGone));
    QVERIFY(f.fileSystem.IsLink(kLost));
}

void CommunityPageTest::ARepairWithAFailureAndADriftedLinkAddsTheDriftToTheStatusLine()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    LeaveDeadLinksAt(f, {kGone, kLost, kKept});

    const std::vector<RepairRequest> requests = RemovalsOfWhatTheListShows(f);
    BringTheTargetBack(f, kLost);
    f.linkService.MakeTheRemovalFailFor(kGone);

    QCOMPARE(TheStatusAfterRepairing(f, page, requests),
             QStringLiteral("1 repaired · 1 failed · 1 had changed on the disk"));
}

void CommunityPageTest::ARepairWhereNothingHadChangedSaysWhatItAlwaysSaid()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);
    LeaveDeadLinksAt(f, {kGone});

    const std::vector<RepairRequest> requests = RemovalsOfWhatTheListShows(f);

    QCOMPARE(TheStatusAfterRepairing(f, page, requests), QStringLiteral("1 link repaired."));
    QVERIFY(!f.fileSystem.Exists(kGone));
}

void CommunityPageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);

    ItFitsTheNarrowestWindow(page, "The destinations page");
}

namespace
{
    int TopWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{}).y();
    }

    int BottomWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{0, widget.height()}).y();
    }

    int RightEdgeWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{widget.width(), 0}).x();
    }

    void OpenWithARowSelected(Fixture& f, CommunityPage& page)
    {
        page.resize(kWidestAPageMayBe, 600);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        f.viewModel.Show();

        const int loose = RowOf(page, QStringLiteral("loose-one"));

        QVERIFY(loose >= 0);
        page.findChild<QTableView*>()->selectRow(loose);
        QCoreApplication::processEvents();
    }
}

void CommunityPageTest::ThePanelStartsLevelWithTheTable()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    OpenWithARowSelected(f, page);

    const auto* panel = page.findChild<ContextPanel*>();

    QVERIFY(panel != nullptr);
    QVERIFY(panel->isVisible());
    QCOMPARE(TopWithin(page, *panel), TopWithin(page, *page.findChild<QTableView*>()));
}

void CommunityPageTest::TheColumnsFitTheViewportWithThePanelOpenAt1140Pixels()
{
    constexpr int kTheWindowOfTheDemo = 1140;

    ApplyModernistTheme(*qApp);

    Fixture f;
    f.fileSystem.AddDirectory(std::filesystem::path(kCommunity) / "paperwing-livery-737-800-sunfield-airways-2");
    for (int filler = 0; filler < 40; ++filler)
    {
        f.fileSystem.AddDirectory(std::filesystem::path(kCommunity)
                                  / QStringLiteral("lanternfish-%1").arg(filler).toStdString());
    }
    f.session.RefreshEntries();

    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.resize(kTheWindowOfTheDemo, 700);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    f.viewModel.Show();

    const int loose = RowOf(page, QStringLiteral("loose-one"));

    QVERIFY(loose >= 0);

    auto* table = page.findChild<QTableView*>();
    table->selectRow(loose);
    QCoreApplication::processEvents();

    const QHeaderView* header = table->horizontalHeader();
    QStringList widths;
    for (int column = 0; column < header->count(); ++column)
    {
        widths << QString::number(header->sectionSize(column));
    }

    const QString said = QStringLiteral("the columns add up to %1 px in a viewport of %2 px: %3")
                             .arg(header->length())
                             .arg(table->viewport()->width())
                             .arg(widths.join(QLatin1Char('+')));

    QVERIFY2(header->length() <= table->viewport()->width(), qPrintable(said));
    QVERIFY(!table->horizontalScrollBar()->isVisible());
}

void CommunityPageTest::ThePanelTitleStripEndsWhereTheColumnHeaderEnds()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    OpenWithARowSelected(f, page);

    const auto* strip = page.findChild<ContextPanel*>()->findChild<QWidget*>(QStringLiteral("PanelHeader"));

    QVERIFY(strip != nullptr);
    QVERIFY(strip->isVisible());
    QCOMPARE(BottomWithin(page, *strip), BottomWithin(page, *page.findChild<QTableView*>()->horizontalHeader()));
}

void CommunityPageTest::ThePanelTitleStripLineLandsOnTheRowsOfTheColumnHeaderLine()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    OpenWithARowSelected(f, page);

    const auto* strip = page.findChild<ContextPanel*>()->findChild<QWidget*>(QStringLiteral("PanelHeader"));

    auto* header = page.findChild<QTableView*>()->horizontalHeader();

    QVERIFY(strip != nullptr);
    LetTheScrollBarShow(page, *page.findChild<QTableView*>());
    QVERIFY(page.findChild<QTableView*>()->verticalScrollBar()->isVisible());

    const QWidget* cap = ScrollBarCapOf(*page.findChild<QTableView*>());

    QVERIFY(cap != nullptr);
    QVERIFY(cap->isVisible());
    MakeTheColumnHeaderOnePixelShorter(*header);
    TheThreeRulesLandOnTheSamePhysicalRows(page, *strip, *header, *cap);
}

void CommunityPageTest::ThePanelLeftLineRunsFromTheTitleStripToTheBottom()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    OpenWithARowSelected(f, page);

    const auto* panel = page.findChild<ContextPanel*>();
    const auto* strip = panel->findChild<QWidget*>(QStringLiteral("PanelHeader"));
    const auto* body = panel->findChild<QWidget*>(QStringLiteral("PanelBody"));

    QVERIFY(strip != nullptr);
    QVERIFY(body != nullptr);
    LetTheScrollBarShow(page, *page.findChild<QTableView*>());
    QVERIFY(page.findChild<QTableView*>()->verticalScrollBar()->isVisible());
    TheLeftRuleRunsTheWholeHeightOfThePanel(page, *strip, *body);
}

void CommunityPageTest::TheScrollBarCapGoesWhenTheScrollBarDoes()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    OpenWithARowSelected(f, page);
    LetTheScrollBarGo(page);

    const QWidget* cap = ScrollBarCapOf(*page.findChild<QTableView*>());

    QVERIFY(cap != nullptr);
    QVERIFY(!page.findChild<QTableView*>()->verticalScrollBar()->isVisible());
    QVERIFY(!cap->isVisible());
}

void CommunityPageTest::TheBarsSpanTheWholePageOverThePanel()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    OpenWithARowSelected(f, page);

    const auto* toolbar = page.findChild<QWidget*>(QStringLiteral("PageToolbar"));

    QVERIFY(page.findChild<ContextPanel*>()->isVisible());
    QVERIFY(toolbar != nullptr);
    QCOMPARE(RightEdgeWithin(page, *toolbar), page.width());
}

namespace
{
    constexpr std::size_t kFourDigits = 1234;
    constexpr int kDesignedGapBetweenTheNoticeAndTheSearch = 16;

    QString Said(const std::size_t folders)
    {
        return QCoreApplication::translate("FoldersOutsideNotice", "%n folder outside the library", nullptr,
                                           static_cast<int>(folders));
    }

    FoldersOutsideNotice* NoticeOf(const CommunityPage& page)
    {
        return page.findChild<FoldersOutsideNotice*>();
    }

    QRect PlaceIn(const QWidget& bar, const QWidget& widget)
    {
        return {widget.mapTo(&bar, QPoint{}), widget.size()};
    }

    int Right(const QRect& place)
    {
        return place.x() + place.width();
    }

    int CenterOf(const QRect& place)
    {
        return place.y() + place.height() / 2;
    }
}

void CommunityPageTest::TheNoticeAppearsWithFoldersOutsideTheLibraryAndGoesWithNone()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    page.resize(kWidestAPageMayBe, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    const FoldersOutsideNotice* notice = NoticeOf(page);
    QVERIFY(notice != nullptr);
    QVERIFY(!notice->isVisibleTo(&page));

    page.ShowFoldersOutside(8);

    QVERIFY(notice->isVisibleTo(&page));
    QCOMPARE(notice->findChild<QLabel*>(QStringLiteral("TriageQuiet"))->text(), Said(8));

    page.ShowFoldersOutside(0);

    QVERIFY(!notice->isVisibleTo(&page));
}

void CommunityPageTest::TheImportButtonOfTheNoticeAsksForTheImport()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    page.resize(kWidestAPageMayBe, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    page.ShowFoldersOutside(8);

    const QSignalSpy asked(&page, &CommunityPage::ImportRequested);
    auto* import = NoticeOf(page)->findChild<QPushButton*>();

    QVERIFY(import != nullptr);
    QVERIFY(import->isVisibleTo(&page));

    import->click();

    QCOMPARE(asked.count(), 1);
}

void CommunityPageTest::TheImportButtonOfTheNoticeIsOfNormalSize()
{
    Fixture f;
    const CommunityPage page(f.viewModel, f.importViewModel, f.model);

    const auto* import = NoticeOf(page)->findChild<QPushButton*>();

    QVERIFY(import != nullptr);
    QCOMPARE(import->property("scale"), QVariant{});
}

void CommunityPageTest::TheNoticeSpeaksPortugueseWhenTheCatalogueIsLoaded()
{
    QTranslator catalogue;
    QVERIFY2(LoadedTheCatalogue(catalogue, QStringLiteral("pt_BR")),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    page.resize(kWidestAPageMayBe, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    page.ShowFoldersOutside(8);

    const QLabel* said = NoticeOf(page)->findChild<QLabel*>(QStringLiteral("TriageQuiet"));
    const QPushButton* import = NoticeOf(page)->findChild<QPushButton*>();

    QCOMPARE(said->text(), Said(8));
    QVERIFY(said->text() != QStringLiteral("8 folder outside the library"));
    QVERIFY(import->text() != QStringLiteral("Import into the library…"));
}

void CommunityPageTest::TheNoticeSitsBetweenTheActionsAndTheSearchWhichEndsTheLine_data()
{
    LanguageChoices();
}

void CommunityPageTest::TheNoticeSitsBetweenTheActionsAndTheSearchWhichEndsTheLine()
{
    QFETCH(const QString, language);

    QTranslator catalogue;
    QVERIFY2(LoadedTheCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    ApplyModernistTheme(*qApp);
    page.resize(kWidestAPageMayBe, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    page.ShowFoldersOutside(kFourDigits);
    QCoreApplication::processEvents();

    const auto* bar = page.findChild<QWidget*>(QStringLiteral("PageToolbar"));
    const auto* reread = bar->findChild<QPushButton*>(QStringLiteral("ReadDestinationsAgain"));
    const auto* search = bar->findChild<QLineEdit*>();

    QVERIFY(bar != nullptr);
    QVERIFY(reread != nullptr);
    QVERIFY(search != nullptr);

    const QRect first = PlaceIn(*bar, *reread);
    const QRect notice = PlaceIn(*bar, *NoticeOf(page));
    const QRect last = PlaceIn(*bar, *search);

    QVERIFY(NoticeOf(page)->isVisibleTo(&page));
    QCOMPARE(CenterOf(notice), CenterOf(first));
    QCOMPARE(CenterOf(last), CenterOf(first));
    QVERIFY(notice.x() > Right(first));
    QCOMPARE(last.x() - Right(notice), kDesignedGapBetweenTheNoticeAndTheSearch);
    QCOMPARE(Right(last), bar->width() - kPageGutter);

    const QLabel* said = NoticeOf(page)->findChild<QLabel*>(QStringLiteral("TriageQuiet"));
    QCOMPARE(said->text(), Said(kFourDigits));
}

void CommunityPageTest::WithFourDigitsOutsideThePageFitsTheNarrowestWindow_data()
{
    LanguageChoices();
}

void CommunityPageTest::WithFourDigitsOutsideThePageFitsTheNarrowestWindow()
{
    QFETCH(const QString, language);

    QTranslator catalogue;
    QVERIFY2(LoadedTheCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    page.ShowFoldersOutside(kFourDigits);
    page.resize(kWidestAPageMayBe, 600);

    ItFitsTheNarrowestWindow(page, "The destinations page with four digits of folders outside the library");

    QVERIFY(NoticeOf(page)->isVisibleTo(&page));
}

namespace
{
    int FootOfTheLowestButton(const QWidget& inside, const QWidget& host)
    {
        int foot = 0;

        for (const QPushButton* button : inside.findChildren<QPushButton*>())
        {
            if (button->isVisibleTo(&host))
            {
                foot = std::max(foot, button->mapTo(&host, QPoint(0, button->height())).y());
            }
        }

        return foot;
    }

    const QAbstractButton* TheHighestButton(const QWidget& inside, const QWidget& host)
    {
        const QAbstractButton* highest = nullptr;

        for (const auto* button : inside.findChildren<QAbstractButton*>())
        {
            if (button->isVisibleTo(&host)
                && (highest == nullptr
                    || button->mapTo(&host, QPoint(0, 0)).y() < highest->mapTo(&host, QPoint(0, 0)).y()))
            {
                highest = button;
            }
        }

        return highest;
    }
}

void CommunityPageTest::TheTriageStripLeavesTheRowBelowItItsOwnTopMarginAndNothingMore()
{
    Fixture f;
    ApplyModernistTheme(*qApp);

    QWidget host;
    auto* strip = new TriageStrip(&host);
    auto* pages = new QStackedWidget(&host);
    auto* page = new CommunityPage(f.viewModel, f.importViewModel, f.model, pages);
    pages->addWidget(page);

    auto* column = new QVBoxLayout(&host);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(strip);
    column->addWidget(pages, 1);

    strip->ShowBreakdown({.broken = 1});
    host.resize(1140, 600);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QCoreApplication::processEvents();

    QVERIFY(strip->isVisibleTo(&host));

    const QAbstractButton* firstBelow = TheHighestButton(*page, host);

    QVERIFY(firstBelow != nullptr);
    QVERIFY(firstBelow->parentWidget()->layout() != nullptr);

    const int footOfTheStrip = FootOfTheLowestButton(*strip, host);
    const int headOfTheRow = firstBelow->mapTo(&host, QPoint(0, 0)).y();

    QVERIFY(footOfTheStrip > 0);
    QCOMPARE(headOfTheRow - footOfTheStrip, firstBelow->parentWidget()->layout()->contentsMargins().top());
}

QTEST_MAIN(CommunityPageTest)

#include "tst_community_page.moc"
