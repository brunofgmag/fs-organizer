#include <QtGui/QWheelEvent>
#include <QtTest/QtTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QWidget>

#include "view/community/RepairDialog.h"
#include "view/setup/StagingLeftoverDialog.h"
#include "view/WheelGuard.h"

namespace
{
    class WheelGuardTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ScrollingOverAnUnfocusedComboLeavesItAlone();
        static void ScrollingOverAFocusedComboStillChangesIt();
        static void ARollOverTheRepairDialogDoesNotRewriteADestructivePlan();
        static void ARollOverTheLeftoverDialogDoesNotTurnAResumeIntoADiscard();
    };
}

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

namespace
{
    RepairCandidate ADeadLinkRepointableTo(const char* name, const char* library)
    {
        RepairCandidate candidate;
        candidate.entry.path = std::filesystem::path(R"(E:\Flight Simulator 2024\Community)") / name;
        candidate.entry.target = std::filesystem::path(R"(D:\gone)") / name;
        candidate.entry.classification = EntryClassification::Broken;
        candidate.targetsLibrary = true;
        candidate.repointTo = std::filesystem::path(library) / name;

        return candidate;
    }

    void RollDownOver(QComboBox* combo)
    {
        combo->clearFocus();
        QVERIFY(!combo->hasFocus());

        QWheelEvent roll = ARollDown(combo);
        QCoreApplication::sendEvent(combo, &roll);
    }
}

void WheelGuardTest::ARollOverTheRepairDialogDoesNotRewriteADestructivePlan()
{
    RepairDialog dialog({ADeadLinkRepointableTo("pmdg-aircraft-77w", R"(D:\MSFS 2024\Aircrafts)"),
                         ADeadLinkRepointableTo("aerosoft-crj", R"(D:\MSFS 2024\Aircrafts)")});
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    const QList<QComboBox*> actions = dialog.findChildren<QComboBox*>();
    QCOMPARE(actions.size(), 2);
    QCOMPARE(actions.first()->count(), 2);
    QCOMPARE(actions.first()->currentIndex(), 0);

    RollDownOver(actions.first());

    QCOMPARE(actions.first()->currentIndex(), 0);

    const std::vector<RepairRequest> requests = dialog.ChosenRequests();
    QCOMPARE(requests.size(), std::size_t{2});
    QCOMPARE(requests.front().action, RepairAction::RemoveDeadNode);
}

void WheelGuardTest::ARollOverTheLeftoverDialogDoesNotTurnAResumeIntoADiscard()
{
    StagingLeftover interrupted;
    interrupted.staging = R"(D:\MSFS 2024\.fsorg-staging\pmdg-aircraft-77w)";
    interrupted.target = R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)";
    interrupted.source = R"(E:\Flight Simulator 2024\Community\pmdg-aircraft-77w)";

    StagingLeftoverDialog dialog({interrupted});
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    const QList<QComboBox*> actions = dialog.findChildren<QComboBox*>();
    QCOMPARE(actions.size(), 1);
    QCOMPARE(dialog.ToResume().size(), std::size_t{1});

    RollDownOver(actions.first());

    QCOMPARE(dialog.ToResume().size(), std::size_t{1});
    QVERIFY(dialog.ToDiscard().empty());
}

QTEST_MAIN(WheelGuardTest)

#include "tst_wheel_guard.moc"
