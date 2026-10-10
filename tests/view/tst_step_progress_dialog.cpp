#include <QtTest/QtTest>

#include <QtCore/QTranslator>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/InstalledCatalogue.h"
#include "view/shell/StepProgressDialog.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/ModernistTones.h"

namespace
{
    class StepProgressDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();
        static void TheWidthDoesNotChangeBetweenSteps();
        static void EachStepWearsItsStageAndFillsItsOwnMeter();
        static void TheStageDecidesTheInkOfTheStepName();
        static void ALongSubjectIsCutInTheMiddle();
        static void TheTooltipKeepsTheFullTextOnlyWhenTheLineWasCut();
        static void CancellingIsAnnouncedOnceAndTheButtonStopsTakingClicks();
        static void TheOneStepFormUsedForReadingShowsABusyMeter();
        static void AMeterThatCannotBeMeasuredBecomesAMeasuredOneAgainWhenTheStepChanges();
        static void TheButtonSpeaksTheLanguage_data();
        static void TheButtonSpeaksTheLanguage();
    };

    constexpr int kEveryStep = 3;

    [[nodiscard]] QStringList ThreeSteps()
    {
        return {QStringLiteral("Extracting"), QStringLiteral("Checking what was extracted"),
                QStringLiteral("Putting into place")};
    }

    [[nodiscard]] QList<QLabel*> NamesOf(const StepProgressDialog& dialog)
    {
        return dialog.findChildren<QLabel*>(QStringLiteral("StepName"));
    }

    [[nodiscard]] QList<QProgressBar*> MetersOf(const StepProgressDialog& dialog)
    {
        return dialog.findChildren<QProgressBar*>(QStringLiteral("ImportMeter"));
    }

    [[nodiscard]] QList<QLabel*> QuietLinesOf(const StepProgressDialog& dialog)
    {
        return dialog.findChildren<QLabel*>(QStringLiteral("ImportQuiet"));
    }

    [[nodiscard]] QPushButton* CancelOf(const StepProgressDialog& dialog)
    {
        return dialog.findChild<QPushButton*>(QStringLiteral("CancelTheSteps"));
    }

    [[nodiscard]] QColor InkOf(const QLabel& label)
    {
        return label.palette().color(QPalette::WindowText);
    }
}

void StepProgressDialogTest::initTestCase()
{
    ApplyModernistTheme(*qApp);
}

void StepProgressDialogTest::TheWidthDoesNotChangeBetweenSteps()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    const int wide = dialog.width();

    for (int step = 0; step < kEveryStep; ++step)
    {
        dialog.ShowTheStep(step);
        dialog.ShowTheSubject(QStringLiteral("Item %1 of 3 · %2").arg(step + 1).arg(QString(300, QLatin1Char('x'))));
        dialog.ShowTheDetail(QStringLiteral("Addon 1 of 2 · %1").arg(QString(300, QLatin1Char('y'))));
        QCoreApplication::processEvents();

        QCOMPARE(dialog.width(), wide);
    }
}

void StepProgressDialogTest::EachStepWearsItsStageAndFillsItsOwnMeter()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    const QList<QLabel*> names = NamesOf(dialog);
    const QList<QProgressBar*> meters = MetersOf(dialog);

    QCOMPARE(names.size(), kEveryStep);
    QCOMPARE(meters.size(), kEveryStep);

    QCOMPARE(names.at(0)->property("stage").toString(), QStringLiteral("now"));
    QCOMPARE(names.at(1)->property("stage").toString(), QStringLiteral("next"));
    QCOMPARE(names.at(2)->property("stage").toString(), QStringLiteral("next"));

    dialog.ShowTheStep(1);

    QCOMPARE(names.at(0)->property("stage").toString(), QStringLiteral("done"));
    QCOMPARE(names.at(1)->property("stage").toString(), QStringLiteral("now"));
    QCOMPARE(names.at(2)->property("stage").toString(), QStringLiteral("next"));
    QCOMPARE(meters.at(0)->value(), meters.at(0)->maximum());
    QCOMPARE(meters.at(1)->value(), 0);
    QCOMPARE(meters.at(2)->value(), 0);

    dialog.ShowTheProgress(25, 100);

    QCOMPARE(meters.at(1)->value(), meters.at(1)->maximum() / 4);
    QCOMPARE(meters.at(0)->value(), meters.at(0)->maximum());
    QCOMPARE(meters.at(2)->value(), 0);

    dialog.ShowTheStep(2);

    QCOMPARE(names.at(1)->property("stage").toString(), QStringLiteral("done"));
    QCOMPARE(names.at(2)->property("stage").toString(), QStringLiteral("now"));
    QCOMPARE(meters.at(1)->value(), meters.at(1)->maximum());
    QCOMPARE(meters.at(2)->value(), 0);

    dialog.ShowTheProgress(0, 0);

    QCOMPARE(meters.at(2)->value(), 0);
}

