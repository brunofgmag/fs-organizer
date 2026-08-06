#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>

#include <QtCore/QDir>

#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/ModelRetranslation.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    class AddonTreeModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheTreeMirrorsTheSnapshotAndSurvivesTheModelTester();
        static void CheckStatesComeFromTheEnabledIndex();
        static void ClickingACheckboxAsksInsteadOfChangingTheModel();
        static void RefreshingTheIndexUpdatesCheckStatesWithoutResettingTheTree();
        static void OnlyANodeWhoseDestinationDiffersFromTheDefaultShowsIt();
        static void AnAddonInConflictSaysSoOnTheTreeAndInTheTooltip();
        static void AConflictThatArrivesLaterShowsUpWithoutResettingTheTree();
        static void OnlyAnAddonFolderAnswersThatItIsEnabled();
        static void TheConflictItselfIsHandedOverForWhoeverHasToResolveIt();
        static void OnlyAnAddonLinkedAwayFromItsOwnDestinationIsMarkedAsDivergent();
        static void AnAddonLinkedElsewhereSaysOnTheTreeWhereItActuallySits();
        static void ABrokenLinkWearsTheTagAndAlarmsTheRow();
        static void OnlyTheNameColumnCarriesTheCheckbox();
        static void TheModelCountsAddonsAndHowManyAreEnabled();
        static void RetranslatingKeepsThePersistentIndexesOfAProxyUsable();
        static void ASelectedCategoryTalliesTheAddonsUnderItAtAnyDepth();
        static void AnAddonSelectedInsideASelectedCategoryIsTalliedOnce();
        static void TheTallyReadsEnabledBrokenAndStrayedFromTheAddonsItReached();
    };
}

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kPmdg = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kCrj = "D:/MSFS 2024/Aircrafts/aerosoft-crj";

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
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode(kPmdg), AddonNode(kCrj)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = "D:/MSFS 2024";
        library.children = {aircrafts};

        return library;
    }

    SimulatorProfile Profile(std::vector<DestinationOverride> overrides = {})
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = "D:/MSFS 2024", .label = "Biblioteca do Bruno"}};
        profile.destinationOverrides = std::move(overrides);

        return profile;
    }

    ProfileSnapshot SnapshotWith(const std::vector<std::filesystem::path>& enabled)
    {
        ProfileSnapshot snapshot;
        snapshot.libraries = {LibraryTree()};
        snapshot.enabled = EnabledAddons(enabled);

        return snapshot;
    }

    QModelIndex Category(const AddonTreeModel& model)
    {
        return model.index(0, 0, model.index(0, 0, {}));
    }

    QModelIndex AddonAt(const AddonTreeModel& model, const int row)
    {
        return model.index(row, 0, Category(model));
    }

    DestinationEntry LinkIn(const std::filesystem::path& destination, const std::filesystem::path& addonFolder)
    {
        return DestinationEntry{.path = destination / addonFolder.filename(),
                                .target = addonFolder,
                                .classification = EntryClassification::Managed};
    }

    QString TextOf(const AddonTreeModel& model, const QModelIndex& row, const int column)
    {
        return model.data(row.siblingAtColumn(column), Qt::DisplayRole).toString();
    }

    constexpr auto kEmbraer = "D:/MSFS 2024/Aircrafts/Jets/e195";

    ProfileSnapshot NestedSnapshotWith(const std::vector<std::filesystem::path>& enabled)
    {
        TreeNode jets;
        jets.kind = TreeNodeKind::Category;
        jets.path = "D:/MSFS 2024/Aircrafts/Jets";
        jets.children = {AddonNode(kEmbraer)};

        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode(kPmdg), AddonNode(kCrj), jets};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = "D:/MSFS 2024";
        library.children = {aircrafts};

        ProfileSnapshot snapshot;
        snapshot.libraries = {library};
        snapshot.enabled = EnabledAddons(enabled);

        return snapshot;
    }
}

void AddonTreeModelTest::TheTreeMirrorsTheSnapshotAndSurvivesTheModelTester()
{
    AddonTreeModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.Show(SnapshotWith({}), Profile());

    QCOMPARE(model.rowCount({}), 1);
    QCOMPARE(model.rowCount(model.index(0, 0, {})), 1);
    QCOMPARE(model.rowCount(Category(model)), 2);
    QCOMPARE(model.parent(Category(model)), model.index(0, 0, {}));
    QCOMPARE(model.columnCount({}), int{AddonTreeModel::Columns});
    QCOMPARE(TextOf(model, model.index(0, 0, {}), AddonTreeModel::AddonColumn), QStringLiteral("Biblioteca do Bruno"));
}

