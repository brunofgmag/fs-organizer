#include <QtTest/QtTest>

#include <array>

#include "domain/support/CaseFolding.h"
#include "domain/support/StringUtils.h"

namespace
{
    class StringUtilsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void PlainLettersIgnoreCase();
        static void AnAccentedNameIgnoresCaseTheWayPathsDo();
        static void GreekAndCyrillicLettersIgnoreCase();
        static void DifferentLettersAndDifferentLengthsAreNotEqual();
        static void TwoEmptyStringsAreEqual();
        static void BytesThatAreNotUtf8DoNotBreakTheComparison();
        static void ItAgreesWithTheKeyThePathsCompareBy();
    };
}

void StringUtilsTest::PlainLettersIgnoreCase()
{
    QVERIFY(EqualsIgnoringCase("SimBridge", "simbridge"));
    QVERIFY(EqualsIgnoringCase("lib-1", "LIB-1"));
    QVERIFY(!EqualsIgnoringCase("simbridge", "simbridge2"));
}

void StringUtilsTest::AnAccentedNameIgnoresCaseTheWayPathsDo()
{
    const std::string upper = "\xC3\x81rvore";
    const std::string lower = "\xC3\xA1rvore";

    QVERIFY(EqualsIgnoringCase(upper, lower));
    QVERIFY(EqualsIgnoringCase(lower, upper));
    QVERIFY(!EqualsIgnoringCase(upper, "arvore"));
}

void StringUtilsTest::GreekAndCyrillicLettersIgnoreCase()
{
    QVERIFY(EqualsIgnoringCase("\xCE\xA3", "\xCF\x83"));
    QVERIFY(EqualsIgnoringCase("\xD0\x94-addon", "\xD0\xB4-ADDON"));
    QVERIFY(EqualsIgnoringCase("\xC5\x81odz", "\xC5\x82odz"));
}

void StringUtilsTest::DifferentLettersAndDifferentLengthsAreNotEqual()
{
    QVERIFY(!EqualsIgnoringCase("\xC3\x81rvore", "\xC3\x81rvores"));
    QVERIFY(!EqualsIgnoringCase("\xC3\x81", "\xC3\x89"));
    QVERIFY(!EqualsIgnoringCase("abc", "ab"));
    QVERIFY(!EqualsIgnoringCase("", "a"));
}

void StringUtilsTest::TwoEmptyStringsAreEqual()
{
    QVERIFY(EqualsIgnoringCase("", ""));
}

void StringUtilsTest::BytesThatAreNotUtf8DoNotBreakTheComparison()
{
    QVERIFY(EqualsIgnoringCase("\xC3", "\xE3"));
    QVERIFY(EqualsIgnoringCase("\xC3\x28Z", "\xE3\x28z"));
    QVERIFY(!EqualsIgnoringCase("\xE2\x82", "\xE2"));
    QVERIFY(EqualsIgnoringCase("\xFF"
                               "X",
                               "\xFF"
                               "x"));
}

void StringUtilsTest::ItAgreesWithTheKeyThePathsCompareBy()
{
    const std::array<std::string, 9> samples{"\xC3\x81rvore", "\xC3\xA1rvore", "arvore",           "\xC3",     "\xE3",
                                             "\xC3\xA3",      "\xE2\x82",      "\xF0\x9F\x98\x80", "\xCE\xA3x"};

    for (const std::string& left : samples)
    {
        for (const std::string& right : samples)
        {
            QCOMPARE(EqualsIgnoringCase(left, right), LoweredForComparison(left) == LoweredForComparison(right));
        }
    }
}

QTEST_APPLESS_MAIN(StringUtilsTest)

#include "tst_string_utils.moc"
