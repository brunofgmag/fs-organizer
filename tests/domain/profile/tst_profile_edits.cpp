#include <QtTest/QtTest>

#include "domain/profile/ProfileEdits.h"
#include "tests/support/PathPrinting.h"

class ProfileEditsTest : public QObject
{
    Q_OBJECT

private slots:
    static void AProfileIsRemovedFromTheListByItsIdentifier();
    static void TheLastProfileIsNeverRemoved();
    static void AProfileThatIsNotThereRemovesNothing();
    static void UnregisteringALibraryDropsItAndTheOverridesThatNamedIt();
    static void UnregisteringALibraryLeavesTheOverridesOfTheOthersAlone();
    static void UnregisteringALibraryThatIsNotThereChangesNothing();
    static void RepointingADestinationCarriesTheOverridesThatNamedTheOldPath();
    static void RepointingADestinationCarriesTheDefaultWhenItWasTheOneMoved();
    static void RepointingADestinationLeavesTheOtherDestinationAlone();
    static void RepointingMatchesTheOldPathWithoutCaseOrSeparatorDifferences();
};

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kCommunity2024 = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kMoved = "F:/Flight Simulator 2024/Community2024";

    SimulatorProfile ProfileWithTwoLibraries()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.destinations = {kCommunity, kCommunity2024};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", "D:/MSFS 2024", "MSFS 2024"},
                             Library{"library-2", "F:/Extra Addons", "Extra Addons"}};
        profile.destinationOverrides = {DestinationOverride{"library-1", "Aircrafts", kCommunity2024},
                                        DestinationOverride{"library-2", "Sceneries", kCommunity2024}};

        return profile;
    }
}

void ProfileEditsTest::UnregisteringALibraryDropsItAndTheOverridesThatNamedIt()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    UnregisterLibrary(profile, "library-1");

    QCOMPARE(profile.libraries.size(), std::size_t{1});
    QCOMPARE(profile.libraries.front().id, std::string("library-2"));
    QCOMPARE(profile.destinationOverrides.size(), std::size_t{1});
    QCOMPARE(profile.destinationOverrides.front().libraryId, std::string("library-2"));
}

void ProfileEditsTest::UnregisteringALibraryLeavesTheOverridesOfTheOthersAlone()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    UnregisterLibrary(profile, "library-2");

    QCOMPARE(profile.destinationOverrides.size(), std::size_t{1});
    QCOMPARE(profile.destinationOverrides.front().libraryId, std::string("library-1"));
    QCOMPARE(profile.destinationOverrides.front().relativePath, std::filesystem::path("Aircrafts"));
}

void ProfileEditsTest::UnregisteringALibraryThatIsNotThereChangesNothing()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    UnregisterLibrary(profile, "library-9");

    QCOMPARE(profile.libraries.size(), std::size_t{2});
    QCOMPARE(profile.destinationOverrides.size(), std::size_t{2});
}

void ProfileEditsTest::RepointingADestinationCarriesTheOverridesThatNamedTheOldPath()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    RepointDestination(profile, kCommunity2024, kMoved);

    QCOMPARE(profile.destinations.size(), std::size_t{2});
    QCOMPARE(profile.destinations[1], std::filesystem::path(kMoved));
    QCOMPARE(profile.destinationOverrides[0].destination, std::filesystem::path(kMoved));
    QCOMPARE(profile.destinationOverrides[1].destination, std::filesystem::path(kMoved));
}

void ProfileEditsTest::RepointingADestinationCarriesTheDefaultWhenItWasTheOneMoved()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    RepointDestination(profile, kCommunity, "F:/Flight Simulator 2024/Community");

    QCOMPARE(profile.defaultDestination, std::filesystem::path("F:/Flight Simulator 2024/Community"));
    QCOMPARE(profile.destinations[0], std::filesystem::path("F:/Flight Simulator 2024/Community"));
}

void ProfileEditsTest::RepointingADestinationLeavesTheOtherDestinationAlone()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    RepointDestination(profile, kCommunity2024, kMoved);

    QCOMPARE(profile.destinations[0], std::filesystem::path(kCommunity));
    QCOMPARE(profile.defaultDestination, std::filesystem::path(kCommunity));
}

void ProfileEditsTest::RepointingMatchesTheOldPathWithoutCaseOrSeparatorDifferences()
{
    SimulatorProfile profile = ProfileWithTwoLibraries();

    RepointDestination(profile, R"(e:\FLIGHT SIMULATOR 2024\community2024\)", kMoved);

    QCOMPARE(profile.destinations[1], std::filesystem::path(kMoved));
    QCOMPARE(profile.destinationOverrides[0].destination, std::filesystem::path(kMoved));
}

void ProfileEditsTest::AProfileIsRemovedFromTheListByItsIdentifier()
{
    std::vector<SimulatorProfile> profiles(2);
    profiles[0].id = "msfs2024";
    profiles[1].id = "msfs2020";

    QVERIFY(RemoveProfile(profiles, "msfs2024"));
    QCOMPARE(profiles.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(profiles.front().id), QStringLiteral("msfs2020"));
}

void ProfileEditsTest::TheLastProfileIsNeverRemoved()
{
    std::vector<SimulatorProfile> profiles(1);
    profiles[0].id = "msfs2024";

    QVERIFY(!RemoveProfile(profiles, "msfs2024"));
    QCOMPARE(profiles.size(), std::size_t{1});
}

void ProfileEditsTest::AProfileThatIsNotThereRemovesNothing()
{
    std::vector<SimulatorProfile> profiles(2);
    profiles[0].id = "msfs2024";
    profiles[1].id = "msfs2020";

    QVERIFY(!RemoveProfile(profiles, "msfs2019"));
    QCOMPARE(profiles.size(), std::size_t{2});
}

QTEST_APPLESS_MAIN(ProfileEditsTest)

#include "tst_profile_edits.moc"
