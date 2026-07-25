#include <QtTest/QtTest>

#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

class PathUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    static void TwoPathsThatNameTheSameFolderShareAKey();
    static void ARootKeepsItsSeparator();
    static void AnEmptyPathHasAnEmptyKey();
    static void ReparsePrefixesAreStripped();
};

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
    const std::filesystem::path expected = R"(E:\Library\Add-ons\aerosoft-aircraft-a346-pro)";

    QCOMPARE(NormalizeReparseTarget(R"(\??\E:\Library\Add-ons\aerosoft-aircraft-a346-pro)"), expected);
    QCOMPARE(NormalizeReparseTarget(R"(\\?\E:\Library\Add-ons\aerosoft-aircraft-a346-pro)"), expected);
    QCOMPARE(NormalizeReparseTarget(R"(\??\UNC\server\share)"), std::filesystem::path(R"(\\server\share)"));
}

QTEST_APPLESS_MAIN(PathUtilsTest)

#include "tst_path_utils.moc"
