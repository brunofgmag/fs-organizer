#include <QtTest/QtTest>

#include "domain/profile/OrphanOverrides.h"
#include "domain/tree/EffectiveDestination.h"
#include "tests/support/PathPrinting.h"

namespace
{
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
}

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
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }
}

void OrphanOverridesTest::AnOverridePointingAtADestinationOfTheProfileIsNotAnOrphan()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kOtherDestination}};

    QVERIFY(OverridesPointingNowhere(profile).empty());
}

void OrphanOverridesTest::AnOverridePointingOutsideTheProfileIsReported()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kGoneDestination},
        DestinationOverride{.libraryId = "library-1", .relativePath = "Sceneries", .destination = kOtherDestination}};

    const std::vector<DestinationOverride> orphans = OverridesPointingNowhere(profile);

    QCOMPARE(orphans.size(), std::size_t{1});
    QCOMPARE(orphans.front().relativePath, std::filesystem::path{"Aircrafts"});
    QCOMPARE(orphans.front().destination, std::filesystem::path{kGoneDestination});
}

void OrphanOverridesTest::ADestinationIsRecognisedWhateverTheCaseAndTheSeparator()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {DestinationOverride{.libraryId = "library-1",
                                                        .relativePath = "Aircrafts",
                                                        .destination = R"(e:\flight simulator 2024\community2024\)"}};

    QVERIFY(OverridesPointingNowhere(profile).empty());
}

void OrphanOverridesTest::AnOrphanOverrideDoesNotDecideWhereTheAddonGoes()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kGoneDestination}};

    QCOMPARE(EffectiveDestination(profile, "D:/MSFS 2024/Aircrafts/pmdg-777"), std::filesystem::path{kCommunity});
}

void OrphanOverridesTest::DroppingTheOrphansKeepsTheOnesThatStillPoint()
{
    SimulatorProfile profile = Profile();
    profile.destinationOverrides = {
        DestinationOverride{.libraryId = "library-1", .relativePath = "Aircrafts", .destination = kGoneDestination},
        DestinationOverride{.libraryId = "library-1", .relativePath = "Sceneries", .destination = kOtherDestination}};

    DropOverridesPointingNowhere(profile);

    QCOMPARE(profile.destinationOverrides.size(), std::size_t{1});
    QCOMPARE(profile.destinationOverrides.front().relativePath, std::filesystem::path{"Sceneries"});
}

QTEST_APPLESS_MAIN(OrphanOverridesTest)

#include "tst_orphan_overrides.moc"