void AddonTreeModelTest::CheckStatesComeFromTheEnabledIndex()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({kPmdg}), Profile());

    QCOMPARE(model.data(Category(model), Qt::CheckStateRole).toInt(), Qt::PartiallyChecked);
    QCOMPARE(model.data(model.index(0, 0, Category(model)), Qt::CheckStateRole).toInt(), Qt::Checked);
    QCOMPARE(model.data(model.index(1, 0, Category(model)), Qt::CheckStateRole).toInt(), Qt::Unchecked);
}

void AddonTreeModelTest::ClickingACheckboxAsksInsteadOfChangingTheModel()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}), Profile());

    const QSignalSpy asked(&model, &AddonTreeModel::ToggleRequested);

    QVERIFY(!model.setData(Category(model), Qt::Checked, Qt::CheckStateRole));
    QCOMPARE(asked.size(), 1);
    QCOMPARE(model.data(Category(model), Qt::CheckStateRole).toInt(), Qt::Unchecked);
}

void AddonTreeModelTest::RefreshingTheIndexUpdatesCheckStatesWithoutResettingTheTree()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}), Profile());

    const QSignalSpy reset(&model, &AddonTreeModel::modelReset);
    const QSignalSpy changed(&model, &AddonTreeModel::dataChanged);

    model.Refresh(SnapshotWith({kPmdg}), Profile());

    QCOMPARE(reset.size(), 0);
    QVERIFY(!changed.empty());
    QCOMPARE(model.data(model.index(0, 0, Category(model)), Qt::CheckStateRole).toInt(), Qt::Checked);
    QCOMPARE(model.data(Category(model), Qt::CheckStateRole).toInt(), Qt::PartiallyChecked);
}

void AddonTreeModelTest::OnlyANodeWhoseDestinationDiffersFromTheDefaultShowsIt()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}),
               Profile({{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kCommunity2024}}));

    QCOMPARE(TextOf(model, Category(model), AddonTreeModel::AddonColumn), QStringLiteral("Aircrafts"));
    QCOMPARE(TextOf(model, Category(model), AddonTreeModel::DestinationColumn),
             QStringLiteral("Community2024 · pinned"));
    QCOMPARE(TextOf(model, AddonAt(model, 0), AddonTreeModel::DestinationColumn),
             QStringLiteral("Community2024 · pinned"));
    QCOMPARE(TextOf(model, model.index(0, 0, {}), AddonTreeModel::DestinationColumn), QStringLiteral("Community"));
}

void AddonTreeModelTest::AnAddonInConflictSaysSoOnTheTreeAndInTheTooltip()
{
    ProfileSnapshot snapshot = SnapshotWith({});
    snapshot.conflicts = CopyConflicts{{CopyConflict{
        .destinationPath = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", .libraryPath = kPmdg}}};

    AddonTreeModel model;
    model.Show(snapshot, Profile());

    const QModelIndex conflicted = model.index(0, 0, Category(model));
    QVERIFY(model.data(conflicted, AddonTreeModel::ConflictRole).toBool());
    QCOMPARE(TextOf(model, conflicted, AddonTreeModel::AddonColumn), QStringLiteral("pmdg-aircraft-77w"));
    QCOMPARE(model.data(conflicted.siblingAtColumn(AddonTreeModel::AddonColumn), TagTextRole).toString(),
             QStringLiteral("In conflict"));
    QVERIFY(model.data(conflicted, AlarmingRole).toBool());
    QVERIFY(model.data(conflicted, Qt::ToolTipRole)
                .toString()
                .contains(
                    QDir::toNativeSeparators(QStringLiteral("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"))));

    const QModelIndex quiet = model.index(1, 0, Category(model));
    QVERIFY(!model.data(quiet, AddonTreeModel::ConflictRole).toBool());
    QCOMPARE(TextOf(model, quiet, AddonTreeModel::AddonColumn), QStringLiteral("aerosoft-crj"));
    QVERIFY(model.data(quiet.siblingAtColumn(AddonTreeModel::AddonColumn), TagTextRole).toString().isEmpty());
}

void AddonTreeModelTest::AConflictThatArrivesLaterShowsUpWithoutResettingTheTree()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}), Profile());

    const QSignalSpy reset(&model, &AddonTreeModel::modelReset);

    ProfileSnapshot refreshed = SnapshotWith({});
    refreshed.conflicts = CopyConflicts{
        {CopyConflict{.destinationPath = "E:/Flight Simulator 2024/Community/aerosoft-crj", .libraryPath = kCrj}}};

    model.Refresh(refreshed, Profile());

    QCOMPARE(reset.size(), 0);
    QVERIFY(model.data(model.index(1, 0, Category(model)), AddonTreeModel::ConflictRole).toBool());
    QVERIFY(!model.data(model.index(0, 0, Category(model)), AddonTreeModel::ConflictRole).toBool());
}

