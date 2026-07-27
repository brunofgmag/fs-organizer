#include <QtTest/QtTest>

#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeModel.h"

class AddonTreeFilterModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void EmptyCategoriesAppearByDefaultAndHideOnDemand();
    static void SearchingByNameKeepsTheAncestorsOfMatches();
    static void ClearingTheSearchRestoresTheTree();
};

namespace
{
    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;

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

    ProfileSnapshot Snapshot()
    {
        TreeNode library = CategoryNode("D:/MSFS 2024",
                                        {CategoryNode("D:/MSFS 2024/Aircrafts",
                                                      {AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                                                       AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")}),
                                         CategoryNode("D:/MSFS 2024/Vazia", {})});
        library.kind = TreeNodeKind::Library;

        ProfileSnapshot snapshot;
        snapshot.libraries = {std::move(library)};

        return snapshot;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {"E:/Flight Simulator 2024/Community"};
        profile.defaultDestination = "E:/Flight Simulator 2024/Community";

        return profile;
    }
}

void AddonTreeFilterModelTest::EmptyCategoriesAppearByDefaultAndHideOnDemand()
{
    AddonTreeModel model;
    model.Show(Snapshot(), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    const QModelIndex library = filter.index(0, 0, {});
    QCOMPARE(filter.rowCount(library), 2);

    filter.HideEmptyCategories(true);
    QCOMPARE(filter.rowCount(filter.index(0, 0, {})), 1);

    filter.HideEmptyCategories(false);
    QCOMPARE(filter.rowCount(filter.index(0, 0, {})), 2);
}

void AddonTreeFilterModelTest::SearchingByNameKeepsTheAncestorsOfMatches()
{
    AddonTreeModel model;
    model.Show(Snapshot(), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search("CRJ");

    QCOMPARE(filter.rowCount({}), 1);
    const QModelIndex library = filter.index(0, 0, {});
    QCOMPARE(filter.rowCount(library), 1);
    const QModelIndex category = filter.index(0, 0, library);
    QCOMPARE(filter.rowCount(category), 1);
    QCOMPARE(filter.data(filter.index(0, 0, category), Qt::DisplayRole).toString(), QStringLiteral("aerosoft-crj"));
}

void AddonTreeFilterModelTest::ClearingTheSearchRestoresTheTree()
{
    AddonTreeModel model;
    model.Show(Snapshot(), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search("fenix");
    QCOMPARE(filter.rowCount(filter.index(0, 0, filter.index(0, 0, {}))), 1);

    filter.Search({});
    QCOMPARE(filter.rowCount(filter.index(0, 0, filter.index(0, 0, {}))), 2);
}

QTEST_APPLESS_MAIN(AddonTreeFilterModelTest)

#include "tst_addon_tree_filter_model.moc"
