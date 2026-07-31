#include <QtTest/QtTest>

#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/CommunityModel.h"

class CommunityModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheTableShowsOneRowPerEntry();
    static void FilteringByEachClassificationReturnsExactlyItsSubset();
    static void ClearingTheFilterShowsEverythingAgain();
    static void AnEntryInConflictSaysSoAndCanBeFilteredOnItsOwn();
    static void ACellWithNothingExtraToSayLeavesTheTooltipToTheDelegate();
};

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";

    DestinationEntry Entry(const std::filesystem::path& path,
                           const std::filesystem::path& target,
                           const EntryClassification classification)
    {
        return {path, target, classification};
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;

        return profile;
    }

    std::vector<DestinationEntry> OneOfEachClass()
    {
        return {Entry("E:/Flight Simulator 2024/Community/managed", "D:/MSFS 2024/Sceneries/managed",
                      EntryClassification::Managed),
                Entry("E:/Flight Simulator 2024/Community/external", "C:/Elsewhere/external",
                      EntryClassification::External),
                Entry("E:/Flight Simulator 2024/Community/broken", "D:/Removed/broken", EntryClassification::Broken),
                Entry("E:/Flight Simulator 2024/Community/unavailable", "X:/Gone/unavailable",
                      EntryClassification::Unavailable),
                Entry("E:/Flight Simulator 2024/Community/physical", {}, EntryClassification::Unmanaged),
                Entry("E:/Flight Simulator 2024/Community2024/duplicated", "D:/MSFS 2024/Sceneries/duplicated",
                      EntryClassification::Duplicated)};
    }
}

void CommunityModelTest::TheTableShowsOneRowPerEntry()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), {});

    QCOMPARE(model.rowCount({}), 6);
    QCOMPARE(model.data(model.index(0, CommunityModel::NameColumn), Qt::DisplayRole).toString(),
             QStringLiteral("managed"));
    QCOMPARE(model.data(model.index(0, CommunityModel::DestinationColumn), Qt::DisplayRole).toString(),
             QStringLiteral("Community"));
    QCOMPARE(model.data(model.index(5, CommunityModel::DestinationColumn), Qt::DisplayRole).toString(),
             QStringLiteral("Community2024"));
    QCOMPARE(model.data(model.index(0, CommunityModel::TargetColumn), Qt::DisplayRole).toString(),
             QStringLiteral(R"(D:\MSFS 2024\Sceneries\managed)"));
    QCOMPARE(model.data(model.index(4, CommunityModel::TargetColumn), Qt::DisplayRole).toString(), QString());
}

void CommunityModelTest::FilteringByEachClassificationReturnsExactlyItsSubset()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), {});

    CommunityFilterModel filter;
    filter.setSourceModel(&model);

    const std::vector<std::pair<EntryClassification, QString>> classes = {
        {EntryClassification::Managed, "managed"},    {EntryClassification::External, "external"},
        {EntryClassification::Broken, "broken"},      {EntryClassification::Unavailable, "unavailable"},
        {EntryClassification::Unmanaged, "physical"}, {EntryClassification::Duplicated, "duplicated"},
    };

    for (const auto& [classification, name] : classes)
    {
        filter.ShowOnly(classification);

        QCOMPARE(filter.rowCount({}), 1);
        QCOMPARE(filter.data(filter.index(0, CommunityModel::NameColumn), Qt::DisplayRole).toString(), name);
    }
}

void CommunityModelTest::ClearingTheFilterShowsEverythingAgain()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), {});

    CommunityFilterModel filter;
    filter.setSourceModel(&model);

    filter.ShowOnly(EntryClassification::Broken);
    QCOMPARE(filter.rowCount({}), 1);

    filter.ShowOnly(std::nullopt);
    QCOMPARE(filter.rowCount({}), 6);
}

void CommunityModelTest::AnEntryInConflictSaysSoAndCanBeFilteredOnItsOwn()
{
    const CopyConflicts conflicts{
        {CopyConflict{"E:/Flight Simulator 2024/Community/physical", "D:/MSFS 2024/Utils/physical"}}};

    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), conflicts);

    const QModelIndex conflicted = model.index(4, CommunityModel::ClassificationColumn);
    QVERIFY(model.data(conflicted, CommunityModel::ConflictRole).toBool());
    QCOMPARE(model.data(conflicted, Qt::DisplayRole).toString(), QStringLiteral("Não gerenciada · em conflito"));
    QVERIFY(
        model.data(conflicted, Qt::ToolTipRole).toString().contains(QStringLiteral(R"(D:\MSFS 2024\Utils\physical)")));

    QVERIFY(!model.data(model.index(0, CommunityModel::ClassificationColumn), CommunityModel::ConflictRole).toBool());

    CommunityFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnlyTheConflicted(true);

    QCOMPARE(filter.rowCount({}), 1);
    QCOMPARE(filter.data(filter.index(0, CommunityModel::NameColumn), Qt::DisplayRole).toString(),
             QStringLiteral("physical"));
}

void CommunityModelTest::ACellWithNothingExtraToSayLeavesTheTooltipToTheDelegate()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), {});

    for (int column = 0; column <= CommunityModel::TargetColumn; ++column)
    {
        QVERIFY(model.data(model.index(0, column), Qt::ToolTipRole).toString().isEmpty());
    }
}

QTEST_APPLESS_MAIN(CommunityModelTest)

#include "tst_community_model.moc"
