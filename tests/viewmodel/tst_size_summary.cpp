#include <QtTest/QtTest>

#include "viewmodel/SizeSummary.h"

namespace
{
    class SizeSummaryTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void NothingSelectedSaysNothing();
        static void ASumThatCoversEverythingSelectedShowsOnlyTheSize();
        static void AnIncompleteSumSaysHowMuchOfTheSelectionItCovers();
        static void TheReasonNamesTheClassificationThatWasNotMeasured();
        static void TwoClassificationsLeftOutAreBothNamed();
    };
}

void SizeSummaryTest::NothingSelectedSaysNothing()
{
    QVERIFY(SizeOfTheSelection(SelectionSize{}).isEmpty());
}

void SizeSummaryTest::ASumThatCoversEverythingSelectedShowsOnlyTheSize()
{
    const SelectionSize size{.bytes = 4096, .measured = 3, .selected = 3};

    QCOMPARE(SizeOfTheSelection(size), AsSize(4096));
}

void SizeSummaryTest::AnIncompleteSumSaysHowMuchOfTheSelectionItCovers()
{
    const SelectionSize size{.bytes = 4096, .measured = 4, .selected = 5};
    const QString line = SizeOfTheSelection(size);

    QVERIFY(line.startsWith(AsSize(4096)));
    QVERIFY2(line.contains(QStringLiteral("4")) && line.contains(QStringLiteral("5")),
             qPrintable(QStringLiteral("the reach is missing from: %1").arg(line)));
}

void SizeSummaryTest::TheReasonNamesTheClassificationThatWasNotMeasured()
{
    const SelectionSize size{
        .bytes = 4096,
        .measured = 4,
        .selected = 5,
        .unmeasured = {UnmeasuredEntries{.classification = EntryClassification::Unavailable, .count = 1}}};

    const QString line = SizeOfTheSelection(size);

    QCOMPARE(line, QStringLiteral("%1 across 4 of 5 selected · 1 unavailable not measured").arg(AsSize(4096)));
}

void SizeSummaryTest::TwoClassificationsLeftOutAreBothNamed()
{
    const SelectionSize size{
        .bytes = 4096,
        .measured = 3,
        .selected = 5,
        .unmeasured = {UnmeasuredEntries{.classification = EntryClassification::Unavailable, .count = 1},
                       UnmeasuredEntries{.classification = EntryClassification::Broken, .count = 1}}};

    const QString line = SizeOfTheSelection(size);

    QVERIFY(line.contains(QStringLiteral("1 unavailable not measured")));
    QVERIFY(line.contains(QStringLiteral("1 broken not measured")));
}

QTEST_MAIN(SizeSummaryTest)

#include "tst_size_summary.moc"
