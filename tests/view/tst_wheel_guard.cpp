#include <QtGui/QWheelEvent>
#include <QtTest/QtTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QWidget>

#include "view/WheelGuard.h"

class WheelGuardTest : public QObject
{
    Q_OBJECT

private slots:
    static void ScrollingOverAnUnfocusedComboLeavesItAlone();
    static void ScrollingOverAFocusedComboStillChangesIt();
};

namespace
{
    QWheelEvent ARollDown(QWidget* widget)
    {
        const QPointF centre(widget->width() / 2.0, widget->height() / 2.0);

        return {centre,
                widget->mapToGlobal(centre.toPoint()),
                {},
                {0, -120},
                Qt::NoButton,
                Qt::NoModifier,
                Qt::NoScrollPhase,
                false};
    }

    QComboBox* GuardedComboIn(QWidget& host)
    {
        auto* combo = new QComboBox(&host);
        combo->addItems({QStringLiteral("Ligar"), QStringLiteral("Desligar")});
        combo->resize(80, 24);
        LetTheWheelScrollPastUnlessTheWidgetHasFocus(combo);

        return combo;
    }
}

void WheelGuardTest::ScrollingOverAnUnfocusedComboLeavesItAlone()
{
    QWidget host;
    host.resize(200, 100);
    QComboBox* combo = GuardedComboIn(host);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    combo->clearFocus();

    QVERIFY(!combo->hasFocus());
    QCOMPARE(combo->currentIndex(), 0);

    QWheelEvent roll = ARollDown(combo);
    QCoreApplication::sendEvent(combo, &roll);

    QCOMPARE(combo->currentIndex(), 0);
}

void WheelGuardTest::ScrollingOverAFocusedComboStillChangesIt()
{
    QWidget host;
    host.resize(200, 100);
    QComboBox* combo = GuardedComboIn(host);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    combo->setFocus();
    QVERIFY(combo->hasFocus());

    QWheelEvent roll = ARollDown(combo);
    QCoreApplication::sendEvent(combo, &roll);

    QCOMPARE(combo->currentIndex(), 1);
}

QTEST_MAIN(WheelGuardTest)

#include "tst_wheel_guard.moc"
