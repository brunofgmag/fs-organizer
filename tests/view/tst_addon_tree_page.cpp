#include <QtTest/QtTest>
#include <QtCore/QAbstractProxyModel>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeView>

#include <algorithm>
#include <string>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "application/DeletionService.h"
#include "view/library/AddonTreePage.h"
#include "viewmodel/DeletionViewModel.h"

namespace
{
    class AddonTreePageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ARescanPutsTheSelectionAndTheScrollBackWhereTheyWere();
        static void ARescanKeepsTheCurrentRowOfASelectionThatSpansSeveralAddons();
        static void AnAddonThatMovedRowIsFoundAgainBecauseItIsRememberedByPath();
        static void AnAddonThatVanishedLeavesTheTreeWithNothingSelected();
        static void ABatchWithNothingToDoSaysTheSelectionWasAlreadyAsAsked();
        static void ABatchStoppedByTheDiskSaysSoInsteadOfClaimingTheSelectionWasAlreadyRight();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kChosen = "D:/MSFS 2024/Aircrafts/addon-17";
    constexpr auto kCompanion = "D:/MSFS 2024/Aircrafts/addon-05";
    constexpr int kAddonsPerCategory = 20;

    const QStringList& Categories()
    {
        static const QStringList categories{QStringLiteral("Aircrafts"), QStringLiteral("Sceneries"),
                                            QStringLiteral("Traffic")};

        return categories;
    }

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    std::filesystem::path AddonPath(const QString& category, const int index)
    {
        return std::filesystem::path(kLibrary) / category.toStdString()
            / ("addon-" + QStringLiteral("%1").arg(index, 2, 10, QLatin1Char('0')).toStdString());
    }

    TreeNode CategoryNode(const QString& category, const std::vector<int>& order, const std::filesystem::path& skipped)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = std::filesystem::path(kLibrary) / category.toStdString();

        for (const int index : order)
        {
            const std::filesystem::path path = AddonPath(category, index);

            if (ComparablePath(path) != ComparablePath(skipped))
            {
                node.children.push_back(AddonNode(path));
            }
        }

        return node;
    }

    std::vector<int> Ascending()
    {
        std::vector<int> order;

        for (int index = 0; index < kAddonsPerCategory; ++index)
        {
            order.push_back(index);
        }

        return order;
    }

    TreeNode LibraryTree(const std::vector<int>& order, const std::filesystem::path& skipped)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;

        for (const QString& category : Categories())
        {
            node.children.push_back(CategoryNode(category, order, skipped));
        }

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
            fileSystem.AddDirectory(kLibrary);

            for (const QString& category : Categories())
            {
                fileSystem.AddDirectory(std::filesystem::path(kLibrary) / category.toStdString());

                for (int index = 0; index < kAddonsPerCategory; ++index)
                {
                    fileSystem.AddDirectory(AddonPath(category, index));
                }
            }

            catalog.SetTree(kLibrary, LibraryTree(Ascending(), {}));

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = Profile().id;
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, filesystemProbe, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        AddonTreeModel model;
        FakeSimulatorPackages packages;
        AddonTreeViewModel viewModel{session, service, model, packages, sizes, notifier};
        DeletionService deletionService{filesystemProbe, files, linking, classifier, processProbe, log, sizes};
        DeletionViewModel deletion{session, service, settings, deletionService, sizes};
    };

    const TreeNode* NodeUnder(const QTreeView& tree, const QModelIndex& position)
    {
        const auto* filter = qobject_cast<const QAbstractProxyModel*>(tree.model());

        return AddonTreeModel::NodeAt(filter->mapToSource(position));
    }

    QModelIndex IndexOf(const QTreeView& tree, const std::filesystem::path& path, const QModelIndex& parent)
    {
        for (int row = 0; row < tree.model()->rowCount(parent); ++row)
        {
            const QModelIndex position = tree.model()->index(row, 0, parent);
            const TreeNode* node = NodeUnder(tree, position);

            if (node != nullptr && ComparablePath(node->path) == ComparablePath(path))
            {
                return position;
            }

            if (const QModelIndex found = IndexOf(tree, path, position); found.isValid())
            {
                return found;
            }
        }

        return {};
    }

    void OpenEveryCategory(QTreeView& tree)
    {
        for (int row = 0; row < tree.model()->rowCount({}); ++row)
        {
            const QModelIndex library = tree.model()->index(row, 0, {});
            tree.expand(library);

            for (int child = 0; child < tree.model()->rowCount(library); ++child)
            {
                tree.expand(tree.model()->index(child, 0, library));
            }
        }
    }

    struct Screen
    {
        explicit Screen(Fixture& fixture) : page(fixture.viewModel, fixture.deletion, fixture.model, fixture.notifier)
        {
            page.resize(900, 320);
            page.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&page));

            fixture.viewModel.ShowActiveProfile();

            tree = page.findChild<QTreeView*>();
            OpenEveryCategory(*tree);
        }

        [[nodiscard]] std::vector<std::string> SelectedPaths() const
        {
            std::vector<std::string> paths;

            for (const QModelIndex& position : tree->selectionModel()->selectedRows())
            {
                if (const TreeNode* node = NodeUnder(*tree, position))
                {
                    paths.push_back(ComparablePath(node->path));
                }
            }

            return paths;
        }

        [[nodiscard]] std::string CurrentPath() const
        {
            const TreeNode* node = NodeUnder(*tree, tree->selectionModel()->currentIndex());

            return node == nullptr ? std::string{} : ComparablePath(node->path);
        }

        AddonTreePage page;
        QTreeView* tree = nullptr;
    };
}

