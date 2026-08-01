#include <QtTest/QtTest>

#include <memory>

#include "infrastructure/platform/SingleInstance.h"

class SingleInstanceTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheFirstGuardDoesNotSeeAnyoneElse();
    static void ASecondGuardWithTheSameNameSeesTheFirst();
    static void AGuardWithAnotherNameIsAlone();
    static void TheNameIsFreeAgainOnceTheGuardIsGone();
};

namespace
{
    constexpr auto kName = L"Local\\fs-organizer-tests-a";
    constexpr auto kOtherName = L"Local\\fs-organizer-tests-b";
}

void SingleInstanceTest::TheFirstGuardDoesNotSeeAnyoneElse()
{
    const SingleInstance guard(kName);

    QVERIFY(!guard.AnotherIsRunning());
}

void SingleInstanceTest::ASecondGuardWithTheSameNameSeesTheFirst()
{
    const SingleInstance first(kName);
    const SingleInstance second(kName);

    QVERIFY(!first.AnotherIsRunning());
    QVERIFY(second.AnotherIsRunning());
}

void SingleInstanceTest::AGuardWithAnotherNameIsAlone()
{
    const SingleInstance first(kName);
    const SingleInstance other(kOtherName);

    QVERIFY(!other.AnotherIsRunning());
}

void SingleInstanceTest::TheNameIsFreeAgainOnceTheGuardIsGone()
{
    {
        const SingleInstance first(kName);
        QVERIFY(!first.AnotherIsRunning());
    }

    const SingleInstance again(kName);

    QVERIFY(!again.AnotherIsRunning());
}

QTEST_APPLESS_MAIN(SingleInstanceTest)

#include "tst_single_instance.moc"
