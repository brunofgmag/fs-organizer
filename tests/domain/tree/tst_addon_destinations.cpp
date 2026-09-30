#include <vector>

#include <QtTest/QtTest>

#include "domain/tree/AddonDestinations.h"
#include "domain/tree/DestinationDivergence.h"
#include "domain/tree/EffectiveDestination.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class AddonDestinationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ItAnswersTheSameDestinationTheFreeFunctionDoes();
        static void ItAnswersTheSameStrayedFolderTheFreeFunctionDoes();
        static void ItReadsTheFirstStrayedLinkJustLikeTheFreeFunction();
        static void ABrokenLinkAtThePlannedPathIsWhatLinksNowhereMeans();
        static void ABrokenLinkSomewhereElseLeavesTheAddonAlone();
        static void AnOverrideNamingAPathThatIsNoLongerADestinationDoesNotDecide();
        static void TheBatchAnswersTheSameStrayedFolderAsAskingEachAddonAlone();
        static void TheBatchKeepsTheOrderOfTheFoldersAndAnswersEmptyForAnAddonWithNoLink();
        static void TheBatchReadsTheFirstStrayedLinkOfAnAddonLinkedInTwoDestinations();
        static void TheBatchOfNoFolderAnswersNothing();
    };
}

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kLibrary = "D:/MSFS 2024";

    SimulatorProfile ProfileWith(std::vector<DestinationOverride> overrides)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"},
                             Library{.id = "library-2", .path = "F:/Extra Addons", .label = "Extra Addons"}};
        profile.destinationOverrides = std::move(overrides);

        return profile;
    }

    SimulatorProfile ProfileWithOverridesAtEveryLevel()
    {
        return ProfileWith(
            {{.libraryId = "library-1", .relativePath = "Sceneries", .destination = kCommunity2024},
             {.libraryId = "library-1", .relativePath = "Sceneries/Europe/orbx-eglc", .destination = kCommunity},
             {.libraryId = "library-2", .relativePath = "Aircrafts", .destination = kCommunity2024}});
    }

    std::vector<std::filesystem::path> FoldersToAsk()
    {
        return {"D:/MSFS 2024/Sceneries/Europe/orbx-eglc",
                "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn",
                "D:/MSFS 2024/Sceneries/Europe",
                "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w",
                "D:/MSFS 2024/loose-addon",
                "F:/Extra Addons/Aircrafts/fenix-a320",
                "G:/Somewhere Else/stray-addon"};
    }

    DestinationEntry LinkAt(const std::filesystem::path& path,
                            const std::filesystem::path& target,
                            const EntryClassification classification = EntryClassification::Managed)
    {
        return {.path = path, .target = target, .classification = classification};
    }

    std::vector<DestinationEntry> EntriesPointingAllOver()
    {
        return {
            LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", "D:/MSFS 2024/Sceneries/Europe/orbx-eglc"),
            LinkAt("E:/Flight Simulator 2024/Community2024/orbx-lfmn", "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn"),
            LinkAt("E:/Flight Simulator 2024/Community/orbx-lfmn", "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn",
                   EntryClassification::Duplicated),
            LinkAt("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w", "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
            LinkAt("E:/Flight Simulator 2024/Community/fenix-a320", "F:/Extra Addons/Aircrafts/fenix-a320"),
            LinkAt("E:/Flight Simulator 2024/Community/gone", "D:/MSFS 2024/loose-addon",
                   EntryClassification::Unmanaged)};
    }
}

void AddonDestinationsTest::ItAnswersTheSameDestinationTheFreeFunctionDoes()
{
    const SimulatorProfile profile = ProfileWithOverridesAtEveryLevel();
    const AddonDestinations prepared(profile, EntriesPointingAllOver());

    for (const std::filesystem::path& folder : FoldersToAsk())
    {
        QCOMPARE(prepared.Of(folder).destination, EffectiveDestination(profile, folder));
    }
}

void AddonDestinationsTest::ItAnswersTheSameStrayedFolderTheFreeFunctionDoes()
{
    const SimulatorProfile profile = ProfileWithOverridesAtEveryLevel();
    const std::vector<DestinationEntry> entries = EntriesPointingAllOver();
    const AddonDestinations prepared(profile, entries);

    for (const std::filesystem::path& folder : FoldersToAsk())
    {
        QCOMPARE(prepared.Of(folder).strayedTo, DestinationItStrayedTo(profile, entries, folder));
    }
}

void AddonDestinationsTest::ItReadsTheFirstStrayedLinkJustLikeTheFreeFunction()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn";
    const std::vector<DestinationEntry> entries = {
        LinkAt("E:/Flight Simulator 2024/Community2024/orbx-lfmn", folder),
        LinkAt("E:/Flight Simulator 2024/Community/orbx-lfmn", folder, EntryClassification::Duplicated),
        LinkAt("G:/Another Community/orbx-lfmn", folder, EntryClassification::Divergent)};

    const AddonDestinations prepared(profile, entries);

    QCOMPARE(prepared.Of(folder).strayedTo, DestinationItStrayedTo(profile, entries, folder));
    QCOMPARE(prepared.Of(folder).strayedTo, std::filesystem::path("E:/Flight Simulator 2024/Community2024"));
}

