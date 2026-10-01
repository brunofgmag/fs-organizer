#include <QtTest/QtTest>

#include <algorithm>

#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class AddonTreeFilterModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EmptyCategoriesAppearByDefaultAndHideOnDemand();
        static void HidingEmptyCategoriesReachesTheDeclaredOnesToo();
        static void SearchingByNameKeepsTheAncestorsOfMatches();
        static void ClearingTheSearchRestoresTheTree();
        static void TheStateFilterShowsOnlyTheAddonsTheModelCallsEnabledOrDisabled();
        static void ACategoryWithoutAMatchingAddonIsHiddenUnderAStateFilter();
        static void TheStateFilterAndTheSearchBothHaveToHold();
        static void TheStateFilterAndHidingEmptyCategoriesBothHold();
        static void TheFilterSaysWhetherItHidesAnyAddon();
        static void TheStateFilterFollowsTheModelWhenItIsShownAgain();
        static void UnderAStateFilterEachCountReadsShownOfTotal();
        static void UnderASearchEachCountReadsShownOfTotal();
        static void WithoutAFilterOrASearchTheCountsAreTheModelsOwn();
        static void TogglingAnAddonUnderAStateFilterMovesTheCounts();
        static void TheCountsAreWalkedOncePerChangeAndNotOncePerRead();
        static void ChangingTheFilterAnnouncesTheCounts();
        static void ACategoryEmptiedByARefreshLeavesTheEnabledFilter();
        static void ACategoryEmptiedByARefreshLeavesTheDisabledFilter();
    };
}

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

    TreeNode DeclaredCategoryNode(const std::filesystem::path& path)
    {
        TreeNode node = CategoryNode(path, {});
        node.declaredAsCategory = true;

        return node;
    }

    ProfileSnapshot Snapshot()
    {
        TreeNode library = CategoryNode("D:/MSFS 2024",
                                        {CategoryNode("D:/MSFS 2024/Aircrafts",
                                                      {AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                                                       AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")}),
                                         DeclaredCategoryNode("D:/MSFS 2024/Vazia")});
        library.kind = TreeNodeKind::Library;

        ProfileSnapshot snapshot;
        snapshot.libraries = {std::move(library)};

        return snapshot;
    }

    ProfileSnapshot MixedSnapshot(const std::vector<std::filesystem::path>& enabled)
    {
        TreeNode library =
            CategoryNode("D:/MSFS 2024",
                         {CategoryNode("D:/MSFS 2024/Aircrafts",
                                       {AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                                        AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")}),
                          CategoryNode("D:/MSFS 2024/Sceneries", {AddonNode("D:/MSFS 2024/Sceneries/lfpg-paris")}),
                          DeclaredCategoryNode("D:/MSFS 2024/Vazia")});
        library.kind = TreeNodeKind::Library;

        ProfileSnapshot snapshot;
        snapshot.libraries = {std::move(library)};
        snapshot.enabled = EnabledAddons(enabled);

        return snapshot;
    }

    QStringList Shown(const QAbstractItemModel& model, const QModelIndex& parent = {})
    {
        QStringList names;

        for (int row = 0; row < model.rowCount(parent); ++row)
        {
            const QModelIndex position = model.index(row, AddonTreeModel::AddonColumn, parent);

            names.append(model.data(position, Qt::DisplayRole).toString());
            names.append(Shown(model, position));
        }

        return names;
    }

    QModelIndex Named(const QAbstractItemModel& model, const QString& name, const QModelIndex& parent = {})
    {
        for (int row = 0; row < model.rowCount(parent); ++row)
        {
            const QModelIndex position = model.index(row, AddonTreeModel::AddonColumn, parent);

            if (model.data(position, Qt::DisplayRole).toString() == name)
            {
                return position;
            }

            if (const QModelIndex found = Named(model, name, position); found.isValid())
            {
                return found;
            }
        }

        return {};
    }

    QString CountOf(const QAbstractItemModel& model, const QString& name)
    {
        const QModelIndex position = Named(model, name);

        return position.isValid() ? model.data(position, QuietSuffixRole).toString() : QStringLiteral("not shown");
    }

    QString Said(const int shown, const int total)
    {
        return QStringLiteral("%1 of %2").arg(shown).arg(total);
    }

    QString SaidOfLibrary(const int categoriesShown, const int categories, const int addonsShown, const int addons)
    {
        return QStringLiteral("%1 · %2").arg(QStringLiteral("%1 of %2 category").arg(categoriesShown).arg(categories),
                                             QStringLiteral("%1 of %2 addon").arg(addonsShown).arg(addons));
    }

    void CompareEveryCountWithTheModel(const AddonTreeFilterModel& filter, const QModelIndex& parent = {})
    {
        for (int row = 0; row < filter.rowCount(parent); ++row)
        {
            const QModelIndex position = filter.index(row, AddonTreeModel::AddonColumn, parent);
            const QModelIndex source = filter.mapToSource(position);

            QCOMPARE(filter.data(position, QuietSuffixRole), source.model()->data(source, QuietSuffixRole));
            CompareEveryCountWithTheModel(filter, position);
        }
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

void AddonTreeFilterModelTest::HidingEmptyCategoriesReachesTheDeclaredOnesToo()
{
    TreeNode declared = CategoryNode("D:/MSFS 2024/Categoria Teste", {});
    declared.declaredAsCategory = true;

    TreeNode library =
        CategoryNode("D:/MSFS 2024",
                     {CategoryNode("D:/MSFS 2024/Aircrafts", {AddonNode("D:/MSFS 2024/Aircrafts/aerosoft-crj")}),
                      CategoryNode("D:/MSFS 2024/navigraph-efb-chartsapp", {}), std::move(declared)});
    library.kind = TreeNodeKind::Library;

    ProfileSnapshot snapshot;
    snapshot.libraries = {std::move(library)};

    AddonTreeModel model;
    model.Show(snapshot, Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    QCOMPARE(filter.rowCount(filter.index(0, 0, {})), 3);

    filter.HideEmptyCategories(true);

    const QModelIndex shown = filter.index(0, 0, {});
    QCOMPARE(filter.rowCount(shown), 1);
    QCOMPARE(filter.data(filter.index(0, AddonTreeModel::AddonColumn, shown), Qt::DisplayRole).toString(),
             QStringLiteral("Aircrafts"));
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
    QCOMPARE(filter.data(filter.index(0, AddonTreeModel::AddonColumn, category), Qt::DisplayRole).toString(),
             QStringLiteral("aerosoft-crj"));
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

void AddonTreeFilterModelTest::TheStateFilterShowsOnlyTheAddonsTheModelCallsEnabledOrDisabled()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.ShowOnly(AddonStateFilter::Enabled);
    QVERIFY(Shown(filter).contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(!Shown(filter).contains(QStringLiteral("fenix-a320")));
    QVERIFY(!Shown(filter).contains(QStringLiteral("lfpg-paris")));

    filter.ShowOnly(AddonStateFilter::Disabled);
    QVERIFY(!Shown(filter).contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(Shown(filter).contains(QStringLiteral("fenix-a320")));
    QVERIFY(Shown(filter).contains(QStringLiteral("lfpg-paris")));

    filter.ShowOnly(AddonStateFilter::All);
    QVERIFY(Shown(filter).contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(Shown(filter).contains(QStringLiteral("fenix-a320")));
    QVERIFY(Shown(filter).contains(QStringLiteral("lfpg-paris")));
}

void AddonTreeFilterModelTest::ACategoryWithoutAMatchingAddonIsHiddenUnderAStateFilter()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.ShowOnly(AddonStateFilter::Enabled);
    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("aerosoft-crj")}));

    filter.ShowOnly(AddonStateFilter::Disabled);
    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("fenix-a320"),
                          QStringLiteral("Sceneries"), QStringLiteral("lfpg-paris")}));

    filter.ShowOnly(AddonStateFilter::All);
    QVERIFY(Shown(filter).contains(QStringLiteral("Vazia")));
}

void AddonTreeFilterModelTest::TheStateFilterAndTheSearchBothHaveToHold()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search("A320");
    filter.ShowOnly(AddonStateFilter::Disabled);
    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("fenix-a320")}));

    filter.ShowOnly(AddonStateFilter::Enabled);
    QCOMPARE(filter.rowCount({}), 0);

    filter.ShowOnly(AddonStateFilter::All);
    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("fenix-a320")}));
}

