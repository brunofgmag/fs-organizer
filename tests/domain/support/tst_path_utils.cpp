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
        static void TheRootOfAPathIsReadFromItsTextWhicheverSystemRunsTheTest();
        static void AnEmptyPathHasAnEmptyKey();
        static void ReparsePrefixesAreStripped();
        static void TheLastComponentIsReadWhicheverSeparatorWroteIt();
        static void TheParentIsReadWhicheverSeparatorWroteTheChild();
        static void TheParentOfAFolderAtTheRootIsTheRootAndTheRootHasNoParent();
        static void AFolderIsInsideTheRootOfItsOwnVolume();
        static void ASiblingWhoseNameStartsWithTheRootIsNotInsideIt();
        static void NothingIsInsideARootThatWasNeverNamed();
        static void TwoSpellingsOfAnAccentedNameShareAKey();
        static void BytesThatAreNotUtf8AreCarriedThrough();
        static void ANameOutsideTheHostCodePageStillBecomesUtf8();
    };
}

void PathUtilsTest::TheRootOfAPathIsReadFromItsTextWhicheverSystemRunsTheTest()
{
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8("D:/Library/Sceneries"))), std::string("D:/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8(R"(D:\Library\Sceneries)"))), std::string("D:/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8("D:"))), std::string("D:/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8(R"(\\nas\share\Library)"))), std::string("//nas/share/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8("//nas/share"))), std::string("//nas/share/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8(R"(\\?\D:\Library)"))), std::string("//?/D:/"));
    QCOMPARE(AsUtf8(RootOf(PathFromUtf8("/home/pilot/Library"))), std::string("/"));
    QVERIFY(RootOf(PathFromUtf8("Library/Sceneries")).empty());
    QVERIFY(RootOf(std::filesystem::path{}).empty());
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

void PathUtilsTest::TheParentIsReadWhicheverSeparatorWroteTheChild()
{
    const std::string expected = "e:/flight simulator 2024/community";

    QCOMPARE(ComparablePath(ParentOf("E:/Flight Simulator 2024/Community/aerosoft-crj")), expected);
    QCOMPARE(ComparablePath(ParentOf(PathFromUtf8(R"(E:\Flight Simulator 2024\community\aerosoft-crj)"))), expected);
    QCOMPARE(AsUtf8(ParentOf(PathFromUtf8(R"(E:\Flight Simulator 2024\community\aerosoft-crj)"))),
             std::string(R"(E:\Flight Simulator 2024\community)"));
    QCOMPARE(AsUtf8(ParentOf("/home/pilot/Community/aerosoft-crj")), std::string("/home/pilot/Community"));
    QVERIFY(ParentOf("aerosoft-crj").empty());
    QVERIFY(ParentOf({}).empty());
}

void PathUtilsTest::TheParentOfAFolderAtTheRootIsTheRootAndTheRootHasNoParent()
{
    QCOMPARE(AsUtf8(ParentOf(PathFromUtf8(R"(E:\Community)"))), std::string(R"(E:\)"));
    QCOMPARE(AsUtf8(ParentOf("E:/Community")), std::string("E:/"));
    QCOMPARE(AsUtf8(ParentOf("/Community")), std::string("/"));
    QCOMPARE(AsUtf8(ParentOf(PathFromUtf8(R"(\\nas\share\Community)"))), std::string(R"(\\nas\share\)"));
    QVERIFY(ParentOf("E:/").empty());
    QVERIFY(ParentOf("/").empty());
    QVERIFY(ParentOf(PathFromUtf8(R"(\\nas\share)")).empty());
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
