#include <QtTest/QtTest>

#include "domain/profile/OrphanOverrides.h"
#include "domain/tree/EffectiveDestination.h"
#include "tests/support/PathPrinting.h"

class OrphanOverridesTest : public QObject
{
    Q_OBJECT

private slots:
    static void AnOverridePointingAtADestinationOfTheProfileIsNotAnOrphan();
    static void AnOverridePointingOutsideTheProfileIsReported();
    static void ADestinationIsRecognisedWhateverTheCaseAndTheSeparator();
    static void AnOrphanOverrideDoesNotDecideWhereTheAddonGoes();
    static void DroppingTheOrphansKeepsTheOnesThatStillPoint();
};

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kOtherDestination = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kGoneDestination = "E:/Flight Simulator 2020/Community";
    constexpr auto kLibrary = "D:/MSFS 2024";

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.destinations = {kCommunity, kOtherDestination};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }
}

void OrphanOverridesTest::AnOverridePointingAtADestinationOfTheProfileIsNotAnOrphan()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {DestinationOverride{"library-1", "Aircrafts", kOtherDestination}};

    QVERIFY(OverridesPointingNowhere(profile).empty());
}

void OrphanOverridesTest::AnOverridePointingOutsideTheProfileIsReported()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {DestinationOverride{"library-1", "Aircrafts", kGoneDestination},
                                    DestinationOverride{"library-1", "Sceneries", kOtherDestination}};

    const std::vector<DestinationOverride> orphans = OverridesPointingNowhere(profile);

    QCOMPARE(orphans.size(), std::size_t{1});
    QCOMPARE(orphans.front().relativePath, std::filesystem::path{"Aircrafts"});
    QCOMPARE(orphans.front().destination, std::filesystem::path{kGoneDestination});
}

void OrphanOverridesTest::ADestinationIsRecognisedWhateverTheCaseAndTheSeparator()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {
        DestinationOverride{"library-1", "Aircrafts", R"(e:\flight simulator 2024\community2024\)"}};

    QVERIFY(OverridesPointingNowhere(profile).empty());
}

void OrphanOverridesTest::AnOrphanOverrideDoesNotDecideWhereTheAddonGoes()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {DestinationOverride{"library-1", "Aircrafts", kGoneDestination}};

    QCOMPARE(EffectiveDestination(profile, "D:/MSFS 2024/Aircrafts/pmdg-777"), std::filesystem::path{kCommunity});
}

void OrphanOverridesTest::DroppingTheOrphansKeepsTheOnesThatStillPoint()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {DestinationOverride{"library-1", "Aircrafts", kGoneDestination},
                                    DestinationOverride{"library-1", "Sceneries", kOtherDestination}};

    DropOverridesPointingNowhere(profile);

    QCOMPARE(profile.destinationOverrides.size(), std::size_t{1});
    QCOMPARE(profile.destinationOverrides.front().relativePath, std::filesystem::path{"Sceneries"});
}

QTEST_APPLESS_MAIN(OrphanOverridesTest)

#include "tst_orphan_overrides.moc"