void AddonTreeModelTest::OnlyAnAddonFolderAnswersThatItIsEnabled()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({kPmdg}), Profile());

    QVERIFY(model.data(model.index(0, 0, Category(model)), AddonTreeModel::EnabledRole).toBool());
    QVERIFY(!model.data(model.index(1, 0, Category(model)), AddonTreeModel::EnabledRole).toBool());
    QVERIFY(!model.data(Category(model), AddonTreeModel::EnabledRole).toBool());
}

void AddonTreeModelTest::TheConflictItselfIsHandedOverForWhoeverHasToResolveIt()
{
    ProfileSnapshot snapshot = SnapshotWith({});
    snapshot.conflicts = CopyConflicts{{CopyConflict{
        .destinationPath = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", .libraryPath = kPmdg}}};

    AddonTreeModel model;
    model.Show(snapshot, Profile());

    const QVariant details = model.data(model.index(0, 0, Category(model)), AddonTreeModel::ConflictDetailsRole);

    QVERIFY(details.isValid());
    QCOMPARE(details.value<CopyConflict>().destinationPath,
             std::filesystem::path("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(details.value<CopyConflict>().libraryPath, std::filesystem::path(kPmdg));

    QVERIFY(!model.data(model.index(1, 0, Category(model)), AddonTreeModel::ConflictDetailsRole).isValid());
}

void AddonTreeModelTest::OnlyAnAddonLinkedAwayFromItsOwnDestinationIsMarkedAsDivergent()
{
    AddonTreeModel model;
    ProfileSnapshot snapshot = SnapshotWith({kPmdg, kCrj});
    snapshot.entries = {LinkIn(kCommunity2024, kPmdg), LinkIn(kCommunity, kCrj)};

    model.Show(snapshot, Profile());

    QVERIFY(model.data(AddonAt(model, 0), AddonTreeModel::DivergentRole).toBool());
    QVERIFY(!model.data(AddonAt(model, 1), AddonTreeModel::DivergentRole).toBool());
}

void AddonTreeModelTest::AnAddonLinkedElsewhereSaysOnTheTreeWhereItActuallySits()
{
    AddonTreeModel model;
    ProfileSnapshot snapshot = SnapshotWith({kPmdg, kCrj});
    snapshot.entries = {LinkIn(kCommunity2024, kPmdg), LinkIn(kCommunity, kCrj)};

    model.Show(snapshot, Profile());

    QCOMPARE(TextOf(model, AddonAt(model, 0), AddonTreeModel::DestinationColumn), QStringLiteral("Community2024"));
    QCOMPARE(TextOf(model, AddonAt(model, 1), AddonTreeModel::DestinationColumn), QStringLiteral("Community"));
    QVERIFY(model.data(AddonAt(model, 0).siblingAtColumn(AddonTreeModel::DestinationColumn), AlertRole).toBool());
    QVERIFY(model.data(AddonAt(model, 0), Qt::ToolTipRole).toString().contains(QStringLiteral("Community2024")));
}

void AddonTreeModelTest::ABrokenLinkWearsTheTagAndAlarmsTheRow()
{
    AddonTreeModel model;
    ProfileSnapshot snapshot = SnapshotWith({kPmdg, kCrj});
    snapshot.entries = {DestinationEntry{.path = std::filesystem::path(kCommunity) / "pmdg-aircraft-77w",
                                         .target = kPmdg,
                                         .classification = EntryClassification::Broken},
                        LinkIn(kCommunity, kCrj)};

    model.Show(snapshot, Profile());

    const QModelIndex broken = AddonAt(model, 0).siblingAtColumn(AddonTreeModel::AddonColumn);

    QVERIFY(model.data(broken, AddonTreeModel::BrokenRole).toBool());
    QCOMPARE(model.data(broken, TagTextRole).toString(), QStringLiteral("No target"));
    QCOMPARE(model.data(broken, TagToneRole).toInt(), static_cast<int>(TagTone::Filled));
    QVERIFY(model.data(broken, AlarmingRole).toBool());

    QVERIFY(!model.data(AddonAt(model, 1), AddonTreeModel::BrokenRole).toBool());
}

void AddonTreeModelTest::OnlyTheNameColumnCarriesTheCheckbox()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({kPmdg}), Profile());

    const QModelIndex named = AddonAt(model, 0);

    QVERIFY(model.flags(named).testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(model.data(named, Qt::CheckStateRole).isValid());

    const QModelIndex version = named.siblingAtColumn(AddonTreeModel::VersionColumn);

    QVERIFY(!model.flags(version).testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!model.data(version, Qt::CheckStateRole).isValid());
}

void AddonTreeModelTest::TheModelCountsAddonsAndHowManyAreEnabled()
{
    AddonTreeModel model;

    QCOMPARE(model.AddonCount(), std::size_t{0});
    QCOMPARE(model.EnabledCount(), std::size_t{0});

    model.Show(SnapshotWith({kPmdg}), Profile());

    QCOMPARE(model.AddonCount(), std::size_t{2});
    QCOMPARE(model.EnabledCount(), std::size_t{1});

    model.Refresh(SnapshotWith({kPmdg, kCrj}), Profile());

    QCOMPARE(model.EnabledCount(), std::size_t{2});
}

void AddonTreeModelTest::RetranslatingKeepsThePersistentIndexesOfAProxyUsable()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({kPmdg}), Profile());

    AddonTreeFilterModel proxy;
    proxy.setSourceModel(&model);

    const QModelIndex library = proxy.index(0, 0, {});
    QVERIFY(library.isValid());
    QVERIFY(proxy.rowCount(library) > 0);

    const QPersistentModelIndex category(proxy.index(0, 0, library));
    QVERIFY(category.isValid());
    QVERIFY(proxy.rowCount(category) > 0);

    SayTheModelWasRetranslated(model);

    const QModelIndex again = proxy.index(0, 0, {});
    QVERIFY2(again.isValid(), "the proxy lost the root on the language change");

    const QModelIndex child = proxy.index(0, 0, again);
    QVERIFY2(child.isValid(), "the proxy lost the mapping of the children on the language change");
    QVERIFY(child.data(Qt::DisplayRole).isValid());
    QCOMPARE(proxy.rowCount(child), model.rowCount(proxy.mapToSource(child)));
}

void AddonTreeModelTest::ASelectedCategoryTalliesTheAddonsUnderItAtAnyDepth()
{
    AddonTreeModel model;
    model.Show(NestedSnapshotWith({}), Profile());

    const QModelIndex aircrafts = Category(model);
    const SelectionTally tally = model.TallyOf({AddonTreeModel::NodeAt(aircrafts)});

    QCOMPARE(tally.addons.size(), std::size_t{3});
    QCOMPARE(tally.categories, std::size_t{1});
    QVERIFY(std::ranges::find(tally.addons, std::filesystem::path(kEmbraer)) != tally.addons.end());
}

void AddonTreeModelTest::AnAddonSelectedInsideASelectedCategoryIsTalliedOnce()
{
    AddonTreeModel model;
    model.Show(NestedSnapshotWith({}), Profile());

    const QModelIndex aircrafts = Category(model);
    const SelectionTally tally =
        model.TallyOf({AddonTreeModel::NodeAt(aircrafts), AddonTreeModel::NodeAt(AddonAt(model, 0))});

    QCOMPARE(tally.addons.size(), std::size_t{3});
    QCOMPARE(tally.categories, std::size_t{1});
    QCOMPARE(tally.categoriesCrossed, std::size_t{2});
}

void AddonTreeModelTest::TheTallyReadsEnabledBrokenAndStrayedFromTheAddonsItReached()
{
    AddonTreeModel model;
    ProfileSnapshot snapshot = NestedSnapshotWith({kPmdg, kEmbraer});
    snapshot.entries = {LinkIn(kCommunity, kPmdg),
                        DestinationEntry{.path = std::filesystem::path(kCommunity) / "e195",
                                         .target = kEmbraer,
                                         .classification = EntryClassification::Broken}};

    model.Show(snapshot, Profile());

    const SelectionTally tally = model.TallyOf({AddonTreeModel::NodeAt(Category(model))});

    QCOMPARE(tally.addons.size(), std::size_t{3});
    QCOMPARE(tally.enabled, std::size_t{2});
    QCOMPARE(tally.broken, std::size_t{1});
    QVERIFY(tally.alarming);
}

QTEST_MAIN(AddonTreeModelTest)

#include "tst_addon_tree_model.moc"
