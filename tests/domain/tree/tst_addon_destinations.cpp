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
        static void ItIsLinkedOnlyWhenALiveLinkTargetsTheFolder();
        static void ItIsBrokenOnlyWhenItIsLinkedAndItsPlannedPathLinksNowhere();
        static void ItNeedsRelinkingWhenItStrayedOrIsBroken();
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

void AddonDestinationsTest::ItIsLinkedOnlyWhenALiveLinkTargetsTheFolder()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-eglc";
    const std::filesystem::path another = "D:/MSFS 2024/Sceneries/Europe/orbx-lfmn";

    const AddonDestinations live(profile, {LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", folder)});
    const AddonDestinations dead(
        profile, {LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", folder, EntryClassification::Broken)});
    const AddonDestinations unmanaged(
        profile, {LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", folder, EntryClassification::Unmanaged)});
    const AddonDestinations elsewhere(profile, {LinkAt("E:/Flight Simulator 2024/Community/orbx-lfmn", another)});

    QVERIFY(live.Of(folder).linked);
    QVERIFY(!dead.Of(folder).linked);
    QVERIFY(!unmanaged.Of(folder).linked);
    QVERIFY(!elsewhere.Of(folder).linked);
}

void AddonDestinationsTest::ItIsBrokenOnlyWhenItIsLinkedAndItsPlannedPathLinksNowhere()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-eglc";
    const DestinationEntry dead = LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc",
                                         "D:/MSFS 2024/Sceneries/Europe/gone", EntryClassification::Broken);
    const DestinationEntry otherName = LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc-copy", folder);

    const AddonDestination onlyDead = AddonDestinations(profile, {dead}).Of(folder);
    const AddonDestination onlyLive = AddonDestinations(profile, {otherName}).Of(folder);
    const AddonDestination both = AddonDestinations(profile, {dead, otherName}).Of(folder);

    QVERIFY(onlyDead.linksNowhere);
    QVERIFY(!onlyDead.linked);
    QVERIFY(!onlyDead.IsBroken());
    QVERIFY(!onlyLive.linksNowhere);
    QVERIFY(onlyLive.linked);
    QVERIFY(!onlyLive.IsBroken());
    QVERIFY(both.IsBroken());
}

void AddonDestinationsTest::ItNeedsRelinkingWhenItStrayedOrIsBroken()
{
    const SimulatorProfile profile = ProfileWith({});
    const std::filesystem::path folder = "D:/MSFS 2024/Sceneries/Europe/orbx-eglc";
    const DestinationEntry healthy = LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc", folder);
    const DestinationEntry strayedLink = LinkAt("E:/Flight Simulator 2024/Community2024/orbx-eglc", folder);
    const DestinationEntry dead = LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc",
                                         "D:/MSFS 2024/Sceneries/Europe/gone", EntryClassification::Broken);
    const DestinationEntry otherName = LinkAt("E:/Flight Simulator 2024/Community/orbx-eglc-copy", folder);

    const AddonDestination strayed = AddonDestinations(profile, {strayedLink}).Of(folder);
    const AddonDestination broken = AddonDestinations(profile, {dead, otherName}).Of(folder);

    QVERIFY(!strayed.IsBroken());
    QVERIFY(strayed.NeedsRelinking());
    QVERIFY(broken.strayedTo.empty());
    QVERIFY(broken.NeedsRelinking());
    QVERIFY(!AddonDestinations(profile, {healthy}).Of(folder).NeedsRelinking());
    QVERIFY(!AddonDestinations(profile, {}).Of(folder).NeedsRelinking());
    QVERIFY(!AddonDestinations(profile, {dead}).Of(folder).NeedsRelinking());
}

QTEST_MAIN(AddonDestinationsTest)
#include "tst_addon_destinations.moc"
