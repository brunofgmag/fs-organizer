#include <algorithm>

#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtTest/QtTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>

#include "view/shell/MainWindow.h"
#include "view/theme/ModernistMetrics.h"
#include "view/shell/TriageStrip.h"
#include "view/theme/ArrowButton.h"
#include "view/theme/PageTab.h"
#include "tests/support/PageFloor.h"

namespace
{
    class MainWindowTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheChromeFitsTheNarrowestWindow();
        static void NothingOffersAnUpdateUntilOneIsOffered();
        static void TheOfferNamesTheVersionAndTurnsIntoARestartWhenItIsStaged();
        static void EveryWidgetItBuildsDiesWithTheWindow();
        static void ACleanInstallShowsNoTriageStrip();
        static void FoldersOutsideTheLibraryAloneLeaveTheStripDown();
        static void EachOfTheThreeThingsTheStripSaysBringsItUp_data();
        static void EachOfTheThreeThingsTheStripSaysBringsItUp();
        static void TheStripOnlyRidesOnThePagesThatCarryIt();
        static void TheFooterCarriesTheSummaryOfThePageYouAreOn();
        static void ThePageTabsSitInARowAndStillSwitchPages();
        static void ThePageTabWritesItsCountApartFromItsName();
        static void LeavingAPageTakesItsStatusMessageAway();
        static void TheMeterFillsWithWhatThePageEnabled();
        static void TheFooterKeepsThePageGutterOnBothEdges();
        static void TheGearOpensTheOptionsAndTheBackButtonNamesWhereItCameFrom();
        static void LeavingTheOptionsGivesBackThePageThatWasOpen();
        static void TheTriageStripStandsDownWhileTheOptionsAreOpen();
        static void ClickingBackFromTheOptionsLeavesTheOriginTabStillMarked();
        static void TheBackTabPointsLeftWithoutAGlyphInItsLabel();
        static void TheOfferPointsDownWhileThereIsSomethingToDownload();
        static void ADownloadedOfferCarriesNoArrow();
    };
}

namespace
{
    AppSettings SettingsWithOneProfile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {"E:/Flight Simulator 2024/Community"};
        profile.defaultDestination = "E:/Flight Simulator 2024/Community";
        profile.libraries = {
            Library{.id = "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}", .path = "D:/MSFS 2024", .label = "MSFS 2024"}};

        AppSettings settings;
        settings.profiles = {profile};
        settings.activeProfileId = "msfs2024";

        return settings;
    }

    [[nodiscard]] QImage Rendered(QWidget& widget, const QSize& size)
    {
        widget.resize(size);

        QImage surface(size, QImage::Format_ARGB32_Premultiplied);
        surface.fill(Qt::magenta);
        widget.render(&surface);

        return surface;
    }
}

void MainWindowTest::EveryWidgetItBuildsDiesWithTheWindow()
{
    QPointer<QStackedWidget> pages;
    QPointer<QComboBox> profiles;
    QPointer<QWidget> header;
    QPointer<QLayout> headerLayout;
    QList<QPointer<QObject>> everythingItBuilt;

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

        QVERIFY(window.findChildren<QLabel*>().size() >= 2);

        for (QObject* child : window.findChildren<QObject*>())
        {
            everythingItBuilt.append(child);
        }
        QVERIFY(everythingItBuilt.size() >= 20);
    }

    QVERIFY(pages.isNull());
    QVERIFY(profiles.isNull());
    QVERIFY(header.isNull());
    QVERIFY(headerLayout.isNull());

    for (const QPointer<QObject>& child : everythingItBuilt)
    {
        QVERIFY2(child.isNull(), "the window was destroyed and something it built outlived it");
    }
}

void MainWindowTest::ACleanInstallShowsNoTriageStrip()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage("Library", library);
    window.CarryTriageOn(library);

    auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip != nullptr);

    window.ShowTriage({});
    QVERIFY(!strip->isVisibleTo(&window));

    window.ShowTriage({.broken = 28, .conflicts = 2, .unmanaged = 178});
    QVERIFY(strip->isVisibleTo(&window));
}

void MainWindowTest::FoldersOutsideTheLibraryAloneLeaveTheStripDown()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage("Library", library);
    window.CarryTriageOn(library);

    const auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip != nullptr);

    window.ShowTriage({.unmanaged = 178});
    QVERIFY(!strip->isVisibleTo(&window));

    window.ShowTriage({.broken = 1, .unmanaged = 178});
    QVERIFY(strip->isVisibleTo(&window));

    window.ShowTriage({.unmanaged = 178});
    QVERIFY(!strip->isVisibleTo(&window));
}

