#include <QtTest/QtTest>

#include <QtCore/QDir>

#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "support/PathText.h"
#include "viewmodel/CommunityModel.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/TagTone.h"

namespace
{
    class CommunityModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheTableShowsOneRowPerEntry();
        static void FilteringByEachClassificationReturnsExactlyItsSubset();
        static void ClearingTheFilterShowsEverythingAgain();
        static void AnEntryInConflictSaysSoAndCanBeFilteredOnItsOwn();
        static void ACellWithNothingExtraToSayLeavesTheTooltipToTheDelegate();
        static void ADuplicatedEntryLooksLikeADefectAndNotLikeSomethingToLeaveAlone();
        static void ADivergentEntrySaysTwoCopiesExistInsteadOfShowingAPath();
        static void AVanishedEntrySaysTheLibraryCopyIsGoneAndLooksLikeALoss();
        static void ADivergentEntryFindsItsConflictByTheFolderTheOtherProgramOwns();
        static void TheNameCellCarriesUnderItTheFolderTheEntryPointsAt();
        static void EveryClassificationSaysWhatItMeansInsteadOfRepeatingThePath();
        static void APhysicalFolderCarriesItsOwnPathUnderTheName();
    };
}

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";

    DestinationEntry Entry(const std::filesystem::path& path,
                           const std::filesystem::path& target,
                           const EntryClassification classification)
    {
        return {.path = path, .target = target, .classification = classification};
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

void CommunityModelTest::ADuplicatedEntryLooksLikeADefectAndNotLikeSomethingToLeaveAlone()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), {});

    const auto rowNamed = [&model](const QString& name)
    {
        for (int row = 0; row < model.rowCount({}); ++row)
        {
            if (model.data(model.index(row, CommunityModel::NameColumn), Qt::DisplayRole).toString() == name)
            {
                return row;
            }
        }

        return -1;
    };

    const int duplicated = rowNamed(QStringLiteral("duplicated"));
    const int broken = rowNamed(QStringLiteral("broken"));
    const int external = rowNamed(QStringLiteral("external"));

    QVERIFY(duplicated >= 0);

    QVERIFY(model.data(model.index(duplicated, CommunityModel::NameColumn), AlarmingRole).toBool());
    QVERIFY(model.data(model.index(broken, CommunityModel::NameColumn), AlarmingRole).toBool());
    QVERIFY(!model.data(model.index(external, CommunityModel::NameColumn), AlarmingRole).toBool());

    const auto toneOf = [&model](const int row)
    {
        return static_cast<TagTone>(
            model.data(model.index(row, CommunityModel::ClassificationColumn), TagToneRole).toInt());
    };

    QVERIFY(toneOf(duplicated) != TagTone::Muted);
    QCOMPARE(toneOf(external), TagTone::Muted);
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
             QStringLiteral("the simulator loads it from your library"));
    QCOMPARE(model.data(model.index(4, CommunityModel::TargetColumn), Qt::DisplayRole).toString(),
             QStringLiteral("a real folder, not in a library yet"));
    QCOMPARE(model.data(model.index(0, CommunityModel::NameColumn), SecondLineRole).toString(),
             QDir::toNativeSeparators(QStringLiteral("D:/MSFS 2024/Sceneries/managed")));
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
    const CopyConflicts conflicts{{CopyConflict{.provenancePath = "E:/Flight Simulator 2024/Community/physical",
                                                .libraryPath = "D:/MSFS 2024/Utils/physical"}}};

    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), conflicts);

    const QModelIndex conflicted = model.index(4, CommunityModel::ClassificationColumn);
    QVERIFY(model.data(conflicted, CommunityModel::ConflictRole).toBool());
    QCOMPARE(model.data(conflicted, Qt::DisplayRole).toString(), QStringLiteral("Unmanaged · in conflict"));
    QVERIFY(model.data(conflicted, Qt::ToolTipRole)
                .toString()
                .contains(QDir::toNativeSeparators(QStringLiteral("D:/MSFS 2024/Utils/physical"))));

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

namespace
{
    constexpr auto kVendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    constexpr auto kLibraryCopy = "D:/MSFS 2024/Utils/gsx-pro";

    DestinationEntry FromAnotherProgram(const EntryClassification classification)
    {
        return DestinationEntry{.path = "E:/Flight Simulator 2024/Community/fsdreamteam-gsx-pro",
                                .target = kLibraryCopy,
                                .classification = classification,
                                .externalOrigin = kVendorFolder,
                                .theOtherProgramTookItsFolderBack = classification == EntryClassification::Divergent};
    }
}

