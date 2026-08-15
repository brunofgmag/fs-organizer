#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

#include "domain/documents/ChartRevisions.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ChartRevisionsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APageCarriedByOneFileAloneNeverProducesAPreviousRevision();
        static void TheFileOfTheHighestVersionIsTheOneInForceAndTheOtherIsThePreviousRevision();
        static void ThePageCountInForceIsTheNumberOfDistinctPagesAndNotTheNumberOfFiles();
        static void BothLinesComeOutInPageOrder();
        static void APageWithThreeRevisionsKeepsOneInForceAndSendsTwoBack();
        static void WithNoVersionToCompareTheFirstFileGivenStaysInForce();
    };

    [[nodiscard]] PageRevision PageOf(const int page, const std::string& file, const long long version)
    {
        return {.page = page, .file = PathFromUtf8(file), .version = version};
    }

    void ChartRevisionsTest::APageCarriedByOneFileAloneNeverProducesAPreviousRevision()
    {
        const WhichRevisionOfEachPage chosen =
            TheRevisionInForceOfEachPage({PageOf(1, "1.pdf", 1486375), PageOf(2, "2.pdf", 1486376)});

        QCOMPARE(chosen.inForce.size(), std::size_t{2});
        QVERIFY(chosen.previous.empty());
    }

    void ChartRevisionsTest::TheFileOfTheHighestVersionIsTheOneInForceAndTheOtherIsThePreviousRevision()
    {
        const WhichRevisionOfEachPage chosen =
            TheRevisionInForceOfEachPage({PageOf(15, "december.pdf", 1473008), PageOf(15, "january.pdf", 1486381)});

        QCOMPARE(chosen.inForce.size(), std::size_t{1});
        QCOMPARE(chosen.inForce.front().file, PathFromUtf8("january.pdf"));
        QCOMPARE(chosen.previous.size(), std::size_t{1});
        QCOMPARE(chosen.previous.front().file, PathFromUtf8("december.pdf"));
    }

    void ChartRevisionsTest::ThePageCountInForceIsTheNumberOfDistinctPagesAndNotTheNumberOfFiles()
    {
        const WhichRevisionOfEachPage chosen =
            TheRevisionInForceOfEachPage({PageOf(1, "a.pdf", 1426565), PageOf(1, "b.pdf", 1486375),
                                          PageOf(2, "c.pdf", 1426566), PageOf(2, "d.pdf", 1486376)});

        QCOMPARE(chosen.inForce.size(), std::size_t{2});
        QCOMPARE(chosen.previous.size(), std::size_t{2});
    }

    void ChartRevisionsTest::BothLinesComeOutInPageOrder()
    {
        const WhichRevisionOfEachPage chosen =
            TheRevisionInForceOfEachPage({PageOf(3, "three-old.pdf", 1426567), PageOf(1, "one-new.pdf", 1486375),
                                          PageOf(3, "three-new.pdf", 1486377), PageOf(1, "one-old.pdf", 1426565)});

        QCOMPARE(chosen.inForce.size(), std::size_t{2});
        QCOMPARE(chosen.inForce.front().file, PathFromUtf8("one-new.pdf"));
        QCOMPARE(chosen.inForce.back().file, PathFromUtf8("three-new.pdf"));
        QCOMPARE(chosen.previous.size(), std::size_t{2});
        QCOMPARE(chosen.previous.front().file, PathFromUtf8("one-old.pdf"));
        QCOMPARE(chosen.previous.back().file, PathFromUtf8("three-old.pdf"));
    }

    void ChartRevisionsTest::APageWithThreeRevisionsKeepsOneInForceAndSendsTwoBack()
    {
        const WhichRevisionOfEachPage chosen = TheRevisionInForceOfEachPage(
            {PageOf(15, "oldest.pdf", 1426565), PageOf(15, "newest.pdf", 1486381), PageOf(15, "middle.pdf", 1473008)});

        QCOMPARE(chosen.inForce.size(), std::size_t{1});
        QCOMPARE(chosen.inForce.front().file, PathFromUtf8("newest.pdf"));
        QCOMPARE(chosen.previous.size(), std::size_t{2});
        QCOMPARE(chosen.previous.front().file, PathFromUtf8("middle.pdf"));
        QCOMPARE(chosen.previous.back().file, PathFromUtf8("oldest.pdf"));
    }

    void ChartRevisionsTest::WithNoVersionToCompareTheFirstFileGivenStaysInForce()
    {
        const WhichRevisionOfEachPage chosen =
            TheRevisionInForceOfEachPage({PageOf(15, "first.pdf", 0), PageOf(15, "second.pdf", 0)});

        QCOMPARE(chosen.inForce.size(), std::size_t{1});
        QCOMPARE(chosen.inForce.front().file, PathFromUtf8("first.pdf"));
        QCOMPARE(chosen.previous.size(), std::size_t{1});
        QCOMPARE(chosen.previous.front().file, PathFromUtf8("second.pdf"));
    }
}

QTEST_APPLESS_MAIN(ChartRevisionsTest)

#include "tst_chart_revisions.moc"