void AddonTreeFilterModelTest::TheStateFilterAndHidingEmptyCategoriesBothHold()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj", "D:/MSFS 2024/Aircrafts/fenix-a320"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);
    filter.HideEmptyCategories(true);

    QVERIFY(!Shown(filter).contains(QStringLiteral("Vazia")));
    QVERIFY(Shown(filter).contains(QStringLiteral("Sceneries")));

    filter.ShowOnly(AddonStateFilter::Enabled);
    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("aerosoft-crj"),
                          QStringLiteral("fenix-a320")}));
}

void AddonTreeFilterModelTest::TheFilterSaysWhetherItHidesAnyAddon()
{
    AddonTreeFilterModel filter;

    QVERIFY(!filter.HidesAddons());
    QVERIFY(filter.ShowingOnly() == AddonStateFilter::All);

    filter.HideEmptyCategories(true);
    QVERIFY(!filter.HidesAddons());

    filter.Search("crj");
    QVERIFY(filter.HidesAddons());

    filter.Search({});
    QVERIFY(!filter.HidesAddons());

    filter.ShowOnly(AddonStateFilter::Disabled);
    QVERIFY(filter.HidesAddons());
    QVERIFY(filter.ShowingOnly() == AddonStateFilter::Disabled);
}

void AddonTreeFilterModelTest::TheStateFilterFollowsTheModelWhenItIsShownAgain()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnly(AddonStateFilter::Enabled);

    QVERIFY(Shown(filter).contains(QStringLiteral("aerosoft-crj")));

    model.Show(MixedSnapshot({"D:/MSFS 2024/Sceneries/lfpg-paris"}), Profile());

    QVERIFY(!Shown(filter).contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(Shown(filter).contains(QStringLiteral("lfpg-paris")));
}

