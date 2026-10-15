#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>

#include "infrastructure/platform/LinuxInstanceGuard.h"
#include "support/PathText.h"

namespace
{
    class LinuxInstanceGuardTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFirstGuardDoesNotSeeAnyoneElse();
        static void ASecondGuardOnTheSameFileSeesTheFirst();
        static void AGuardOnAnotherFileIsAlone();
        static void TheFileIsFreeAgainOnceTheGuardIsGone();
        static void TheFolderOfTheLockIsCreatedWhenItIsNotThere();
        static void ALockLeftByAProcessThatIsGoneDoesNotStopTheNextLaunch();
        static void ALockThatCannotBeWrittenDoesNotStopTheLaunchEither();
        static void BringingTheRunningOneForwardSaysSoOnTheLog();
    };
}

namespace
{
    std::filesystem::path LockIn(const QTemporaryDir& directory, const char* name = "fs-organizer.lock")
    {
        return AsPath(directory.path()) / name;
    }

    qint64 AProcessIdThatIsGone()
    {
        QProcess process;
        process.start(QStringLiteral("true"));
        process.waitForFinished();

        return process.processId() > 0 ? process.processId() : 0x3FFFFE;
    }
}

void LinuxInstanceGuardTest::TheFirstGuardDoesNotSeeAnyoneElse()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard guard(LockIn(directory));

    QVERIFY(!guard.AnotherIsRunning());
}

void LinuxInstanceGuardTest::ASecondGuardOnTheSameFileSeesTheFirst()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard first(LockIn(directory));
    const LinuxInstanceGuard second(LockIn(directory));

    QVERIFY(!first.AnotherIsRunning());
    QVERIFY(second.AnotherIsRunning());
}

void LinuxInstanceGuardTest::AGuardOnAnotherFileIsAlone()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard first(LockIn(directory, "a.lock"));
    const LinuxInstanceGuard other(LockIn(directory, "b.lock"));

    QVERIFY(!first.AnotherIsRunning());
    QVERIFY(!other.AnotherIsRunning());
}

void LinuxInstanceGuardTest::TheFileIsFreeAgainOnceTheGuardIsGone()
{
    const QTemporaryDir directory;
    {
        const LinuxInstanceGuard first(LockIn(directory));
        QVERIFY(!first.AnotherIsRunning());
    }

    const LinuxInstanceGuard again(LockIn(directory));

    QVERIFY(!again.AnotherIsRunning());
}

void LinuxInstanceGuardTest::TheFolderOfTheLockIsCreatedWhenItIsNotThere()
{
    const QTemporaryDir directory;
    const std::filesystem::path lock = AsPath(directory.path()) / "run" / "user" / "fs-organizer.lock";

    const LinuxInstanceGuard first(lock);
    const LinuxInstanceGuard second(lock);

    QVERIFY(!first.AnotherIsRunning());
    QVERIFY(second.AnotherIsRunning());
}

void LinuxInstanceGuardTest::ALockLeftByAProcessThatIsGoneDoesNotStopTheNextLaunch()
{
    const QTemporaryDir directory;
    const std::filesystem::path lock = LockIn(directory);

    std::ofstream(lock) << AProcessIdThatIsGone() << "\nfs-organizer\n"
                        << QSysInfo::machineHostName().toStdString() << "\n";

    const LinuxInstanceGuard guard(lock);

    QVERIFY(!guard.AnotherIsRunning());
}

void LinuxInstanceGuardTest::ALockThatCannotBeWrittenDoesNotStopTheLaunchEither()
{
    const QTemporaryDir directory;
    const std::filesystem::path blocker = LockIn(directory, "not-a-folder");
    std::ofstream(blocker) << "x";

    const LinuxInstanceGuard guard(blocker / "fs-organizer.lock");

    QVERIFY(!guard.AnotherIsRunning());
}

void LinuxInstanceGuardTest::BringingTheRunningOneForwardSaysSoOnTheLog()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard guard(LockIn(directory));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("already running")));

    guard.BringTheRunningOneForward();
}

QTEST_APPLESS_MAIN(LinuxInstanceGuardTest)

#include "tst_linux_instance_guard.moc"
