#include <algorithm>

#include <QtTest/QtTest>

#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "tests/support/PathPrinting.h"

class EffectiveDestinationTest : public QObject
{
    Q_OBJECT

private slots:
    static void WithoutAnyOverrideTheProfileDefaultWins();
    static void AnOverrideOnTheAddonItselfWins();
    static void AnAddonInheritsTheOverrideOfItsCategory();
    static void TheNearestOverrideGoingUpWins();
    static void AnOverrideOfAnotherLibraryIsIgnored();
    static void AnOverrideOnTheLibraryRootReachesEverythingInside();
    static void TheRelativePathIsMatchedWithoutCaseOrSeparatorDifferences();
    static void AnAbsoluteAddonFolderFindsItsOwnLibraryAndOverride();
    static void AnAddonFolderOutsideEveryLibraryFallsBackToTheDefault();
    static void AnOverrideNamingAPathThatIsNoLongerADestinationDoesNotDecide();
};

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";

    SimulatorProfile ProfileWith(std::vector<DestinationOverride> overrides)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", "D:/MSFS 2024", "MSFS 2024"},
                             Library{"library-2", "F:/Extra Addons", "Extra Addons"}};
        profile.destinationOverrides = std::move(overrides);

        return profile;
    }
}

void EffectiveDestinationTest::WithoutAnyOverrideTheProfileDefaultWins()
{
    QCOMPARE(EffectiveDestination(ProfileWith({}), "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity));
}

void EffectiveDestinationTest::AnOverrideOnTheAddonItselfWins()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", "Aircrafts/pmdg-aircraft-77w", kCommunity2024}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::AnAddonInheritsTheOverrideOfItsCategory()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", "Aircrafts", kCommunity2024}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::TheNearestOverrideGoingUpWins()
{
    const SimulatorProfile profile =
        ProfileWith({{"library-1", "Aircrafts", kCommunity2024}, {"library-1", "Aircrafts/Fenix", kCommunity}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/Fenix/fenix-a320"),
             std::filesystem::path(kCommunity));
    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::AnOverrideOfAnotherLibraryIsIgnored()
{
    const SimulatorProfile profile = ProfileWith({{"library-2", "Aircrafts", kCommunity2024}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity));
}

void EffectiveDestinationTest::AnOverrideOnTheLibraryRootReachesEverythingInside()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", "", kCommunity2024}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity2024));
    QCOMPARE(EffectiveDestination(profile, "library-1", ""), std::filesystem::path(kCommunity2024));
    QCOMPARE(EffectiveDestination(profile, "library-1", "."), std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::TheRelativePathIsMatchedWithoutCaseOrSeparatorDifferences()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", R"(Aircrafts\Fenix)", kCommunity2024}});

    QCOMPARE(EffectiveDestination(profile, "library-1", "aircrafts/fenix/fenix-a320"),
             std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::AnAbsoluteAddonFolderFindsItsOwnLibraryAndOverride()
{
    const SimulatorProfile profile =
        ProfileWith({{"library-1", "Aircrafts", kCommunity2024}, {"library-2", "Aircrafts", kCommunity2024}});

    QCOMPARE(LibraryContaining(profile, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w")->id, std::string("library-1"));
    QCOMPARE(EffectiveDestination(profile, "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity2024));
    QCOMPARE(EffectiveDestination(profile, "D:/MSFS 2024/Sceneries/tlc-bgjn"), std::filesystem::path(kCommunity));
    QCOMPARE(EffectiveDestination(profile, "d:/msfs 2024/aircrafts/aerosoft-crj"),
             std::filesystem::path(kCommunity2024));
}

void EffectiveDestinationTest::AnAddonFolderOutsideEveryLibraryFallsBackToTheDefault()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", "", kCommunity2024}});

    QVERIFY(LibraryContaining(profile, "C:/Elsewhere/some-addon") == nullptr);
    QCOMPARE(EffectiveDestination(profile, "C:/Elsewhere/some-addon"), std::filesystem::path(kCommunity));
}

void EffectiveDestinationTest::AnOverrideNamingAPathThatIsNoLongerADestinationDoesNotDecide()
{
    const SimulatorProfile profile = ProfileWith({{"library-1", "Aircrafts", "E:/Flight Simulator 2024/Retired"}});

    QVERIFY(std::ranges::find(profile.destinations, std::filesystem::path("E:/Flight Simulator 2024/Retired"))
            == profile.destinations.end());
    QCOMPARE(EffectiveDestination(profile, "library-1", "Aircrafts/pmdg-aircraft-77w"),
             std::filesystem::path(kCommunity));
}

QTEST_APPLESS_MAIN(EffectiveDestinationTest)

#include "tst_effective_destination.moc"