void AddonTreeFilterModelTest::UnderAStateFilterEachCountReadsShownOfTotal()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.ShowOnly(AddonStateFilter::Enabled);
    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(1, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("Sceneries")), QStringLiteral("not shown"));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(1, 3, 1, 3));

    filter.ShowOnly(AddonStateFilter::Disabled);
    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(1, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("Sceneries")), Said(1, 1));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(2, 3, 2, 3));
}

void AddonTreeFilterModelTest::UnderASearchEachCountReadsShownOfTotal()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search("a320");
    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(1, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(1, 3, 1, 3));

    filter.Search("a");
    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(2, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("Sceneries")), Said(1, 1));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(2, 3, 3, 3));

    filter.ShowOnly(AddonStateFilter::Enabled);
    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(1, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(1, 3, 1, 3));
}

void AddonTreeFilterModelTest::WithoutAFilterOrASearchTheCountsAreTheModelsOwn()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    CompareEveryCountWithTheModel(filter);
    QVERIFY(!CountOf(filter, QStringLiteral("MSFS 2024")).contains(QStringLiteral(" of ")));

    filter.ShowOnly(AddonStateFilter::Disabled);
    filter.Search("a320");
    QVERIFY(CountOf(filter, QStringLiteral("Aircrafts")).contains(QStringLiteral(" of ")));

    filter.ShowOnly(AddonStateFilter::All);
    filter.Search({});
    CompareEveryCountWithTheModel(filter);
    QVERIFY(!CountOf(filter, QStringLiteral("Aircrafts")).contains(QStringLiteral(" of ")));
}