void MainWindowTest::EachOfTheThreeThingsTheStripSaysBringsItUp_data()
{
    QTest::addColumn<std::size_t>("broken");
    QTest::addColumn<std::size_t>("conflicts");
    QTest::addColumn<std::size_t>("duplicated");

    QTest::newRow("a broken link") << std::size_t{1} << std::size_t{0} << std::size_t{0};
    QTest::newRow("a conflict") << std::size_t{0} << std::size_t{1} << std::size_t{0};
    QTest::newRow("a duplicated addon") << std::size_t{0} << std::size_t{0} << std::size_t{1};
}

void MainWindowTest::EachOfTheThreeThingsTheStripSaysBringsItUp()
{
    QFETCH(const std::size_t, broken);
    QFETCH(const std::size_t, conflicts);
    QFETCH(const std::size_t, duplicated);

    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage("Library", library);
    window.CarryTriageOn(library);

    const auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip != nullptr);

    window.ShowTriage({.broken = broken, .conflicts = conflicts, .duplicated = duplicated});

    QVERIFY(strip->isVisibleTo(&window));
}

void MainWindowTest::TheStripOnlyRidesOnThePagesThatCarryIt()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* journal = new QWidget(&window);
    window.AddPage("Library", library);
    PageTab* journalTab = window.AddPage("Journal", journal);
    window.CarryTriageOn(library);

    window.ShowTriage({.broken = 28});

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
    window.AddPage("Library", library);
    PageTab* journalTab = window.AddPage("Journal", journal);

    window.ShowSummary(library, QStringLiteral("346 addons · 27 enabled"));
    window.ShowSummary(journal, QStringLiteral("1204 operations recorded"));

    auto* footer = window.findChild<QLabel*>(QStringLiteral("FooterSummary"));
    QVERIFY(footer != nullptr);
    QCOMPARE(footer->text(), QStringLiteral("346 addons · 27 enabled"));

    journalTab->click();

    QCOMPARE(footer->text(), QStringLiteral("1204 operations recorded"));
}

void MainWindowTest::ThePageTabsSitInARowAndStillSwitchPages()
{
    MainWindow window(SettingsWithOneProfile());

    auto* first = new QWidget(&window);
    auto* second = new QWidget(&window);
    PageTab* firstTab = window.AddPage("Library", first);
    PageTab* secondTab = window.AddPage("Destinations", second);

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

    PageTab* tab = window.AddPage("Library", new QWidget(&window));
    const int bare = tab->sizeHint().width();

    tab->ShowCount(346);

    QCOMPARE(tab->Label(), QStringLiteral("Library"));
    QVERIFY(tab->text().contains(QStringLiteral("346")));
    QVERIFY(tab->sizeHint().width() > bare);

    tab->ShowCount(std::nullopt);

    QCOMPARE(tab->text(), QStringLiteral("Library"));
    QCOMPARE(tab->sizeHint().width(), bare);
}

void MainWindowTest::LeavingAPageTakesItsStatusMessageAway()
{
    MainWindow window(SettingsWithOneProfile());

    auto* first = new QWidget(&window);
    auto* second = new QWidget(&window);
    window.AddPage("Library", first);
    PageTab* secondTab = window.AddPage("Destinations", second);

    window.ShowSummary(first, QStringLiteral("346 addons · 27 enabled"));
    window.ShowSummary(second, QStringLiteral("244 entries"));

    auto* footer = window.findChild<QLabel*>(QStringLiteral("FooterSummary"));

    window.ShowStatus(QStringLiteral("12 operations finished."));
    QCOMPARE(footer->text(), QStringLiteral("12 operations finished."));

    secondTab->click();

    QCOMPARE(footer->text(), QStringLiteral("244 entries"));
}

void MainWindowTest::TheMeterFillsWithWhatThePageEnabled()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage("Library", library);

    auto* meter = window.findChild<QProgressBar*>(QStringLiteral("FooterMeter"));
    QVERIFY(meter != nullptr);
    QVERIFY(!meter->isVisibleTo(&window));

    window.ShowMeter(library, 27, 346);

    QVERIFY(meter->isVisibleTo(&window));
    QCOMPARE(meter->value(), 27);
    QCOMPARE(meter->maximum(), 346);
}

