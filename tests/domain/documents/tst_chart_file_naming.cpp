#include <QtTest/QtTest>

#include "domain/documents/ChartFileNaming.h"
#include "domain/support/PathUtils.h"

namespace
{
    class ChartFileNamingTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheTokenOfAVisualApproachChartNamesAChartAndTheNameCarriesTheCode();
        static void TheTokenOfAHeliportChartNamesAChart();
        static void TheTokenOfTheAipSectionNamesADocumentAndNotAChart();
        static void TheTokenIsReadWhateverTheCaseItIsSpelledIn();
        static void ATokenTheAppDoesNotKnowLeavesTheTypeEmptyAndSaysNothingAboutTheKind();
        static void ACodeIsStillReadBesideATokenTheAppDoesNotKnow();
        static void ACodeCanCarryADigitBecauseTheBglAnswersLF1HForRennes();
        static void AFileNameWithNoSeparatorSaysNothing();
        static void APartThatIsNotFourCharactersLongIsNotACode();
        static void TheCodeComesOutInCapitalsSoTwoSpellingsDoNotSplitTheGroup();
    };

    void ChartFileNamingTest::TheTokenOfAVisualApproachChartNamesAChartAndTheNameCarriesTheCode()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD2_LFST_VAC.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString("VAC"));
        QCOMPARE(QString::fromStdString(said.code), QString("LFST"));
        QVERIFY(!said.namesADocument);
    }

    void ChartFileNamingTest::TheTokenOfAHeliportChartNamesAChart()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD3_LFWH_HELI.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString("HELI"));
        QCOMPARE(QString::fromStdString(said.code), QString("LFWH"));
        QVERIFY(!said.namesADocument);
    }

    void ChartFileNamingTest::TheTokenOfTheAipSectionNamesADocumentAndNotAChart()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD2_LFRN_APT.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString("APT"));
        QCOMPARE(QString::fromStdString(said.code), QString("LFRN"));
        QVERIFY(said.namesADocument);
    }

    void ChartFileNamingTest::TheTokenIsReadWhateverTheCaseItIsSpelledIn()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/ad2_lfst_vac.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString("VAC"));
        QVERIFY(!said.namesADocument);
    }

    void ChartFileNamingTest::ATokenTheAppDoesNotKnowLeavesTheTypeEmptyAndSaysNothingAboutTheKind()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD2_LFST_XYZ.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString());
        QVERIFY(!said.namesADocument);
    }

    void ChartFileNamingTest::ACodeIsStillReadBesideATokenTheAppDoesNotKnow()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD2_LFST_XYZ.pdf"));

        QCOMPARE(QString::fromStdString(said.code), QString("LFST"));
    }

    void ChartFileNamingTest::ACodeCanCarryADigitBecauseTheBglAnswersLF1HForRennes()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/AD2_LF1H_VAC.pdf"));

        QCOMPARE(QString::fromStdString(said.code), QString("LF1H"));
    }

    void ChartFileNamingTest::AFileNameWithNoSeparatorSaysNothing()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("NavDataPro/EBBR/53105.pdf"));

        QCOMPARE(QString::fromStdString(said.type), QString());
        QCOMPARE(QString::fromStdString(said.code), QString());
        QVERIFY(!said.namesADocument);
    }

    void ChartFileNamingTest::APartThatIsNotFourCharactersLongIsNotACode()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Manual_MegaAirport.pdf"));

        QCOMPARE(QString::fromStdString(said.code), QString());
        QCOMPARE(QString::fromStdString(said.type), QString());
    }

    void ChartFileNamingTest::TheCodeComesOutInCapitalsSoTwoSpellingsDoNotSplitTheGroup()
    {
        const WhatTheFileNameSays said = ReadTheChartFileName(PathFromUtf8("Charts/ad2_lfst_vac.pdf"));

        QCOMPARE(QString::fromStdString(said.code), QString("LFST"));
    }
}

QTEST_APPLESS_MAIN(ChartFileNamingTest)

#include "tst_chart_file_naming.moc"
