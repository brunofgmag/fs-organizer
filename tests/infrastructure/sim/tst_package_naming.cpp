#include <QtTest/QtTest>

#include <string>

#include "infrastructure/sim/PackageNaming.h"

namespace
{
    class PackageNamingTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheGenerationPrefixSeparatesWhatTheSimulatorShipsFromWhatTheAppManages();
        static void TheCodeComesOutOfTheNameInBothLengthsTheListUses();
        static void ANameThatCarriesNoCodeCarriesNoCode();
        static void TheCodeIsReadInUpperCaseBecauseThatIsWhatTheSceneryReaderAnswers();
    };
}

void PackageNamingTest::TheGenerationPrefixSeparatesWhatTheSimulatorShipsFromWhatTheAppManages()
{
    QVERIFY(ItIsContentTheSimulatorShips("fs24-asobo-airport-eham-amsterdam"));
    QVERIFY(ItIsContentTheSimulatorShips("fs20-flyt-airport-sbje-jericoacoara"));

    QVERIFY2(!ItIsContentTheSimulatorShips("communityfs24-aaa-simaddons-animals"),
             "a community entry is a folder the app already reaches by link, and warning that the simulator ships it "
             "would be the app warning about the user's own addon");
    QVERIFY(!ItIsContentTheSimulatorShips("communityfs20-xmd11_light_mod_fs24"));
    QVERIFY(!ItIsContentTheSimulatorShips("unprefixed-legacy-package"));
}

void PackageNamingTest::TheCodeComesOutOfTheNameInBothLengthsTheListUses()
{
    QCOMPARE(AirportCodeInAPackageName("fs24-asobo-airport-eham-amsterdam"), std::string("EHAM"));
    QCOMPARE(AirportCodeInAPackageName("fs24-microsoft-airport-tncm-princess-juliana"), std::string("TNCM"));
    QCOMPARE(AirportCodeInAPackageName("fs20-pauloricardo-airport-sbsp"), std::string("SBSP"));
    QCOMPARE(AirportCodeInAPackageName("fs24-asobo-airport-c53-lowerloon"), std::string("C53"));
    QCOMPARE(AirportCodeInAPackageName("fs24-microsoft-airport-n15-kingston"), std::string("N15"));
}

void PackageNamingTest::ANameThatCarriesNoCodeCarriesNoCode()
{
    QVERIFY(AirportCodeInAPackageName("fs24-asobo-vcockpits-core").empty());
    QVERIFY2(AirportCodeInAPackageName("fs20-microsoft-airport-voloport").empty(),
             "the segment after the marker is a word and not a code, and eight characters is not an identifier");
    QVERIFY(AirportCodeInAPackageName("fs24-asobo-airport-").empty());
    QVERIFY(AirportCodeInAPackageName("fs24-asobo-activities").empty());
}

void PackageNamingTest::TheCodeIsReadInUpperCaseBecauseThatIsWhatTheSceneryReaderAnswers()
{
    const std::string code = AirportCodeInAPackageName("fs24-asobo-airport-eham-amsterdam");

    QVERIFY2(code == std::string("EHAM"),
             "the two sides of the match are the code of a scenery record and the code of a name, and the record "
             "always answers upper case, so the name is the side that has to be lifted");
    QVERIFY2(AirportCodeInAPackageName("fs24-sbsp-airport-eham-amsterdam") == code,
             "a code that appears before the marker is part of the vendor's name, not the airport this covers");
}

QTEST_APPLESS_MAIN(PackageNamingTest)

#include "tst_package_naming.moc"