void MainWindowTest::TheFooterKeepsThePageGutterOnBothEdges()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    window.AddPage("Library", library);
    window.ShowSummary(library, QStringLiteral("346 addons · 27 enabled"));

    window.resize(1200, 800);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QStatusBar* bar = window.statusBar();
    auto* summary = window.findChild<QLabel*>(QStringLiteral("FooterSummary"));
    auto* aside = window.findChild<QLabel*>(QStringLiteral("FooterAside"));
    QVERIFY(summary != nullptr);
    QVERIFY(aside != nullptr);

    aside->setText(QStringLiteral("Flight Simulator 2024 profile"));
    QCoreApplication::processEvents();

    QCOMPARE(summary->mapTo(bar, QPoint()).x(), kPageGutter);
    QCOMPARE(bar->width() - aside->mapTo(bar, QPoint()).x() - aside->width(), kPageGutter);
}

void MainWindowTest::TheGearOpensTheOptionsAndTheBackButtonNamesWhereItCameFrom()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* community = new QWidget(&window);
    auto* options = new QWidget(&window);
    window.AddPage("Library", library);
    PageTab* communityTab = window.AddPage("Destinations", community);
    window.CarryOptionsOn(options);

    communityTab->click();

    auto* gear = window.findChild<QToolButton*>(QStringLiteral("Gear"));
    QVERIFY(gear != nullptr);

    QSignalSpy asked(&window, &MainWindow::OptionsRequested);
    gear->click();

    QVERIFY(window.ShowingOptions());
    QCOMPARE(asked.count(), 1);
    QCOMPARE(window.findChild<QStackedWidget*>()->currentWidget(), options);

    QVERIFY(!communityTab->isVisibleTo(&window));

    const auto* back = window.findChild<PageTab*>(QStringLiteral("BackTab"));
    QVERIFY(back != nullptr);
    QVERIFY(back->isVisibleTo(&window));
    QCOMPARE(back->Label(), QStringLiteral("Back to Destinations"));
}

void MainWindowTest::LeavingTheOptionsGivesBackThePageThatWasOpen()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* community = new QWidget(&window);
    auto* options = new QWidget(&window);
    window.AddPage("Library", library);
    PageTab* communityTab = window.AddPage("Destinations", community);
    window.CarryOptionsOn(options);

    communityTab->click();
    window.ShowOptions();

    QSignalSpy landed(&window, &MainWindow::PageSelected);
    window.LeaveOptions();

    QVERIFY(!window.ShowingOptions());
    QCOMPARE(window.findChild<QStackedWidget*>()->currentWidget(), community);
    QCOMPARE(landed.count(), 1);
    QCOMPARE(landed.front().front().value<QWidget*>(), community);
    QVERIFY(communityTab->isVisibleTo(&window));
}

void MainWindowTest::TheTriageStripStandsDownWhileTheOptionsAreOpen()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* options = new QWidget(&window);
    window.AddPage("Library", library);
    window.CarryOptionsOn(options);
    window.CarryTriageOn(library);

    window.ShowTriage({.broken = 28});

    auto* strip = window.findChild<TriageStrip*>();
    QVERIFY(strip->isVisibleTo(&window));

    window.ShowOptions();
    QVERIFY(!strip->isVisibleTo(&window));

    window.LeaveOptions();
    QVERIFY(strip->isVisibleTo(&window));
}

void MainWindowTest::ClickingBackFromTheOptionsLeavesTheOriginTabStillMarked()
{
    MainWindow window(SettingsWithOneProfile());

    auto* library = new QWidget(&window);
    auto* community = new QWidget(&window);
    auto* options = new QWidget(&window);
    window.AddPage("Library", library);
    PageTab* communityTab = window.AddPage("Destinations", community);
    window.CarryOptionsOn(options);

    communityTab->click();
    QVERIFY2(communityTab->isChecked(),
             "the origin tab did not end up checked when clicked, so demanding it checked afterwards proves nothing");

    window.ShowOptions();

    auto* back = window.findChild<PageTab*>(QStringLiteral("BackTab"));

    QVERIFY2(back != nullptr, "the back tab was not found, so the click under test is not the user's");

    back->click();

    QVERIFY(!window.ShowingOptions());
    QVERIFY2(communityTab->isChecked(), "coming back from the options left the tab strip with none checked");
}

void MainWindowTest::TheChromeFitsTheNarrowestWindow()
{
    MainWindow window(SettingsWithOneProfile());

    ItFitsTheNarrowestWindow(window, "The window chrome");
}

