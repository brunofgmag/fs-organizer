#include <QtTest/QtTest>

#include "tests/doubles/InlineBackgroundRunner.h"
#include "viewmodel/GuardedRunner.h"

namespace
{
    class GuardedRunnerTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ASecondGestureIsRefusedWhileTheFirstIsStillOut();
        static void TheGestureThatFinishedLetsTheNextOneThrough();
        static void TheFlagIsLetGoBeforeTheDoneSoAGestureCanStartFromInsideIt();
        static void ItIsBusyOnlyBetweenTheStartAndTheDone();
        static void TheStartIsToldAfterTheFlagIsTakenSoTheScreenSeesTheGesture();
        static void TheRefusedGestureIsNeverAnnounced();
    };
}

void GuardedRunnerTest::ASecondGestureIsRefusedWhileTheFirstIsStillOut()
{
    InlineBackgroundRunner runner;
    runner.defer = true;

    GuardedRunner guarded(runner);

    int worked = 0;

    guarded.Run(
        [&worked]
        {
            ++worked;
        },
        [] {});
    guarded.Run(
        [&worked]
        {
            ++worked;
        },
        [] {});

    QCOMPARE(runner.runs, 1);

    runner.Finish();

    QCOMPARE(worked, 1);
}

void GuardedRunnerTest::TheGestureThatFinishedLetsTheNextOneThrough()
{
    InlineBackgroundRunner runner;
    GuardedRunner guarded(runner);

    int worked = 0;

    guarded.Run(
        [&worked]
        {
            ++worked;
        },
        [] {});
    guarded.Run(
        [&worked]
        {
            ++worked;
        },
        [] {});

    QCOMPARE(worked, 2);
}

void GuardedRunnerTest::TheFlagIsLetGoBeforeTheDoneSoAGestureCanStartFromInsideIt()
{
    InlineBackgroundRunner runner;
    GuardedRunner guarded(runner);

    int worked = 0;

    guarded.Run(
        [&worked]
        {
            ++worked;
        },
        [&guarded, &worked]
        {
            guarded.Run(
                [&worked]
                {
                    ++worked;
                },
                [] {});
        });

    QCOMPARE(worked, 2);
}

void GuardedRunnerTest::ItIsBusyOnlyBetweenTheStartAndTheDone()
{
    InlineBackgroundRunner runner;
    runner.defer = true;

    GuardedRunner guarded(runner);

    QVERIFY(!guarded.Busy());

    guarded.Run([] {}, [] {});

    QVERIFY(guarded.Busy());

    runner.Finish();

    QVERIFY(!guarded.Busy());
}

void GuardedRunnerTest::TheStartIsToldAfterTheFlagIsTakenSoTheScreenSeesTheGesture()
{
    InlineBackgroundRunner runner;
    runner.defer = true;

    GuardedRunner guarded(runner);

    bool busyWhenAnnounced = false;

    guarded.Run(
        [&guarded, &busyWhenAnnounced]
        {
            busyWhenAnnounced = guarded.Busy();
        },
        [] {}, [] {});

    QVERIFY2(busyWhenAnnounced, "the gesture was announced before the guard was taken");
}

void GuardedRunnerTest::TheRefusedGestureIsNeverAnnounced()
{
    InlineBackgroundRunner runner;
    runner.defer = true;

    GuardedRunner guarded(runner);

    int announced = 0;

    const auto announce = [&announced]
    {
        ++announced;
    };

    guarded.Run(announce, [] {}, [] {});
    guarded.Run(announce, [] {}, [] {});

    QCOMPARE(announced, 1);
}

QTEST_APPLESS_MAIN(GuardedRunnerTest)

#include "tst_guarded_runner.moc"