void CommunityModelTest::ADivergentEntrySaysTwoCopiesExistInsteadOfShowingAPath()
{
    CommunityModel model;
    model.ShowEntries({FromAnotherProgram(EntryClassification::Divergent)}, Profile(), CopyConflicts{});

    const QModelIndex classification = model.index(0, CommunityModel::ClassificationColumn);
    const QModelIndex target = model.index(0, CommunityModel::TargetColumn);

    QCOMPARE(model.data(classification, Qt::DisplayRole).toString(), QStringLiteral("Divergent"));
    QCOMPARE(model.data(target, Qt::DisplayRole).toString(), QStringLiteral("two copies exist"));
    QCOMPARE(model.data(classification, TagToneRole).toInt(), static_cast<int>(TagTone::Outlined));
}

void CommunityModelTest::AVanishedEntrySaysTheLibraryCopyIsGoneAndLooksLikeALoss()
{
    CommunityModel model;
    model.ShowEntries({FromAnotherProgram(EntryClassification::Vanished)}, Profile(), CopyConflicts{});

    const QModelIndex classification = model.index(0, CommunityModel::ClassificationColumn);
    const QModelIndex target = model.index(0, CommunityModel::TargetColumn);

    QCOMPARE(model.data(classification, Qt::DisplayRole).toString(), QStringLiteral("Vanished"));
    QCOMPARE(model.data(target, Qt::DisplayRole).toString(), QStringLiteral("the library copy is gone"));
    QCOMPARE(model.data(classification, TagToneRole).toInt(), static_cast<int>(TagTone::Filled));
    QVERIFY(model.data(classification, AlarmingRole).toBool());
}

void CommunityModelTest::ADivergentEntryFindsItsConflictByTheFolderTheOtherProgramOwns()
{
    const CopyConflicts conflicts{{CopyConflict{
        .provenancePath = kVendorFolder, .libraryPath = kLibraryCopy, .theProvenanceIsAnotherProgram = true}}};

    CommunityModel model;
    model.ShowEntries({FromAnotherProgram(EntryClassification::Divergent)}, Profile(), conflicts);

    const QModelIndex row = model.index(0, CommunityModel::ClassificationColumn);

    QVERIFY(model.ConflictAt(row) != nullptr);
    QCOMPARE(model.ConflictAt(row)->provenancePath, std::filesystem::path{kVendorFolder});
    QCOMPARE(model.data(row, Qt::DisplayRole).toString(), QStringLiteral("Divergent"));
}

void CommunityModelTest::TheNameCellCarriesUnderItTheFolderTheEntryPointsAt()
{
    CommunityModel model;
    model.ShowEntries({FromAnotherProgram(EntryClassification::Divergent),
                       Entry("E:/Flight Simulator 2024/Community/aerosoft-crj", "D:/MSFS 2024/Aircrafts/aerosoft-crj",
                             EntryClassification::Managed)},
                      Profile(), CopyConflicts{});

    QCOMPARE(model.data(model.index(0, CommunityModel::NameColumn), SecondLineRole).toString(),
             AsText(std::filesystem::path{kVendorFolder}));
    QCOMPARE(model.data(model.index(1, CommunityModel::NameColumn), SecondLineRole).toString(),
             AsText(std::filesystem::path{"D:/MSFS 2024/Aircrafts/aerosoft-crj"}));
    QVERIFY(model.data(model.index(0, CommunityModel::ClassificationColumn), SecondLineRole).toString().isEmpty());
}

void CommunityModelTest::EveryClassificationSaysWhatItMeansInsteadOfRepeatingThePath()
{
    CommunityModel model;
    model.ShowEntries(OneOfEachClass(), Profile(), CopyConflicts{});

    for (int row = 0; row < model.rowCount({}); ++row)
    {
        const QString meaning = model.data(model.index(row, CommunityModel::TargetColumn), Qt::DisplayRole).toString();

        QVERIFY(!meaning.isEmpty());
        QVERIFY(!meaning.contains(QStringLiteral("MSFS")));
    }
}

void CommunityModelTest::APhysicalFolderCarriesItsOwnPathUnderTheName()
{
    CommunityModel model;
    model.ShowEntries({Entry("E:/Flight Simulator 2024/Community/physical", {}, EntryClassification::Unmanaged)},
                      Profile(), CopyConflicts{});

    QCOMPARE(model.data(model.index(0, CommunityModel::NameColumn), SecondLineRole).toString(),
             AsText(std::filesystem::path{"E:/Flight Simulator 2024/Community/physical"}));
}

QTEST_APPLESS_MAIN(CommunityModelTest)

#include "tst_community_model.moc"
