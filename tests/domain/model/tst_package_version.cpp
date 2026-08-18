#include <QtTest/QtTest>

#include "domain/model/PackageVersion.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class PackageVersionTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheComparisonIsByNumberSoTwentySixBeatsNine();
        static void AMissingComponentCountsAsZeroInsteadOfMakingTheVersionsDiffer();
        static void AVersionWithNoNumberInItLeavesTheAnswerToSomethingElse();
        static void TheOfferStandsUnlessTheDestinationCopyIsOlder();
    };
}

void PackageVersionTest::TheComparisonIsByNumberSoTwentySixBeatsNine()
{
    QCOMPARE(HowTheVersionCompares("2.26.16", "2.9.1"), VersionOrder::Newer);
    QCOMPARE(HowTheVersionCompares("2.9.1", "2.26.16"), VersionOrder::Older);
    QCOMPARE(HowTheVersionCompares("2.26.16", "2.26.16"), VersionOrder::TheSame);
}

void PackageVersionTest::AMissingComponentCountsAsZeroInsteadOfMakingTheVersionsDiffer()
{
    QCOMPARE(HowTheVersionCompares("2.26", "2.26.0"), VersionOrder::TheSame);
    QCOMPARE(HowTheVersionCompares("2.26.1", "2.26"), VersionOrder::Newer);
    QCOMPARE(HowTheVersionCompares("0.1.0", "0.1.0"), VersionOrder::TheSame);
}

void PackageVersionTest::AVersionWithNoNumberInItLeavesTheAnswerToSomethingElse()
{
    QCOMPARE(HowTheVersionCompares("", "0.1.0"), VersionOrder::NoOneCanTell);
    QCOMPARE(HowTheVersionCompares("0.1.0", ""), VersionOrder::NoOneCanTell);
    QCOMPARE(HowTheVersionCompares("release", "0.1.0"), VersionOrder::NoOneCanTell);
    QCOMPARE(HowTheVersionCompares("v2.1", "v2.0"), VersionOrder::Newer);
}

void PackageVersionTest::TheOfferStandsUnlessTheDestinationCopyIsOlder()
{
    QVERIFY(TakingItBackIsWorthOffering("2.26.16", "2.9.1"));
    QVERIFY2(TakingItBackIsWorthOffering("0.1.0", "0.1.0"),
             "a package rewritten every cycle under a fixed version still gets the offer, said so in 2026-08-18");
    QVERIFY2(TakingItBackIsWorthOffering("", ""),
             "two manifests that name no version tell the copies apart no better than two equal ones");
    QVERIFY2(!TakingItBackIsWorthOffering("2.9.1", "2.26.16"),
             "an older copy in the destination is not worth carrying over the newer one in the library");
}

QTEST_APPLESS_MAIN(PackageVersionTest)

#include "tst_package_version.moc"