void StepProgressDialogTest::TheStageDecidesTheInkOfTheStepName()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();
    dialog.ShowTheStep(1);

    const ModernistTones tones = TonesOf(CurrentColorScheme());
    const QList<QLabel*> names = NamesOf(dialog);

    QCOMPARE(InkOf(*names.at(0)), tones.secondary);
    QCOMPARE(InkOf(*names.at(2)), tones.faint);
    QVERIFY(InkOf(*names.at(1)) != tones.faint);
    QVERIFY(InkOf(*names.at(1)) != tones.secondary);
}

void StepProgressDialogTest::ALongSubjectIsCutInTheMiddle()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    const QString first = QStringLiteral("Item 1 of 3 · FlyTampa-");
    const QString last = QStringLiteral("-Amsterdam-v2.4.1-final.zip");
    const QString full = first + QString(200, QLatin1Char('m')) + last;

    dialog.ShowTheSubject(full);

    const QLabel* subject = QuietLinesOf(dialog).first();

    QVERIFY(subject->text().size() < full.size());
    QVERIFY(subject->text().startsWith(first.left(10)));
    QVERIFY(subject->text().endsWith(last.right(10)));
    QVERIFY(subject->text().contains(QChar(0x2026)));
}

void StepProgressDialogTest::TheTooltipKeepsTheFullTextOnlyWhenTheLineWasCut()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    const QString full = QStringLiteral("Item 1 of 3 · ") + QString(300, QLatin1Char('m')) + QStringLiteral(".zip");
    const QLabel* subject = QuietLinesOf(dialog).first();

    dialog.ShowTheSubject(full);

    QCOMPARE(subject->toolTip(), full);

    dialog.ShowTheSubject(QStringLiteral("Item 1 of 3 · short.zip"));

    QCOMPARE(subject->text(), QStringLiteral("Item 1 of 3 · short.zip"));
    QVERIFY(subject->toolTip().isEmpty());
}

void StepProgressDialogTest::CancellingIsAnnouncedOnceAndTheButtonStopsTakingClicks()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    QSignalSpy cancelled(&dialog, &StepProgressDialog::Cancelled);
    QPushButton* cancel = CancelOf(dialog);

    QVERIFY(cancel != nullptr);
    QVERIFY(cancel->isEnabled());
    QCOMPARE(cancel->text(), QStringLiteral("Cancel"));

    cancel->click();

    QCOMPARE(cancelled.count(), 1);

    dialog.ShowItIsCancelling(QStringLiteral("Removing what was extracted…"));

    QVERIFY(!cancel->isEnabled());
    QCOMPARE(cancel->text(), QStringLiteral("Cancelling…"));
    QCOMPARE(QuietLinesOf(dialog).last()->text(), QStringLiteral("Removing what was extracted…"));

    QTest::mouseClick(cancel, Qt::LeftButton);
    cancel->click();

    QCOMPARE(cancelled.count(), 1);
}

void StepProgressDialogTest::TheOneStepFormUsedForReadingShowsABusyMeter()
{
    StepProgressDialog dialog(QStringLiteral("Install into the library"), {QStringLiteral("Reading what it holds")},
                              nullptr);
    dialog.show();

    QCOMPARE(MetersOf(dialog).size(), 1);
    QCOMPARE(NamesOf(dialog).size(), 1);

    dialog.ShowThatItCannotBeMeasured();

    const QProgressBar* meter = MetersOf(dialog).first();

    QCOMPARE(meter->minimum(), 0);
    QCOMPARE(meter->maximum(), 0);
    QCOMPARE(NamesOf(dialog).first()->property("stage").toString(), QStringLiteral("now"));
}

void StepProgressDialogTest::AMeterThatCannotBeMeasuredBecomesAMeasuredOneAgainWhenTheStepChanges()
{
    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);
    dialog.show();

    dialog.ShowThatItCannotBeMeasured();

    const QList<QProgressBar*> meters = MetersOf(dialog);

    QCOMPARE(meters.at(0)->maximum(), 0);

    dialog.ShowTheStep(1);

    QVERIFY(meters.at(0)->maximum() > 0);
    QCOMPARE(meters.at(0)->value(), meters.at(0)->maximum());
}

void StepProgressDialogTest::TheButtonSpeaksTheLanguage_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("cancel");
    QTest::addColumn<QString>("cancelling");

    QTest::newRow("English") << QStringLiteral("en") << QStringLiteral("Cancel") << QStringLiteral("Cancelling…");
    QTest::newRow("Brazilian Portuguese")
        << QStringLiteral("pt_BR") << QStringLiteral("Cancelar") << QStringLiteral("Cancelando…");
}

void StepProgressDialogTest::TheButtonSpeaksTheLanguage()
{
    QFETCH(QString, language);
    QFETCH(QString, cancel);
    QFETCH(QString, cancelling);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    StepProgressDialog dialog(QStringLiteral("Installing into the library"), ThreeSteps(), nullptr);

    QCOMPARE(CancelOf(dialog)->text(), cancel);

    dialog.ShowItIsCancelling(QString());

    QCOMPARE(CancelOf(dialog)->text(), cancelling);
}

QTEST_MAIN(StepProgressDialogTest)

#include "tst_step_progress_dialog.moc"
