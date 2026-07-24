#include <QtTest/QtTest>

#include <windows.h>

#include "infrastructure/sim/WindowsProcessProbe.h"

class WindowsProcessProbeTest : public QObject
{
    Q_OBJECT

private slots:
    static void AProcessThatIsRunningIsSeen();
    static void AProcessThatIsNotRunningIsNotSeen();
};

namespace
{
    std::string OwnExecutableName()
    {
        wchar_t buffer[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer).filename().string();
    }
}

void WindowsProcessProbeTest::AProcessThatIsRunningIsSeen()
{
    const WindowsProcessProbe probe({OwnExecutableName()});

    QVERIFY(probe.SimulatorIsRunning());
}

void WindowsProcessProbeTest::AProcessThatIsNotRunningIsNotSeen()
{
    const WindowsProcessProbe probe({"fsorg-no-such-simulator-9d3f.exe"});

    QVERIFY(!probe.SimulatorIsRunning());
}

QTEST_APPLESS_MAIN(WindowsProcessProbeTest)

#include "tst_windows_process_probe.moc"
