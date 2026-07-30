#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>

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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/CommunityPage.h"
#include "view/panels/ContextPanel.h"
#include "viewmodel/SessionNotifier.h"

class CommunityPageTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheTriageConflictActionLeavesEveryConflictedRowSelected();
    static void TheTriageImportActionLeavesEveryUnmanagedFolderSelected();
    static void TheImportButtonCountsTheWholeSelectionAndNotTheFirstRow();
    static void AMixedSelectionOffersBothActionsEachWithItsOwnCount();
    static void OnlyTheActionThatUnblocksCarriesTheAccent();
    static void NothingConflictedMeansNoResolveButtonAtAll();
    static void ARescanThatEmptiesTheTableAlsoEmptiesThePanel();
};

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
        node.addon = Addon{path, Manifest{}};

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
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

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

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = Profile().id;
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
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};
        ImportService importService{engine,  processProbe, filesystemProbe,   catalog, files,
                                    linking, log,          LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, runner, notifier};
        CommunityModel model;
        CommunityViewModel viewModel{service, session, notifier, model};
        ImportViewModel importViewModel{importService, service, processProbe, session, runner};
    };

    int ConflictedAmong(const CommunityPage& page, const CommunityModel& model)
    {
        const auto* table = page.findChild<QTableView*>();
        int conflicted = 0;

        for (const QModelIndex& position : table->selectionModel()->selectedRows())
        {
            const QAbstractProxyModel* filter = qobject_cast<const QAbstractProxyModel*>(table->model());
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
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
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
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* table = page.findChild<QTableView*>();

    QVERIFY(!table->selectionModel()->selectedRows().isEmpty());
}

void CommunityPageTest::TheImportButtonCountsTheWholeSelectionAndNotTheFirstRow()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
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
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
    f.viewModel.Show();

    page.FilterBy(EntryClassification::Unmanaged);
    page.SelectEverythingShown();

    const auto* importChosen = page.findChild<QPushButton*>(QStringLiteral("ImportChosen"));
    const auto* resolveChosen = page.findChild<QPushButton*>(QStringLiteral("ResolveChosen"));

    QVERIFY(resolveChosen != nullptr);
    QVERIFY(resolveChosen->isVisibleTo(&page));
    QVERIFY(importChosen->isEnabled());

    QVERIFY(importChosen->text().contains(QStringLiteral("2")));
    QCOMPARE(resolveChosen->text(), QStringLiteral("Resolver o conflito..."));
}

void CommunityPageTest::OnlyTheActionThatUnblocksCarriesTheAccent()
{
    Fixture f;
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
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
    CommunityPage page(f.viewModel, f.importViewModel, f.model);
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

QTEST_MAIN(CommunityPageTest)

#include "tst_community_page.moc"