void AddonDestinationsTest::ABrokenLinkAtThePlannedPathIsWhatLinksNowhereMeans()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-eglc";
    const AddonDestinations prepared(
        profile, {LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", folder, EntryClassification::Broken)});

    QVERIFY(prepared.Of(folder).linksNowhere);
    QCOMPARE(prepared.Of(folder).destination, PlannedLinkPath(profile, folder).parent_path());
}

void AddonDestinationsTest::ABrokenLinkSomewhereElseLeavesTheAddonAlone()
{
    const SimulatorProfile profile = ProfileWith({});
    const AddonDestinations prepared(profile,
                                     {LinkAt("E:/Flight Simulator 2024/Community/another-addon",
                                             "D:/MSFS 2024/Sceneries/another-addon", EntryClassification::Broken)});

    QVERIFY(!prepared.Of("D:/MSFS 2024/Sceneries/Europe/orbx-eglc").linksNowhere);
}

void AddonDestinationsTest::AnOverrideNamingAPathThatIsNoLongerADestinationDoesNotDecide()
{
    const SimulatorProfile profile =
        ProfileWith({{.libraryId = "library-1", .relativePath = "Sceneries", .destination = "Z:/Gone"}});
    const AddonDestinations prepared(profile, {});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-eglc";

    QCOMPARE(prepared.Of(folder).destination, EffectiveDestination(profile, folder));
    QCOMPARE(prepared.Of(folder).destination, std::filesystem::path(kCommunity));
}

void AddonDestinationsTest::TheBatchAnswersTheSameStrayedFolderAsAskingEachAddonAlone()
{
    const SimulatorProfile profile = ProfileWithOverridesAtEveryLevel();
    const std::vector<DestinationEntry> entries = EntriesPointingAllOver();
    const std::vector<std::filesystem::path> folders = FoldersToAsk();

    const std::vector<std::filesystem::path> batch = DestinationsItStrayedTo(profile, entries, folders);

    QCOMPARE(batch.size(), folders.size());

    std::size_t strayed = 0;

    for (std::size_t index = 0; index < folders.size(); ++index)
    {
        QCOMPARE(batch[index], DestinationItStrayedTo(profile, entries, folders[index]));

        strayed += batch[index].empty() ? 0 : 1;
    }

    QVERIFY(strayed > 0);
    QVERIFY(strayed < folders.size());
}

void AddonDestinationsTest::TheBatchKeepsTheOrderOfTheFoldersAndAnswersEmptyForAnAddonWithNoLink()
{
    const SimulatorProfile profile = ProfileWithOverridesAtEveryLevel();
    const std::vector<DestinationEntry> entries = EntriesPointingAllOver();

    const std::vector<std::filesystem::path> batch = DestinationsItStrayedTo(
        profile, entries,
        {"D:/MSFS 2024/Sceneries/Europe", "F:/Extra Addons/Aircrafts/fenix-a320", "D:/MSFS 2024/Sceneries/Europe",
         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w", "F:/Extra Addons/Aircrafts/fenix-a320"});

    QCOMPARE(batch.size(), std::size_t{5});
    QVERIFY(batch[0].empty());
    QCOMPARE(batch[1], std::filesystem::path(kCommunity));
    QVERIFY(batch[2].empty());
    QVERIFY(batch[3].empty());
    QCOMPARE(batch[4], std::filesystem::path(kCommunity));
}

void AddonDestinationsTest::TheBatchReadsTheFirstStrayedLinkOfAnAddonLinkedInTwoDestinations()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn";
    const std::vector<DestinationEntry> entries = {
        LinkAt("E:/Flight Simulator 2024/Community2024/orbx-lfmn", folder),
        LinkAt("E:/Flight Simulator 2024/Community/orbx-lfmn", folder, EntryClassification::Duplicated),
        LinkAt("G:/Another Community/orbx-lfmn", folder, EntryClassification::Divergent)};

    const std::vector<std::filesystem::path> batch = DestinationsItStrayedTo(profile, entries, {folder});

    QCOMPARE(batch.size(), std::size_t{1});
    QCOMPARE(batch.front(), DestinationItStrayedTo(profile, entries, folder));
    QCOMPARE(batch.front(), std::filesystem::path("E:/Flight Simulator 2024/Community2024"));
}

void AddonDestinationsTest::TheBatchOfNoFolderAnswersNothing()
{
    const SimulatorProfile profile = ProfileWith({});

    QVERIFY(DestinationsItStrayedTo(profile, EntriesPointingAllOver(), {}).empty());
}

QTEST_MAIN(AddonDestinationsTest)
#include "tst_addon_destinations.moc"
