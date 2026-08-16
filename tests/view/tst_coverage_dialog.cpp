#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <vector>

#include "tests/support/ButtonLookup.h"
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
    ButtonSaying(agreeing, QStringLiteral("Turn the simulator's one off"))->click();
    QCOMPARE(accepted.count(), 1);

    CoverageDialog refusing({Covered(QStringLiteral("SBGR"), QStringLiteral("gsx-pro-airport-sbgr"),
                                     QStringLiteral("fs24-asobo-airport-sbgr-guarulhos"))});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Leave both on"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
}

void CoverageDialogTest::TheButtonCountsTheAirportsInsteadOfAlwaysSayingOne()
{
    const CoverageDialog alone({Covered(QStringLiteral("SBGR"), QStringLiteral("yours"), QStringLiteral("theirs"))});
    QVERIFY(ButtonSaying(alone, QStringLiteral("Turn the simulator's one off")) != nullptr);

    const CoverageDialog several({Covered(QStringLiteral("SBGR"), QStringLiteral("yours"), QStringLiteral("theirs")),
                                  Covered(QStringLiteral("LPMA"), QStringLiteral("mine"), QStringLiteral("hers"))});
    QVERIFY(ButtonSaying(several, QStringLiteral("Turn the simulator's ones off")) != nullptr);
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

    QVERIFY2(said.contains(QStringLiteral("package name")) && said.contains(QStringLiteral("archive")),
             "the app cannot open what the simulator ships, so it reads the code off the name, and the sentence that "
             "claims the overlap is where that has to be said");
}

QTEST_MAIN(CoverageDialogTest)

#include "tst_coverage_dialog.moc"
