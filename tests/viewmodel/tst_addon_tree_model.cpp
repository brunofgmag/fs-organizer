#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QtTest>

#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeModel.h"

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
};

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
        node.addon = Addon{path, Manifest{}};

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
        profile.libraries = {Library{"library-1", "D:/MSFS 2024", "Biblioteca do Bruno"}};
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
    QCOMPARE(model.data(model.index(0, 0, {}), Qt::DisplayRole).toString(), QStringLiteral("Biblioteca do Bruno"));
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
    QVERIFY(changed.size() > 0);
    QCOMPARE(model.data(model.index(0, 0, Category(model)), Qt::CheckStateRole).toInt(), Qt::Checked);
    QCOMPARE(model.data(Category(model), Qt::CheckStateRole).toInt(), Qt::PartiallyChecked);
}

void AddonTreeModelTest::OnlyANodeWhoseDestinationDiffersFromTheDefaultShowsIt()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}), Profile({{"library-1", "Aircrafts", kCommunity2024}}));

    QCOMPARE(model.data(model.index(0, 0, {}), Qt::DisplayRole).toString(), QStringLiteral("Biblioteca do Bruno"));
    QCOMPARE(model.data(Category(model), Qt::DisplayRole).toString(), QStringLiteral("Aircrafts  →  Community2024"));
    QCOMPARE(model.data(model.index(0, 0, Category(model)), Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w  →  Community2024"));
}

void AddonTreeModelTest::AnAddonInConflictSaysSoOnTheTreeAndInTheTooltip()
{
    ProfileSnapshot snapshot = SnapshotWith({});
    snapshot.conflicts = CopyConflicts{{CopyConflict{"E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kPmdg}}};

    AddonTreeModel model;
    model.Show(snapshot, Profile());

    const QModelIndex conflicted = model.index(0, 0, Category(model));
    QVERIFY(model.data(conflicted, AddonTreeModel::ConflictRole).toBool());
    QCOMPARE(model.data(conflicted, Qt::DisplayRole).toString(), QStringLiteral("pmdg-aircraft-77w (em conflito)"));
    QVERIFY(model.data(conflicted, Qt::ToolTipRole)
                .toString()
                .contains(QStringLiteral("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w")));

    const QModelIndex quiet = model.index(1, 0, Category(model));
    QVERIFY(!model.data(quiet, AddonTreeModel::ConflictRole).toBool());
    QCOMPARE(model.data(quiet, Qt::DisplayRole).toString(), QStringLiteral("aerosoft-crj"));
}

void AddonTreeModelTest::AConflictThatArrivesLaterShowsUpWithoutResettingTheTree()
{
    AddonTreeModel model;
    model.Show(SnapshotWith({}), Profile());

    const QSignalSpy reset(&model, &AddonTreeModel::modelReset);

    ProfileSnapshot refreshed = SnapshotWith({});
    refreshed.conflicts = CopyConflicts{{CopyConflict{"E:/Flight Simulator 2024/Community/aerosoft-crj", kCrj}}};

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
    snapshot.conflicts = CopyConflicts{{CopyConflict{"E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", kPmdg}}};

    AddonTreeModel model;
    model.Show(snapshot, Profile());

    const QVariant details = model.data(model.index(0, 0, Category(model)), AddonTreeModel::ConflictDetailsRole);

    QVERIFY(details.isValid());
    QCOMPARE(details.value<CopyConflict>().destinationPath,
             std::filesystem::path("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w"));
    QCOMPARE(details.value<CopyConflict>().libraryPath, std::filesystem::path(kPmdg));

    QVERIFY(!model.data(model.index(1, 0, Category(model)), AddonTreeModel::ConflictDetailsRole).isValid());
}

QTEST_MAIN(AddonTreeModelTest)

#include "tst_addon_tree_model.moc"