void AddonTreePageTest::ARescanPutsTheSelectionAndTheScrollBackWhereTheyWere()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    screen.tree->setCurrentIndex(chosen);

    QScrollBar* bar = screen.tree->verticalScrollBar();
    QVERIFY(bar->maximum() > 0);

    bar->setValue(bar->maximum() / 2);
    const int scrolled = bar->value();
    QVERIFY(scrolled > 0);

    f.viewModel.ShowActiveProfile();
    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{ComparablePath(kChosen)});
    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));
    QCOMPARE(screen.tree->verticalScrollBar()->value(), scrolled);
}

void AddonTreePageTest::ARescanKeepsTheCurrentRowOfASelectionThatSpansSeveralAddons()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex first = IndexOf(*screen.tree, kCompanion, {});
    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(first.isValid());
    QVERIFY(chosen.isValid());

    screen.tree->selectionModel()->select(first, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    screen.tree->selectionModel()->setCurrentIndex(chosen, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));

    f.viewModel.ShowActiveProfile();
    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), (std::vector<std::string>{ComparablePath(kCompanion), ComparablePath(kChosen)}));
    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));
}

void AddonTreePageTest::AnAddonThatMovedRowIsFoundAgainBecauseItIsRememberedByPath()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    const int rowBefore = chosen.row();
    screen.tree->setCurrentIndex(chosen);

    std::vector<int> reversed = Ascending();
    std::ranges::reverse(reversed);
    f.catalog.SetTree(kLibrary, LibraryTree(reversed, {}));

    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{ComparablePath(kChosen)});
    QVERIFY(screen.tree->selectionModel()->selectedRows().front().row() != rowBefore);
}

void AddonTreePageTest::AnAddonThatVanishedLeavesTheTreeWithNothingSelected()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    screen.tree->setCurrentIndex(chosen);

    QScrollBar* bar = screen.tree->verticalScrollBar();
    bar->setValue(bar->maximum() / 2);
    QVERIFY(bar->value() > 0);

    f.catalog.SetTree(kLibrary, LibraryTree(Ascending(), kChosen));

    f.viewModel.ShowActiveProfile();

    QVERIFY(!IndexOf(*screen.tree, kChosen, {}).isValid());
    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{});
}

namespace
{
    const TreeNode* AddonOf(const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});

        return position.isValid() ? NodeUnder(*screen.tree, position) : nullptr;
    }

    QString LastStatusOf(const QSignalSpy& spy)
    {
        return spy.isEmpty() ? QString{} : spy.back().front().toString();
    }
}

void AddonTreePageTest::ABatchWithNothingToDoSaysTheSelectionWasAlreadyAsAsked()
{
    Fixture f;
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "addon-17", kChosen);

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    const TreeNode* addon = AddonOf(screen, kChosen);
    QVERIFY(addon != nullptr);

    f.viewModel.Toggle({addon}, true);

    QCOMPARE(LastStatusOf(status), QString{"Nothing to do: the selection was already the way you asked."});
}

void AddonTreePageTest::ABatchStoppedByTheDiskSaysSoInsteadOfClaimingTheSelectionWasAlreadyRight()
{
    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "addon-17";
    f.fileSystem.AddLink(link, kChosen);

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    const TreeNode* addon = AddonOf(screen, kChosen);
    QVERIFY(addon != nullptr);
    QVERIFY(f.fileSystem.RemoveNode(link));

    f.viewModel.Toggle({addon}, false);

    QCOMPARE(LastStatusOf(status),
             QString{"Nothing was applied: 1 addon was not the way the screen showed it. The list is up to date now."});
}

QTEST_MAIN(AddonTreePageTest)

#include "tst_addon_tree_page.moc"
