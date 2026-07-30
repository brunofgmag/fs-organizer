#include <crtdbg.h>

#include <QtCore/QPointer>
#include <QtTest/QtTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>

#include "view/shell/MainWindow.h"
#include "view/shell/TriageStrip.h"
#include "view/theme/PageTab.h"

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    static void EveryWidgetItBuildsDiesWithTheWindow();
    static void BuildingAndDestroyingTheWindowLeavesTheHeapWhereItWas();
    static void ACleanInstallShowsNoTriageStrip();
    static void TheStripOnlyRidesOnThePagesThatCarryIt();
    static void TheFooterCarriesTheSummaryOfThePageYouAreOn();
    static void ThePageTabsSitInARowAndStillSwitchPages();
    static void ThePageTabWritesItsCountApartFromItsName();
    static void LeavingAPageTakesItsStatusMessageAway();
    static void TheMeterFillsWithWhatThePageEnabled();
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
#ifndef _DEBUG
    QSKIP("CRT heap accounting only exists in the debug runtime.");
#else
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
#endif
}

void MainWindowTest::ACleanInstallShowsNoTriageStrip()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage(QStringLiteral("Biblioteca"), library);
    window.CarryTriageOn(library);

    auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip != nullptr);

    window.ShowTriage(0, 0, 0);
    QVERIFY(!strip->isVisibleTo(&window));

    window.ShowTriage(28, 2, 178);
    QVERIFY(strip->isVisibleTo(&window));
}

void MainWindowTest::TheStripOnlyRidesOnThePagesThatCarryIt()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* journal = new QWidget(&window);
    window.AddPage(QStringLiteral("Biblioteca"), library);
    PageTab* journalTab = window.AddPage(QStringLiteral("Diário"), journal);
    window.CarryTriageOn(library);

    window.ShowTriage(28, 0, 0);

    auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip->isVisibleTo(&window));

    journalTab->click();
    QVERIFY(!strip->isVisibleTo(&window));
}

void MainWindowTest::TheFooterCarriesTheSummaryOfThePageYouAreOn()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* journal = new QWidget(&window);
    window.AddPage(QStringLiteral("Biblioteca"), library);
    PageTab* journalTab = window.AddPage(QStringLiteral("Diário"), journal);

    window.ShowSummary(library, QStringLiteral("346 addons · 27 habilitados"));
    window.ShowSummary(journal, QStringLiteral("1204 operações registradas"));

    auto* footer = window.findChild<QLabel*>(QStringLiteral("FooterSummary"));
    QVERIFY(footer != nullptr);
    QCOMPARE(footer->text(), QStringLiteral("346 addons · 27 habilitados"));

    journalTab->click();

    QCOMPARE(footer->text(), QStringLiteral("1204 operações registradas"));
}

void MainWindowTest::ThePageTabsSitInARowAndStillSwitchPages()
{
    MainWindow window(SettingsWithOneProfile());

    auto* first = new QWidget(&window);
    auto* second = new QWidget(&window);
    PageTab* firstTab = window.AddPage(QStringLiteral("Biblioteca"), first);
    PageTab* secondTab = window.AddPage(QStringLiteral("Community"), second);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* pages = window.findChild<QStackedWidget*>();
    QCOMPARE(pages->currentWidget(), first);
    QVERIFY(firstTab->isChecked());

    QSignalSpy selected(&window, &MainWindow::PageSelected);
    secondTab->click();

    QCOMPARE(pages->currentWidget(), second);
    QCOMPARE(selected.count(), 1);

    const QPoint firstSpot = firstTab->mapTo(&window, QPoint(0, 0));
    const QPoint secondSpot = secondTab->mapTo(&window, QPoint(0, 0));
    QCOMPARE(firstSpot.y(), secondSpot.y());
    QVERIFY(secondSpot.x() > firstSpot.x());
}

void MainWindowTest::ThePageTabWritesItsCountApartFromItsName()
{
    MainWindow window(SettingsWithOneProfile());

    PageTab* tab = window.AddPage(QStringLiteral("Biblioteca"), new QWidget(&window));
    const int bare = tab->sizeHint().width();

    tab->ShowCount(346);

    QCOMPARE(tab->Label(), QStringLiteral("Biblioteca"));
    QVERIFY(tab->text().contains(QStringLiteral("346")));
    QVERIFY(tab->sizeHint().width() > bare);

    tab->ShowCount(std::nullopt);

    QCOMPARE(tab->text(), QStringLiteral("Biblioteca"));
    QCOMPARE(tab->sizeHint().width(), bare);
}

void MainWindowTest::LeavingAPageTakesItsStatusMessageAway()
{
    MainWindow window(SettingsWithOneProfile());

    auto* first = new QWidget(&window);
    auto* second = new QWidget(&window);
    window.AddPage(QStringLiteral("Biblioteca"), first);
    PageTab* secondTab = window.AddPage(QStringLiteral("Community"), second);

    window.ShowSummary(first, QStringLiteral("346 addons · 27 habilitados"));
    window.ShowSummary(second, QStringLiteral("244 entradas"));

    auto* footer = window.findChild<QLabel*>(QStringLiteral("FooterSummary"));

    window.ShowStatus(QStringLiteral("12 operação(ões) concluída(s)."));
    QCOMPARE(footer->text(), QStringLiteral("12 operação(ões) concluída(s)."));

    secondTab->click();

    QCOMPARE(footer->text(), QStringLiteral("244 entradas"));
}

void MainWindowTest::TheMeterFillsWithWhatThePageEnabled()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage(QStringLiteral("Biblioteca"), library);

    auto* meter = window.findChild<QProgressBar*>(QStringLiteral("FooterMeter"));
    QVERIFY(meter != nullptr);
    QVERIFY(!meter->isVisibleTo(&window));

    window.ShowMeter(library, 27, 346);

    QVERIFY(meter->isVisibleTo(&window));
    QCOMPARE(meter->value(), 27);
    QCOMPARE(meter->maximum(), 346);
}

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
