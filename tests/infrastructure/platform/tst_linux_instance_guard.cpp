#include <QtCore/QLockFile>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>
#include <QtWidgets/QWidget>

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
        static void ASecondLaunchBringsTheMainWindowOfTheFirstForward();
        static void ASecondLaunchShowsAMainWindowThatWasHidden();
        static void ASecondLaunchLeavesAnyOtherWindowAlone();
        static void ASecondLaunchWithNobodyAnsweringSaysSoAndReturns();
        static void TheSocketGoesAwayWithTheGuardThatOwnedIt();
        static void ASocketLeftByARunThatDiedDoesNotStopTheNextOneFromAnswering();
    };
}

namespace
{
    constexpr int kAnswerTimeoutMs = 10000;

    void ExpectTheOffscreenPluginToRefuseRaise()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("does not support raise")));
    }

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

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not listen")));

    const LinuxInstanceGuard guard(blocker / "fs-organizer.lock");

    QVERIFY(!guard.AnotherIsRunning());
}

void LinuxInstanceGuardTest::ASecondLaunchBringsTheMainWindowOfTheFirstForward()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard first(LockIn(directory));
    const LinuxInstanceGuard second(LockIn(directory));
    QVERIFY(second.AnotherIsRunning());

    QWidget main;
    main.setWindowTitle(QCoreApplication::applicationName());
    main.show();
    QVERIFY(QTest::qWaitForWindowExposed(&main));

    QWidget other;
    other.setWindowTitle(QStringLiteral("a dialog"));
    other.show();
    QVERIFY(QTest::qWaitForWindowExposed(&other));
    other.activateWindow();
    QTRY_COMPARE(QApplication::activeWindow(), &other);

    ExpectTheOffscreenPluginToRefuseRaise();
    second.BringTheRunningOneForward();

    QTRY_COMPARE_WITH_TIMEOUT(QApplication::activeWindow(), &main, kAnswerTimeoutMs);
}

void LinuxInstanceGuardTest::ASecondLaunchShowsAMainWindowThatWasHidden()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard first(LockIn(directory));
    const LinuxInstanceGuard second(LockIn(directory));

    QWidget main;
    main.setWindowTitle(QCoreApplication::applicationName());
    QVERIFY(!main.isVisible());

    ExpectTheOffscreenPluginToRefuseRaise();
    second.BringTheRunningOneForward();

    QTRY_VERIFY_WITH_TIMEOUT(main.isVisible(), kAnswerTimeoutMs);
}

void LinuxInstanceGuardTest::ASecondLaunchLeavesAnyOtherWindowAlone()
{
    const QTemporaryDir directory;
    const LinuxInstanceGuard first(LockIn(directory));
    const LinuxInstanceGuard second(LockIn(directory));

    QWidget main;
    main.setWindowTitle(QCoreApplication::applicationName());

    QWidget other;
    other.setWindowTitle(QStringLiteral("not the main window"));

    ExpectTheOffscreenPluginToRefuseRaise();
    second.BringTheRunningOneForward();

    QTRY_VERIFY_WITH_TIMEOUT(main.isVisible(), kAnswerTimeoutMs);
    QVERIFY2(!other.isVisible(), "a window that is not the main one was shown");
}

void LinuxInstanceGuardTest::ASecondLaunchWithNobodyAnsweringSaysSoAndReturns()
{
    const QTemporaryDir directory;
    QLockFile held(QString::fromStdString(LockIn(directory).string()));
    QVERIFY(held.tryLock(0));

    const LinuxInstanceGuard guard(LockIn(directory));
    QVERIFY(guard.AnotherIsRunning());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not answer")));

    guard.BringTheRunningOneForward();
}

void LinuxInstanceGuardTest::TheSocketGoesAwayWithTheGuardThatOwnedIt()
{
    const QTemporaryDir directory;
    const QString socket = directory.path() + QStringLiteral("/fs-organizer.sock");

    {
        const LinuxInstanceGuard first(LockIn(directory));
        QVERIFY(QFileInfo::exists(socket));
    }

    QVERIFY2(!QFileInfo::exists(socket), "the socket of a guard that is gone was left on disk");
}

void LinuxInstanceGuardTest::ASocketLeftByARunThatDiedDoesNotStopTheNextOneFromAnswering()
{
    const QTemporaryDir directory;
    const QString socket = directory.path() + QStringLiteral("/fs-organizer.sock");
    std::ofstream(socket.toStdString()) << "left behind";

    const LinuxInstanceGuard first(LockIn(directory));
    const LinuxInstanceGuard second(LockIn(directory));

    QWidget main;
    main.setWindowTitle(QCoreApplication::applicationName());

    ExpectTheOffscreenPluginToRefuseRaise();
    second.BringTheRunningOneForward();

    QTRY_VERIFY_WITH_TIMEOUT(main.isVisible(), kAnswerTimeoutMs);
}

QTEST_MAIN(LinuxInstanceGuardTest)

#include "tst_linux_instance_guard.moc"