void AddonTreeFilterModelTest::TogglingAnAddonUnderAStateFilterMovesTheCounts()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnly(AddonStateFilter::Enabled);

    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(1, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(1, 3, 1, 3));

    model.Refresh(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj", "D:/MSFS 2024/Aircrafts/fenix-a320",
                                 "D:/MSFS 2024/Sceneries/lfpg-paris"}),
                  Profile());

    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), Said(2, 2));
    QCOMPARE(CountOf(filter, QStringLiteral("Sceneries")), Said(1, 1));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), SaidOfLibrary(2, 3, 3, 3));

    model.Refresh(MixedSnapshot({}), Profile());

    QCOMPARE(CountOf(filter, QStringLiteral("Aircrafts")), QStringLiteral("not shown"));
    QCOMPARE(CountOf(filter, QStringLiteral("MSFS 2024")), QStringLiteral("not shown"));
}

void AddonTreeFilterModelTest::TheCountsAreWalkedOncePerChangeAndNotOncePerRead()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    for (int round = 0; round < 50; ++round)
    {
        static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));
    }

    QCOMPARE(filter.TimesItWalkedTheTree(), std::size_t{0});

    filter.ShowOnly(AddonStateFilter::Enabled);

    for (int round = 0; round < 50; ++round)
    {
        static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));
        static_cast<void>(CountOf(filter, QStringLiteral("MSFS 2024")));
    }

    QCOMPARE(filter.TimesItWalkedTheTree(), std::size_t{1});

    filter.Search("crj");
    static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));
    static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));

    QCOMPARE(filter.TimesItWalkedTheTree(), std::size_t{2});

    model.Refresh(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj", "D:/MSFS 2024/Aircrafts/fenix-a320"}),
                  Profile());
    static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));
    static_cast<void>(CountOf(filter, QStringLiteral("Aircrafts")));

    QCOMPARE(filter.TimesItWalkedTheTree(), std::size_t{3});
}

void AddonTreeFilterModelTest::ChangingTheFilterAnnouncesTheCounts()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);

    const auto announcesTheCounts = [](const QSignalSpy& spy)
    {
        return std::ranges::any_of(spy,
                                   [](const QList<QVariant>& arguments)
                                   {
                                       return arguments.at(2).value<QList<int>>().contains(QuietSuffixRole);
                                   });
    };

    const QSignalSpy afterTheState(&filter, &AddonTreeFilterModel::dataChanged);
    filter.ShowOnly(AddonStateFilter::Disabled);
    QVERIFY(announcesTheCounts(afterTheState));

    const QSignalSpy afterTheSearch(&filter, &AddonTreeFilterModel::dataChanged);
    filter.Search("a320");
    QVERIFY(announcesTheCounts(afterTheSearch));
}

void AddonTreeFilterModelTest::ACategoryEmptiedByARefreshLeavesTheEnabledFilter()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj", "D:/MSFS 2024/Sceneries/lfpg-paris"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnly(AddonStateFilter::Enabled);

    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("aerosoft-crj"),
                          QStringLiteral("Sceneries"), QStringLiteral("lfpg-paris")}));

    model.Refresh(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("aerosoft-crj")}));
}

void AddonTreeFilterModelTest::ACategoryEmptiedByARefreshLeavesTheDisabledFilter()
{
    AddonTreeModel model;
    model.Show(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj"}), Profile());

    AddonTreeFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnly(AddonStateFilter::Disabled);

    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("fenix-a320"),
                          QStringLiteral("Sceneries"), QStringLiteral("lfpg-paris")}));

    model.Refresh(MixedSnapshot({"D:/MSFS 2024/Aircrafts/aerosoft-crj", "D:/MSFS 2024/Sceneries/lfpg-paris"}),
                  Profile());

    QCOMPARE(Shown(filter),
             (QStringList{QStringLiteral("MSFS 2024"), QStringLiteral("Aircrafts"), QStringLiteral("fenix-a320")}));
}

QTEST_APPLESS_MAIN(AddonTreeFilterModelTest)

#include "tst_addon_tree_filter_model.moc"
