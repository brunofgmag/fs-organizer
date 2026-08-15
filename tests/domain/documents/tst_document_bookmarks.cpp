#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "domain/documents/DocumentBookmarks.h"

namespace
{
    class DocumentBookmarksTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSectionThatStartedLastBeforeThePageIsTheOneHoldingIt();
        static void ThePageASectionStartsOnBelongsToThatSection();
        static void TheSubsectionWinsOverTheChapterThatCarriesIt();
        static void WhenSeveralSectionsStartOnOnePageTheFirstOfThemHoldsIt();
        static void APageAheadOfEverySectionIsHeldByNone();
        static void ADocumentWithoutAnOutlineHoldsNoPage();
    };

    const std::vector<DocumentSection> kManual = {{.title = "Introduction", .page = 0},
                                                  {.title = "Systems", .page = 10},
                                                  {.title = "Hydraulics", .page = 12},
                                                  {.title = "Limitations", .page = 20}};

    void DocumentBookmarksTest::TheSectionThatStartedLastBeforeThePageIsTheOneHoldingIt()
    {
        QCOMPARE(TheSectionHolding(kManual, 25), std::optional<std::size_t>{3});
    }

    void DocumentBookmarksTest::ThePageASectionStartsOnBelongsToThatSection()
    {
        QCOMPARE(TheSectionHolding(kManual, 20), std::optional<std::size_t>{3});
    }

    void DocumentBookmarksTest::TheSubsectionWinsOverTheChapterThatCarriesIt()
    {
        QCOMPARE(TheSectionHolding(kManual, 13), std::optional<std::size_t>{2});
    }

    void DocumentBookmarksTest::WhenSeveralSectionsStartOnOnePageTheFirstOfThemHoldsIt()
    {
        const std::vector<DocumentSection> aSpread = {{.title = "Starting Off", .page = 10},
                                                      {.title = "System Requirements", .page = 10},
                                                      {.title = "Support", .page = 10},
                                                      {.title = "Copyrights", .page = 10},
                                                      {.title = "The Airport", .page = 11}};

        QVERIFY2(TheSectionHolding(aSpread, 10) == std::optional<std::size_t>{0},
                 "a manual laid out two pages up carries several headings on one page of the file, and what the "
                 "reader sees at the top of it is the first, not the copyright notice at the foot of the right "
                 "column");
    }

    void DocumentBookmarksTest::APageAheadOfEverySectionIsHeldByNone()
    {
        const std::vector<DocumentSection> afterACover = {{.title = "Systems", .page = 4}};

        QCOMPARE(TheSectionHolding(afterACover, 1), std::nullopt);
    }

    void DocumentBookmarksTest::ADocumentWithoutAnOutlineHoldsNoPage()
    {
        QCOMPARE(TheSectionHolding({}, 30), std::nullopt);
    }

}

QTEST_APPLESS_MAIN(DocumentBookmarksTest)

#include "tst_document_bookmarks.moc"
