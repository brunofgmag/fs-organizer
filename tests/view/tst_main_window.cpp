#include <crtdbg.h>

#include <QtCore/QPointer>
#include <QtTest/QtTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QStackedWidget>

#include "view/MainWindow.h"

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    static void EveryWidgetItBuildsDiesWithTheWindow();
    static void BuildingAndDestroyingTheWindowLeavesTheHeapWhereItWas();
};

namespace
{
    AppSettings SettingsWithOneProfile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {"E:/Flight Simulator 2024/Community"};
        profile.defaultDestination = "E:/Flight Simulator 2024/Community";
        profile.libraries = {Library{"{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}", "D:/MSFS 2024", "MSFS 2024"}};

        AppSettings settings;
        settings.profiles = {profile};
        settings.activeProfileId = "msfs2024";

        return settings;
    }
}

void MainWindowTest::EveryWidgetItBuildsDiesWithTheWindow()
{
    QPointer<QStackedWidget> pages;
    QPointer<QComboBox> profiles;
    QPointer<QWidget> header;
    QPointer<QLayout> headerLayout;
    QList<QPointer<QLabel>> labels;

    {
        MainWindow window(SettingsWithOneProfile());

        pages = window.findChild<QStackedWidget*>();
        profiles = window.findChild<QComboBox*>();
        QVERIFY(!pages.isNull());
        QVERIFY(!profiles.isNull());

        header = profiles->parentWidget();
        headerLayout = header->layout();
        QVERIFY(!header.isNull());
        QVERIFY(!headerLayout.isNull());

        for (QLabel* label : window.findChildren<QLabel*>())
        {
            labels.append(label);
        }
        QVERIFY(labels.size() >= 2);
    }

    QVERIFY(pages.isNull());
    QVERIFY(profiles.isNull());
    QVERIFY(header.isNull());
    QVERIFY(headerLayout.isNull());

    for (const QPointer<QLabel>& label : labels)
    {
        QVERIFY(label.isNull());
    }
}

void MainWindowTest::BuildingAndDestroyingTheWindowLeavesTheHeapWhereItWas()
{
    {
        MainWindow warmUp(SettingsWithOneProfile());
    }

    _CrtMemState before{};
    _CrtMemState after{};
    _CrtMemState difference{};

    _CrtMemCheckpoint(&before);
    {
        MainWindow window(SettingsWithOneProfile());
    }
    _CrtMemCheckpoint(&after);

    const int grew = _CrtMemDifference(&difference, &before, &after);
    if (grew != 0)
    {
        _CrtMemDumpStatistics(&difference);
    }

    QCOMPARE(grew, 0);
}

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
