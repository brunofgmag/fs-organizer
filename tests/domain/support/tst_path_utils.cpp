#include <QtTest/QtTest>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PathUtilsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TwoPathsThatNameTheSameFolderShareAKey();
        static void ARootKeepsItsSeparator();
        static void AnEmptyPathHasAnEmptyKey();
        static void ReparsePrefixesAreStripped();
        static void TheLastComponentIsReadWhicheverSeparatorWroteIt();
        static void AFolderIsInsideTheRootOfItsOwnVolume();
        static void ASiblingWhoseNameStartsWithTheRootIsNotInsideIt();
        static void NothingIsInsideARootThatWasNeverNamed();
        static void TwoSpellingsOfAnAccentedNameShareAKey();
        static void BytesThatAreNotUtf8AreCarriedThrough();
        static void ANameOutsideTheHostCodePageStillBecomesUtf8();
    };
}

void PathUtilsTest::ANameOutsideTheHostCodePageStillBecomesUtf8()
{
    const std::string written = "D:/Library/Sceneries/\xC5\x81odz";

    QCOMPARE(AsUtf8(PathFromUtf8(written)), written);
    QCOMPARE(ComparablePath(PathFromUtf8(written)), std::string("d:/library/sceneries/\xC5\x82odz"));
}

void PathUtilsTest::TwoSpellingsOfAnAccentedNameShareAKey()
{
    const std::string upper = "D:/Library/Sceneries/\xC3\x81"
                              "UDIO-\xC5\x81"
                              "\xC3\x87"
                              "\xCE\xA3"
                              "\xD0\x94";
    const std::string lower = "d:/library/sceneries/\xC3\xA1"
                              "udio-\xC5\x82"
                              "\xC3\xA7"
                              "\xCF\x83"
                              "\xD0\xB4";

    QCOMPARE(ComparablePath(PathFromUtf8(upper)), ComparablePath(PathFromUtf8(lower)));
    QCOMPARE(ComparablePath(PathFromUtf8(upper)), lower);
    QCOMPARE(ComparableFileName(PathFromUtf8(upper)), ComparableFileName(PathFromUtf8(lower)));
    QVERIFY(PathIsInside(PathFromUtf8(upper), "D:/Library"));
}

void PathUtilsTest::BytesThatAreNotUtf8AreCarriedThrough()
{
    QCOMPARE(LoweredForComparison("\xC3"), std::string("\xE3"));
    QCOMPARE(LoweredForComparison("\xFF"
                                  "X"),
             std::string("\xFF"
                         "x"));
    QCOMPARE(LoweredForComparison("\xC3\x28Z"), std::string("\xE3\x28z"));
}

void PathUtilsTest::TwoPathsThatNameTheSameFolderShareAKey()
{
    const std::string expected = "d:/msfs 2024/aircrafts/pmdg-aircraft-77w";

    QCOMPARE(ComparablePath("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"), expected);
    QCOMPARE(ComparablePath("d:/msfs 2024/aircrafts/pmdg-aircraft-77w"), expected);
    QCOMPARE(ComparablePath("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/"), expected);
    QCOMPARE(ComparablePath("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w///"), expected);
    QCOMPARE(ComparablePath(R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)"), expected);
    QCOMPARE(ComparablePath(R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w\)"), expected);
    QCOMPARE(ComparablePath("D:/MSFS 2024/Sceneries/../Aircrafts/pmdg-aircraft-77w"), expected);
}

void PathUtilsTest::ARootKeepsItsSeparator()
{
    QCOMPARE(ComparablePath("D:/"), std::string("d:/"));
    QCOMPARE(ComparablePath(R"(D:\)"), std::string("d:/"));
}

void PathUtilsTest::AnEmptyPathHasAnEmptyKey()
{
    QCOMPARE(ComparablePath({}), std::string());
}

void PathUtilsTest::ReparsePrefixesAreStripped()
{
    const std::filesystem::path expected = "E:/Library/Add-ons/aerosoft-aircraft-a346-pro";

    QCOMPARE(NormalizeReparseTarget(R"(\??\E:\Library\Add-ons\aerosoft-aircraft-a346-pro)"), expected);
    QCOMPARE(NormalizeReparseTarget(R"(\\?\E:\Library\Add-ons\aerosoft-aircraft-a346-pro)"), expected);
    QCOMPARE(NormalizeReparseTarget(R"(\??\UNC\server\share)"),
             std::filesystem::path("//server/share").lexically_normal());
}

void PathUtilsTest::TheLastComponentIsReadWhicheverSeparatorWroteIt()
{
    QCOMPARE(ComparableFileName("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"), std::string("pmdg-aircraft-77w"));
    QCOMPARE(ComparableFileName(R"(D:\MSFS 2024\Aircrafts\PMDG-Aircraft-77W)"), std::string("pmdg-aircraft-77w"));
    QCOMPARE(ComparableFileName("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/"), std::string("pmdg-aircraft-77w"));
    QCOMPARE(ComparableFileName("pmdg-aircraft-77w"), std::string("pmdg-aircraft-77w"));
    QCOMPARE(ComparableFileName({}), std::string());
}

void PathUtilsTest::AFolderIsInsideTheRootOfItsOwnVolume()
{
    QVERIFY(PathIsInside("D:/Library", "D:/"));
    QVERIFY(PathIsInside("D:/Library/Aircrafts/pmdg-aircraft-77w", "D:/"));
    QVERIFY(PathIsInside("D:/", "D:/"));
    QVERIFY(!PathIsInside("E:/Library", "D:/"));
}

void PathUtilsTest::ASiblingWhoseNameStartsWithTheRootIsNotInsideIt()
{
    QVERIFY(!PathIsInside("D:/Library", "D:/Lib"));
    QVERIFY(PathIsInside("D:/Lib/aerosoft-crj", "D:/Lib"));
}

void PathUtilsTest::NothingIsInsideARootThatWasNeverNamed()
{
    QVERIFY(!PathIsInside("D:/Library/Aircrafts/aerosoft-crj", {}));
    QVERIFY(!PathIsInside({}, "D:/Library"));
    QVERIFY(PathIsInside({}, {}));
}

QTEST_APPLESS_MAIN(PathUtilsTest)

#include "tst_path_utils.moc"
