#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <vector>

#include "tests/support/ButtonLookup.h"
#include "tests/support/FixedHeadings.h"
#include "view/theme/ModernistTheme.h"
#include "view/library/CoverageDialog.h"

namespace
{
    class CoverageDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheAirportIsNamedWithBothSidesOfTheOverlap();
        static void NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites();
        static void TheButtonCountsTheAirportsInsteadOfAlwaysSayingOne();
        static void NeitherSideOfTheOverlapIsCalledAProblem();
        static void TheWarningSaysTheCodeOfTheSimulatorCameFromTheName();
        static void TheHeadingsLeaveTheScrollAndStandOverTheirColumns();
        static void ScrollingTheListLeavesTheHeadingsWhereTheyAre();
        static void TwoRulesFrameTheListAndTheListHasNoFrame();
    };

    [[nodiscard]] CoverageLine Covered(const QString& code, const QString& yours, const QString& theirs)
    {
        return {.code = code,
                .covered = yours,
                .andBy = theirs,
                .againstTheSimulator = true,
                .packageName = theirs.toStdString(),
                .one = {},
                .other = {}};
    }

    [[nodiscard]] std::vector<CoverageLine> ManyAirports(const int count)
    {
        std::vector<CoverageLine> lines;
        lines.reserve(static_cast<std::size_t>(count));

        for (int at = 0; at < count; ++at)
        {
            lines.push_back(Covered(QStringLiteral("SB%1").arg(at, 2, 10, QLatin1Char('0')),
                                    QStringLiteral("gsx-pro-airport-sbgr-with-a-long-name-v%1").arg(at),
                                    QStringLiteral("fs24-asobo-airport-sbgr-guarulhos-with-a-long-name-v%1").arg(at)));
        }

        return lines;
    }

    struct Window
    {
        explicit Window(const int tall)
        {
            parent.resize(900, tall);
        }

        QWidget parent;
    };

    [[nodiscard]] QStringList TextsOf(const QDialog& dialog)
    {
        QStringList said;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said;
    }
}

void CoverageDialogTest::TheAirportIsNamedWithBothSidesOfTheOverlap()
{
    const CoverageDialog dialog({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                         QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});

    const QStringList said = TextsOf(dialog);

    QVERIFY(said.contains(QStringLiteral("SBGR")));
    QVERIFY(said.contains(QStringLiteral("gsx-pro-airport-sbgr")));
    QVERIFY2(said.contains(QStringLiteral("fs24-asobo-airport-sbgr-guarulhos")),
             "the warning names the package of the simulator, because that is what the user would have to find by "
             "hand otherwise");
}

void CoverageDialogTest::NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites()
{
    CoverageDialog agreeing({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                     QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});
    QSignalSpy accepted(&agreeing, &QDialog::accepted);
    ButtonSaying(agreeing, QStringLiteral("Disable the simulator's airport"))->click();
    QCOMPARE(accepted.count(), 1);

    CoverageDialog refusing({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                     QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Keep both enabled"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
}

void CoverageDialogTest::TheButtonCountsTheAirportsInsteadOfAlwaysSayingOne()
{
    const CoverageDialog alone({Covered(QStringLiteral("SBGR"), QStringLiteral("yours"), QStringLiteral("theirs"))});
    QVERIFY(ButtonSaying(alone, QStringLiteral("Disable the simulator's airport")) != nullptr);

    const CoverageDialog several({Covered(QStringLiteral("SBGR"), QStringLiteral("yours"), QStringLiteral("theirs")),
                                  Covered(QStringLiteral("LPMA"), QStringLiteral("mine"), QStringLiteral("hers"))});
    QVERIFY(ButtonSaying(several, QStringLiteral("Disable the simulator's airports")) != nullptr);
}

void CoverageDialogTest::NeitherSideOfTheOverlapIsCalledAProblem()
{
    const CoverageDialog dialog({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                         QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});

    for (const QString& said : TextsOf(dialog))
    {
        for (const QString& blame : {QStringLiteral("problem"), QStringLiteral("broken"), QStringLiteral("wrong"),
                                     QStringLiteral("conflict"), QStringLiteral("error")})
        {
            QVERIFY2(!said.contains(blame, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("the rule of 2026-07-31 forbids calling either side a problem, and "
                                               "this line says \"%1\": %2")
                                    .arg(blame, said)));
        }
    }
}

void CoverageDialogTest::TheWarningSaysTheCodeOfTheSimulatorCameFromTheName()
{
    const CoverageDialog dialog({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                         QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});

    const QString said = TextsOf(dialog).join(QLatin1Char(' '));

    QVERIFY2(said.contains(QStringLiteral("package name")),
             "the app cannot open what the simulator ships, so it reads the code off the name, and the sentence that "
             "claims the overlap is where that has to be said");
}

void CoverageDialogTest::TheHeadingsLeaveTheScrollAndStandOverTheirColumns()
{
    ApplyModernistTheme(*qApp);
    Window window(1600);
    CoverageDialog dialog(ManyAirports(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTheHeadingsAreOutsideTheScroll(
        dialog, {QStringLiteral("Airport"), QStringLiteral("Yours"), QStringLiteral("The simulator's")});
    VerifyEachHeadingStandsOverItsColumn(dialog);
}

void CoverageDialogTest::ScrollingTheListLeavesTheHeadingsWhereTheyAre()
{
    ApplyModernistTheme(*qApp);
    Window window(500);
    CoverageDialog dialog(ManyAirports(30), &window.parent);
    ShowItSettled(dialog);

    VerifyTheListScrollsForRealAndTheHeadingsStay(dialog);
}

void CoverageDialogTest::TwoRulesFrameTheListAndTheListHasNoFrame()
{
    ApplyModernistTheme(*qApp);
    Window window(1600);
    CoverageDialog dialog(ManyAirports(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTwoRulesFrameTheList(dialog);
}

QTEST_MAIN(CoverageDialogTest)

#include "tst_coverage_dialog.moc"