void MainWindowTest::NothingOffersAnUpdateUntilOneIsOffered()
{
    const MainWindow window(SettingsWithOneProfile());

    auto* offer = window.findChild<QPushButton*>(QStringLiteral("UpdateOffer"));
    QVERIFY(offer != nullptr);
    QVERIFY2(offer->isHidden(), "the window offered an update before anyone said there was one");
}

void MainWindowTest::TheOfferNamesTheVersionAndTurnsIntoARestartWhenItIsStaged()
{
    MainWindow window(SettingsWithOneProfile());
    auto* offer = window.findChild<QPushButton*>(QStringLiteral("UpdateOffer"));
    QSignalSpy chosen(&window, &MainWindow::UpdateOfferChosen);

    window.ShowUpdateOffer(UpdateOffer::Available, QStringLiteral("0.49.0"));

    QVERIFY(!offer->isHidden());
    QVERIFY2(offer->text().contains(QStringLiteral("0.49.0")), "the offer did not say which version it was offering");

    const QString offering = offer->text();

    window.ShowUpdateOffer(UpdateOffer::Staged, QStringLiteral("0.49.0"));

    QVERIFY2(offer->text() != offering, "a staged update reads the same as one that still has to be downloaded");

    offer->click();
    QCOMPARE(chosen.count(), 1);

    window.ShowUpdateOffer(UpdateOffer::None, {});

    QVERIFY(offer->isHidden());
}

void MainWindowTest::TheBackTabPointsLeftWithoutAGlyphInItsLabel()
{
    MainWindow window(SettingsWithOneProfile());

    auto* community = new QWidget(&window);
    window.AddPage("Destinations", community)->click();
    window.CarryOptionsOn(new QWidget(&window));
    window.ShowOptions();

    auto* back = window.findChild<PageTab*>(QStringLiteral("BackTab"));
    QVERIFY(back != nullptr);
    QVERIFY(!back->Label().contains(QChar(0x2190)));

    PageTab pointing(back->Label());
    pointing.LeadWith(ArrowHeading::Left);

    PageTab bare(back->Label());

    const QSize size = pointing.sizeHint();

    QCOMPARE(back->sizeHint(), size);
    QVERIFY2(Rendered(*back, size) == Rendered(pointing, size), "the back tab does not draw an arrow pointing left");
    QVERIFY2(Rendered(*back, size) != Rendered(bare, size), "the back tab draws nothing before its name");
}

void MainWindowTest::TheOfferPointsDownWhileThereIsSomethingToDownload()
{
    MainWindow window(SettingsWithOneProfile());
    auto* offer = window.findChild<ArrowButton*>(QStringLiteral("UpdateOffer"));
    QVERIFY(offer != nullptr);

    window.ShowUpdateOffer(UpdateOffer::Available, QStringLiteral("0.49.0"));

    QCOMPARE(offer->text(), QStringLiteral("v0.49.0"));

    ArrowButton pointing;
    pointing.setText(offer->text());
    pointing.LeadWith(ArrowHeading::Down);
    pointing.setProperty("role", offer->property("role"));
    pointing.setFixedHeight(offer->height());

    ArrowButton bare;
    bare.setText(offer->text());
    bare.setProperty("role", offer->property("role"));
    bare.setFixedHeight(offer->height());

    const QSize size(pointing.sizeHint().width(), offer->height());

    QCOMPARE(offer->sizeHint(), pointing.sizeHint());
    QVERIFY2(Rendered(*offer, size) == Rendered(pointing, size), "the offer does not draw an arrow pointing down");
    QVERIFY2(Rendered(*offer, size) != Rendered(bare, size), "the offer draws nothing before its version");
}

void MainWindowTest::ADownloadedOfferCarriesNoArrow()
{
    MainWindow window(SettingsWithOneProfile());
    auto* offer = window.findChild<ArrowButton*>(QStringLiteral("UpdateOffer"));
    QVERIFY(offer != nullptr);

    window.ShowUpdateOffer(UpdateOffer::Available, QStringLiteral("0.49.0"));
    window.ShowUpdateOffer(UpdateOffer::Staged, QStringLiteral("0.49.0"));

    const QPushButton plain(offer->text());

    QCOMPARE(offer->sizeHint(), plain.sizeHint());

    ArrowButton bare;
    bare.setText(offer->text());
    bare.setProperty("role", offer->property("role"));
    bare.setFixedHeight(offer->height());

    const QSize size(plain.sizeHint().width(), offer->height());

    QVERIFY2(Rendered(*offer, size) == Rendered(bare, size), "a staged update still carries the arrow of the download");
}

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
