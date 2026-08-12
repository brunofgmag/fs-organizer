#include <QtTest/QtTest>

#include <string>
#include <vector>

#include "domain/documents/DocumentClassification.h"
#include "domain/support/PathUtils.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class DocumentClassificationTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APdfUnderAFolderNamedAfterACodeTheAddonCarriesIsAChart();
        static void TheCodeThatComesOutIsTheOneTheAddonCarriesAndNotTheSpellingOfTheFolder();
        static void APdfUnderAFolderNamedChartsIsAChartEvenWithNoCodeInTheName();
        static void APdfUnderAFolderNamedDocsIsADocumentBecauseFourCapitalsAreNotACode();
        static void APdfAtTheRootOfTheAddonIsADocument();
        static void AnAddonWhoseCodeWasNeverExtractedHasNoChartByCode();
        static void AnAddonWhoseCodeWasNeverExtractedStillHasTheChartsClause();
    };

    const std::vector<std::string> kBrussels = {"EBBR"};
    const std::vector<std::string> kNothingWasExtracted = {};

    void DocumentClassificationTest::APdfUnderAFolderNamedAfterACodeTheAddonCarriesIsAChart()
    {
        const ClassifiedDocument classified = ClassifyDocument(PathFromUtf8("NavDataPro/EBBR/53105.pdf"), kBrussels);

        QCOMPARE(classified.kind, DocumentKind::Chart);
        QCOMPARE(QString::fromStdString(classified.code), QString("EBBR"));
    }

    void DocumentClassificationTest::TheCodeThatComesOutIsTheOneTheAddonCarriesAndNotTheSpellingOfTheFolder()
    {
        const ClassifiedDocument classified = ClassifyDocument(PathFromUtf8("NavDataPro/ebbr/53105.pdf"), kBrussels);

        QCOMPARE(classified.kind, DocumentKind::Chart);
        QCOMPARE(QString::fromStdString(classified.code), QString("EBBR"));
    }

    void DocumentClassificationTest::APdfUnderAFolderNamedChartsIsAChartEvenWithNoCodeInTheName()
    {
        const ClassifiedDocument classified = ClassifyDocument(PathFromUtf8("Charts/approach.pdf"), kBrussels);

        QCOMPARE(classified.kind, DocumentKind::Chart);
        QCOMPARE(QString::fromStdString(classified.code), QString());
    }

    void DocumentClassificationTest::APdfUnderAFolderNamedDocsIsADocumentBecauseFourCapitalsAreNotACode()
    {
        const ClassifiedDocument classified = ClassifyDocument(PathFromUtf8("DOCS/handbook.pdf"), kBrussels);

        QCOMPARE(classified.kind, DocumentKind::Document);
        QCOMPARE(QString::fromStdString(classified.code), QString());
    }

    void DocumentClassificationTest::APdfAtTheRootOfTheAddonIsADocument()
    {
        const ClassifiedDocument classified = ClassifyDocument(PathFromUtf8("Manual_MegaAirport.pdf"), kBrussels);

        QCOMPARE(classified.kind, DocumentKind::Document);
    }

    void DocumentClassificationTest::AnAddonWhoseCodeWasNeverExtractedHasNoChartByCode()
    {
        const ClassifiedDocument classified =
            ClassifyDocument(PathFromUtf8("NavDataPro/EBBR/53105.pdf"), kNothingWasExtracted);

        QCOMPARE(classified.kind, DocumentKind::Document);
    }

    void DocumentClassificationTest::AnAddonWhoseCodeWasNeverExtractedStillHasTheChartsClause()
    {
        const ClassifiedDocument classified =
            ClassifyDocument(PathFromUtf8("Charts/approach.pdf"), kNothingWasExtracted);

        QCOMPARE(classified.kind, DocumentKind::Chart);
    }
}

QTEST_APPLESS_MAIN(DocumentClassificationTest)

#include "tst_document_classification.moc"
