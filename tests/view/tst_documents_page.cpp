#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtPdfWidgets/QPdfView>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSplitter>

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeChartCatalogueParser.h"
#include "tests/doubles/FakeChartVersions.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeDocumentIndexCache.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/APdf.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/documents/DocumentReader.h"
#include "view/documents/DocumentsPage.h"
#include "viewmodel/DocumentsViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DocumentsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void OneClickOnTheLineOpensIt();
        static void TheStarTurnsTheFavouriteWithoutOpeningAnything();
        static void OnlyTheGlyphOfTheArrowOpensAndClosesTheGroup();
        static void TheBarSwapsTheListAndLeavesOpenWhatWasOpen();
        static void TheNameIsCutInTheMiddleBecauseWhatTellsTwoApartIsAtTheEnd();
        static void DetachingLeavesTheTabAsItWasAndMarksThePlaceItLeft();
        static void TheDetachedWindowShowsTheReadingAndNotAnEmptyPane();
        static void TheAddonAsksForThePanelThatCarriesIt();
        static void TheIndexKeepsTheWidthTheFormWasMeasuredAt();
        static void TheIndexRunsToTheEdgeOfThePageLikeEveryOtherList();
        static void TheProgressAppearsWhenTheReadingStartsAndGoesWhenItEnds();
        static void TheWheelZoomsTheChartFromTheStartAndTheManualOnlyIfAsked();
        static void TheWheelStopsZoomingWhenTheReaderIsToldItShouldNot();
        static void TheWheelSwitchSaysWhichOfTheTwoKindsItIsAbout();
        static void EachKindCarriesItsOwnPairOfSwitches();
        static void DraggingMovesADocumentAndNotOnlyAChart();
        static void APointerThatDoesNotWanderLeavesThePageWhereItWas();
        static void DraggingMovesNothingWhenTheReaderIsToldItShouldNot();
        static void ThePointerSaysWhichOfTheTwoOfficesTheLeftButtonIsOn();
        static void ThePointerSaysWhenTheReadingCarriesALink();
        static void ThePaneMarksTheSectionThatHoldsThePageTheReadingIsOn();
        static void AMarkHangsUnderItsSectionOnABranchBornOpenAndLightsTheButton();
        static void TheButtonSaysWhichPageIsBeingMarkedAndWhichWayItWent();
        static void ADerivedMarkIsNamedByItsPageAndANameTheUserGaveWins();
        static void ADocumentWithoutAnOutlineShowsThePaneOnceItCarriesAMark();
        static void TheMenuAnswersOnAMarkAndOnNothingElse();
        static void TheSearchStepsForwardAndBackThroughWhatItFound();
        static void SteppingToAMatchFurtherDownTheSamePageScrollsToIt();
        static void TheLensesTakeTheReadingCloserAndFurtherAway();
        static void TheMarkMenuOpensOnTheMarkAndNotOnTheEmptySpaceBelowIt();
        static void TheMarkTheReaderTurnsIsKeptWithTheDocumentThatIsOpen();
        static void SayingNoToTheQuestionLeavesTheMarkWhereItIs();
        static void AReaderOnItsWayOutStopsAnsweringThePagesItIsTakingWithIt();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");
    const std::string kBrussels = "aerosoft-airport-ebbr-brussels";
    const std::string kCrj = "aerosoft-crj";
    const std::string kCatalogueOfBrussels = "the catalogue of Brussels";

    [[nodiscard]] std::filesystem::path FolderOf(const std::string& folderName)
    {
        return PathUnder(kLibrary, PathFromUtf8(folderName));
    }

    [[nodiscard]] SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.libraries = {{.id = "library-1", .path = kLibrary, .label = "Library"}};

        return profile;
    }

    [[nodiscard]] TreeNode AddonNamed(const std::string& folderName)
    {
        return {.kind = TreeNodeKind::Addon,
                .path = FolderOf(folderName),
                .addon = Addon{},
                .children = {},
                .declaredAsCategory = false};
    }

    [[nodiscard]] ChartCatalogue TheBrusselsCatalogue()
    {
        return {.icao = "EBBR",
                .entries = {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                            {.chartId = "53206", .chartType = "IAC", .chartName = "ILS or LOC Y 25L"}}};
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kLibrary);

            catalog.SetTree(kLibrary,
                            TreeNode{.kind = TreeNodeKind::Library,
                                     .path = kLibrary,
                                     .addon = {},
                                     .children = {AddonNamed(kBrussels), AddonNamed(kCrj)},
                                     .declaredAsCategory = true});

            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "scenery" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"EBBR"}));

            fileSystem.AddFile(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53117.pdf");
            fileSystem.AddFile(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53206.pdf");
            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "catalogue.json",
                                           kCatalogueOfBrussels);
            catalogueParser.Answer(kCatalogueOfBrussels, TheBrusselsCatalogue());

            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol2_Quick Reference Guide_550_700.pdf");
            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol4_Normal Ops Checklist.pdf");

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};
        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        FakeChartCatalogueParser catalogueParser;
        FakeChartVersions chartVersions;
        DocumentService documents{catalog, filesystemProbe, catalogueParser, chartVersions};
        FakeDocumentIndexCache cache;
        DocumentsViewModel viewModel{documents, scenery, session, runner, cache, clock};
    };

    [[nodiscard]] QTreeWidget* TheIndexOf(const DocumentsPage& page, const DocumentPanel panel)
    {
        return page.findChild<QTreeWidget*>(panel == DocumentPanel::Documents ? QStringLiteral("DocumentsIndex")
                                                                              : QStringLiteral("ChartsIndex"));
    }

    [[nodiscard]] QString WhatIsOpen(const DocumentsPage& page)
    {
        const QLabel* caption = page.findChild<QLabel*>(QStringLiteral("ReadingCaption"));

        return caption == nullptr ? QString() : caption->text();
    }

    [[nodiscard]] QTreeWidgetItem* GroupNamed(const QTreeWidget& index, const QString& name)
    {
        for (int group = 0; group < index.topLevelItemCount(); ++group)
        {
            if (index.topLevelItem(group)->text(1) == name)
            {
                return index.topLevelItem(group);
            }
        }

        return nullptr;
    }

    [[nodiscard]] QPushButton* ButtonSaying(const QWidget& page, const QString& text)
    {
        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text().startsWith(text))
            {
                return button;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool SomethingSays(const QWidget& page, const QString& text)
    {
        for (const QLabel* said : page.findChildren<QLabel*>(QStringLiteral("EmptyBody")))
        {
            if (said->text().contains(text))
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool ItSaysItIsReading(const DocumentsPage& page)
    {
        const QPushButton* stop = ButtonSaying(page, QStringLiteral("Stop"));

        return stop != nullptr && stop->isVisible();
    }

    void ClickAt(QTreeWidget& index, const QPoint& where)
    {
        QTest::mouseClick(index.viewport(), Qt::LeftButton, {}, where);
    }

    void RightClickAt(QTreeWidget& index, const QPoint& where)
    {
        QContextMenuEvent asked(QContextMenuEvent::Mouse, where, index.viewport()->mapToGlobal(where));

        QApplication::sendEvent(index.viewport(), &asked);
    }

    [[nodiscard]] bool WhatTheMenuAsks(const QMessageBox::StandardButton answer, QAction& action)
    {
        bool asked = false;

        QTimer::singleShot(0,
                           [&asked, answer]
                           {
                               auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());

                               if (question == nullptr)
                               {
                                   return;
                               }

                               asked = true;
                               question->button(answer)->click();
                           });

        action.trigger();

        return asked;
    }

    const std::vector<ASectionOfAManual> kChapters = {{.title = "Introduction", .page = 0},
                                                      {.title = "Systems", .page = 10},
                                                      {.title = "Hydraulics", .page = 12},
                                                      {.title = "Limitations", .page = 20}};

    [[nodiscard]] std::filesystem::path
    WrittenInto(const QTemporaryDir& folder, const std::wstring& named, const std::string& bytes)
    {
        const std::filesystem::path file = std::filesystem::path(folder.path().toStdWString()) / named;

        std::ofstream written(file, std::ios::binary);
        written.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

        return file;
    }

    [[nodiscard]] QTreeWidget* ThePaneOf(const DocumentReader& reader)
    {
        return reader.findChild<QTreeWidget*>(QStringLiteral("ReadingOutline"));
    }

    [[nodiscard]] QPushButton* TheMarkButtonOf(const DocumentReader& reader)
    {
        return reader.findChild<QPushButton*>(QStringLiteral("BookmarkThePage"));
    }

    [[nodiscard]] QTreeWidgetItem* SectionNamed(const QTreeWidget& pane, const QString& name)
    {
        for (int top = 0; top < pane.topLevelItemCount(); ++top)
        {
            if (pane.topLevelItem(top)->text(0) == name)
            {
                return pane.topLevelItem(top);
            }
        }

        return nullptr;
    }
}

void DocumentsPageTest::OneClickOnTheLineOpensIt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);

    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));

    QVERIFY(crj != nullptr);
    crj->setExpanded(true);

    QTreeWidgetItem* line = crj->child(0);
    const QRect name = index->visualItemRect(line);

    QVERIFY(WhatIsOpen(page).isEmpty());

    ClickAt(*index, QPoint(name.center().x(), name.center().y()));

    QVERIFY2(WhatIsOpen(page).contains(QStringLiteral("Vol2_Quick Reference Guide_550_700")),
             "the line offers one action, so one click on it does that action");
}

void DocumentsPageTest::TheStarTurnsTheFavouriteWithoutOpeningAnything()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));
    crj->setExpanded(true);

    const QRect star = index->visualItemRect(crj->child(0));

    ClickAt(*index, QPoint(star.left() + 4, star.center().y()));

    QCOMPARE(f.settings.stored.documents.size(), std::size_t{1});
    QVERIFY(f.settings.stored.documents.front().favourite);
    QVERIFY2(WhatIsOpen(page).isEmpty(), "the star is the one column that does not open the document");
}

void DocumentsPageTest::OnlyTheGlyphOfTheArrowOpensAndClosesTheGroup()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));
    const QRect glyph = index->visualItemRect(crj);

    QVERIFY(!crj->isExpanded());

    ClickAt(*index, QPoint(glyph.right() - 3, glyph.center().y()));

    QVERIFY2(!crj->isExpanded(),
             "the column is sized by its content and the glyph fills a fraction of it, so the rest of it answering "
             "would be a target with nothing drawn on it");

    ClickAt(*index, QPoint(glyph.left() + 3, glyph.center().y()));

    QVERIFY(crj->isExpanded());
}

void DocumentsPageTest::TheBarSwapsTheListAndLeavesOpenWhatWasOpen()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    QTreeWidget* documents = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*documents, QString::fromStdString(kCrj));
    crj->setExpanded(true);

    ClickAt(*documents, documents->visualItemRect(crj->child(0)).center());

    const QString reading = WhatIsOpen(page);
    QVERIFY(!reading.isEmpty());

    page.Show(DocumentPanel::Charts);

    QVERIFY(TheIndexOf(page, DocumentPanel::Charts)->isVisible());
    QVERIFY(!documents->isVisible());
    QCOMPARE(WhatIsOpen(page), reading);
}

void DocumentsPageTest::TheNameIsCutInTheMiddleBecauseWhatTellsTwoApartIsAtTheEnd()
{
    Fixture f;
    const DocumentsPage page(f.viewModel);

    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->textElideMode(), Qt::ElideMiddle);
    QCOMPARE(TheIndexOf(page, DocumentPanel::Charts)->textElideMode(), Qt::ElideMiddle);
}

void DocumentsPageTest::TheDetachedWindowShowsTheReadingAndNotAnEmptyPane()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    QTreeWidget* documents = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*documents, QString::fromStdString(kCrj));
    crj->setExpanded(true);
    ClickAt(*documents, documents->visualItemRect(crj->child(0)).center());

    page.DetachTheReading();

    const QDialog* window = page.findChild<QDialog*>();
    const DocumentReader* reading = page.findChild<DocumentReader*>();

    QVERIFY(window != nullptr);
    QVERIFY(reading != nullptr);
    QCOMPARE(reading->parentWidget(), window);
    QVERIFY2(reading->isVisible(),
             "the stack hides whatever it removes, so a window that only reparents the reading opens on nothing");
}

void DocumentsPageTest::DetachingLeavesTheTabAsItWasAndMarksThePlaceItLeft()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    QTreeWidget* documents = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*documents, QString::fromStdString(kCrj));
    crj->setExpanded(true);
    ClickAt(*documents, documents->visualItemRect(crj->child(0)).center());

    const QString reading = WhatIsOpen(page);
    const int wide = documents->width();

    page.DetachTheReading();

    QVERIFY2(ButtonSaying(page, QStringLiteral("Documents")) != nullptr,
             "the bar that swaps the panels stays, so nobody comes back to a screen they did not leave");
    QCOMPARE(documents->width(), wide);

    QPushButton* back = ButtonSaying(page, QStringLiteral("Bring it back"));

    QVERIFY2(back != nullptr, "a card marks the place the reading left, and it is the way back");
    QVERIFY(SomethingSays(page, reading));

    back->click();

    QCOMPARE(WhatIsOpen(page), reading);
    QVERIFY2(!ButtonSaying(page, QStringLiteral("Bring it back"))->isVisible(),
             "bringing it back gives the place to the reading it marked");
}

void DocumentsPageTest::TheAddonAsksForThePanelThatCarriesIt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    page.Reveal(kBrussels);

    QVERIFY(TheIndexOf(page, DocumentPanel::Charts)->isVisible());
    QCOMPARE(TheIndexOf(page, DocumentPanel::Charts)->currentItem()->text(1), QStringLiteral("EBBR"));

    page.Reveal(kCrj);

    QVERIFY(TheIndexOf(page, DocumentPanel::Documents)->isVisible());
    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->currentItem()->text(1), QString::fromStdString(kCrj));
}

void DocumentsPageTest::TheIndexRunsToTheEdgeOfThePageLikeEveryOtherList()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1450, 760);
    page.show();
    f.viewModel.ReadTheLibrary();

    const QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    const QSplitter* split = page.findChild<QSplitter*>();

    QVERIFY(index != nullptr);
    QVERIFY(split != nullptr);
    QCOMPARE(index->mapTo(&page, QPoint(0, 0)).x(), 0);
    QCOMPARE(split->mapTo(&page, QPoint(0, split->height())).y(), page.height());
    QVERIFY2(!index->header()->stretchLastSection(),
             "the last column stretching is what keeps the count from reaching the scrollbar");
}

void DocumentsPageTest::TheIndexKeepsTheWidthTheFormWasMeasuredAt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1450, 760);
    page.show();
    f.viewModel.ReadTheLibrary();

    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->width(), 340);
}

void DocumentsPageTest::TheProgressAppearsWhenTheReadingStartsAndGoesWhenItEnds()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();

    QVERIFY2(!ItSaysItIsReading(page), "a tab that was never told to read is not a tab that is reading");

    f.runner.defer = true;
    f.viewModel.ReadTheLibrary();

    QVERIFY2(ItSaysItIsReading(page),
             "the scenery half runs before the first addon is indexed, so waiting for the first count leaves the "
             "screen saying nothing for as long as it takes");

    f.runner.Finish();

    QVERIFY(!ItSaysItIsReading(page));
}

void DocumentsPageTest::ThePointerSaysWhenTheReadingCarriesALink()
{
    const QTemporaryDir folder;
    const std::filesystem::path linked = WrittenInto(folder, L"linked.pdf", AManualWhoseFirstPageIsALinkToTheSecond());

    DocumentReader reader;
    reader.resize(600, 500);
    reader.show();
    reader.Read(linked, 0, DocumentKind::Document, {});

    QPdfView* pages = reader.findChild<QPdfView*>();
    auto* fitWidth = reader.findChild<QPushButton*>(QStringLiteral("FitTheWidth"));

    QVERIFY(pages != nullptr);
    QVERIFY(fitWidth != nullptr);

    fitWidth->setChecked(true);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);

    const QPointF onTheLink(300, 120);
    QMouseEvent moved(QEvent::MouseMove, onTheLink, pages->viewport()->mapToGlobal(onTheLink), Qt::NoButton, {},
                      Qt::NoModifier);

    QCoreApplication::sendEvent(pages->viewport(), &moved);

    QVERIFY2(pages->cursor().shape() == Qt::PointingHandCursor,
             "the page under the pointer is one big link, so the view itself found it");
    QCOMPARE(pages->viewport()->cursor().shape(), Qt::PointingHandCursor);
}

void DocumentsPageTest::TheWheelZoomsTheChartFromTheStartAndTheManualOnlyIfAsked()
{
    const QTemporaryDir folder;
    const std::filesystem::path chart = std::filesystem::path(folder.path().toStdWString()) / L"53117.pdf";

    std::ofstream written(chart, std::ios::binary);
    const std::string bytes = APdfWhoseInfoSays("/Title(CV-1)");
    written.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    written.close();

    DocumentReader reader;
    reader.resize(600, 400);
    reader.show();

    QPdfView* pages = reader.findChild<QPdfView*>();
    QVERIFY(pages != nullptr);

    const auto roll = [pages]
    {
        QWheelEvent turned(QPointF(100, 100), pages->viewport()->mapToGlobal(QPointF(100, 100)), {}, QPoint(0, 120),
                           Qt::NoButton, {}, Qt::NoScrollPhase, false);

        QCoreApplication::sendEvent(pages->viewport(), &turned);
    };

    reader.Read(chart, 0, DocumentKind::Document, {});
    const qreal overTheManual = pages->zoomFactor();
    roll();

    QVERIFY2(qFuzzyCompare(pages->zoomFactor(), overTheManual),
             "a manual is read going down, so the wheel is born scrolling it and the roll changes no zoom");

    reader.Read(chart, 0, DocumentKind::Chart, {});
    const qreal overTheChart = pages->zoomFactor();
    roll();

    QVERIFY2(pages->zoomFactor() > overTheChart,
             "a chart is read by getting closer to it, so there the wheel is born zooming");

    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = true, .dragMovesThePage = false});
    reader.Read(chart, 0, DocumentKind::Document, {});

    const qreal onceAsked = pages->zoomFactor();
    roll();

    QVERIFY2(pages->zoomFactor() > onceAsked,
             "and asking for it over a manual works, which is what the switch was doing nothing about");
}

namespace
{
    void Roll(QPdfView& pages)
    {
        QWheelEvent turned(QPointF(100, 100), pages.viewport()->mapToGlobal(QPointF(100, 100)), {}, QPoint(0, 120),
                           Qt::NoButton, {}, Qt::NoScrollPhase, false);

        QCoreApplication::sendEvent(pages.viewport(), &turned);
    }

    void PointerAt(QPdfView& pages, const QEvent::Type what, const QPointF& where)
    {
        QMouseEvent moved(what, where, pages.viewport()->mapToGlobal(where), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);

        QCoreApplication::sendEvent(pages.viewport(), &moved);
    }

    [[nodiscard]] QPdfView* AManualOpenedIn(DocumentReader& reader, const std::filesystem::path& manual)
    {
        reader.resize(900, 600);
        reader.show();
        reader.Read(manual, 0, DocumentKind::Document, {});

        QPdfView* pages = reader.findChild<QPdfView*>();

        if (pages == nullptr)
        {
            return nullptr;
        }

        return pages->verticalScrollBar()->maximum() > 0 ? pages : nullptr;
    }
}

void DocumentsPageTest::TheWheelStopsZoomingWhenTheReaderIsToldItShouldNot()
{
    const QTemporaryDir folder;
    const std::filesystem::path chart = WrittenInto(folder, L"53117.pdf", APdfWhoseInfoSays("/Title(CV-1)"));

    DocumentReader reader;
    reader.resize(600, 400);
    reader.show();
    reader.SayTheGesturesOf(DocumentKind::Chart, {.wheelZooms = false, .dragMovesThePage = true});
    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = true});

    QPdfView* pages = reader.findChild<QPdfView*>();

    QVERIFY(pages != nullptr);

    reader.Read(chart, 0, DocumentKind::Chart, {});

    const qreal told = pages->zoomFactor();

    Roll(*pages);

    QVERIFY2(qFuzzyCompare(pages->zoomFactor(), told), "told not to, the wheel leaves the chart where it was");

    reader.SayTheGesturesOf(DocumentKind::Chart, {.wheelZooms = true, .dragMovesThePage = true});
    Roll(*pages);

    QVERIFY2(pages->zoomFactor() > told, "and told to, it takes the same chart closer");
}

void DocumentsPageTest::TheWheelSwitchSaysWhichOfTheTwoKindsItIsAbout()
{
    const QTemporaryDir folder;
    const std::filesystem::path both = WrittenInto(folder, L"53117.pdf", APdfWhoseInfoSays("/Title(CV-1)"));

    DocumentReader reader;
    reader.resize(600, 400);
    reader.show();

    const QPushButton* wheel = reader.findChild<QPushButton*>(QStringLiteral("WheelZooms"));

    QVERIFY(wheel != nullptr);

    reader.Read(both, 0, DocumentKind::Chart, {});

    const QString overAChart = wheel->toolTip();

    reader.Read(both, 0, DocumentKind::Document, {});

    QVERIFY2(wheel->toolTip() != overAChart,
             "the switch is the same button on both panels, so a tip that never changes calls a manual a chart");
}

void DocumentsPageTest::EachKindCarriesItsOwnPairOfSwitches()
{
    const QTemporaryDir folder;
    const std::filesystem::path both = WrittenInto(folder, L"53117.pdf", APdfWhoseInfoSays("/Title(CV-1)"));

    DocumentReader reader;
    reader.resize(600, 400);
    reader.show();

    reader.SayTheGesturesOf(DocumentKind::Chart, {.wheelZooms = true, .dragMovesThePage = false});
    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = true});

    auto* wheel = reader.findChild<QPushButton*>(QStringLiteral("WheelZooms"));
    auto* drag = reader.findChild<QPushButton*>(QStringLiteral("DragMovesThePage"));

    QVERIFY(wheel != nullptr);
    QVERIFY(drag != nullptr);

    reader.Read(both, 0, DocumentKind::Chart, {});

    QVERIFY2(wheel->isChecked() && !drag->isChecked(), "the chart shows the pair that belongs to the chart");

    reader.Read(both, 0, DocumentKind::Document, {});

    QVERIFY2(!wheel->isChecked() && drag->isChecked(),
             "and opening a manual swaps both switches, because the answer the chart gave is not an answer about "
             "the manual");

    wheel->click();

    reader.Read(both, 0, DocumentKind::Chart, {});

    QVERIFY2(wheel->isChecked(),
             "turning it on over a manual left the chart where it was, which is the whole point of the two pairs");

    QSignalSpy asked(&reader, &DocumentReader::TheWheelWasSetToZoom);

    wheel->click();

    QCOMPARE(asked.count(), 1);
    QVERIFY2(asked.first().at(0).value<DocumentKind>() == DocumentKind::Chart,
             "the switch says which kind it answered for, and Document is the zero of the enum, so only asking for "
             "the chart tells a carried value apart from a conversion that gave up");
}

void DocumentsPageTest::DraggingMovesADocumentAndNotOnlyAChart()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    QPdfView* pages = AManualOpenedIn(reader, manual);

    QVERIFY2(pages != nullptr, "the manual has to be taller than the reader for a drag to have anywhere to go");

    const int born = pages->verticalScrollBar()->value();

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 200));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 200));

    QVERIFY2(pages->verticalScrollBar()->value() == born,
             "a manual is born without the drag, because the gesture that reads it is the scroll");

    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = true});

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 200));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 200));

    QVERIFY2(pages->verticalScrollBar()->value() > born,
             "and asked for, dragging the page upwards walks the reading forward on a manual too");
}

void DocumentsPageTest::APointerThatDoesNotWanderLeavesThePageWhereItWas()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    QPdfView* pages = AManualOpenedIn(reader, manual);

    QVERIFY(pages != nullptr);

    const int before = pages->verticalScrollBar()->value();

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 300 - QApplication::startDragDistance() + 1));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 300 - QApplication::startDragDistance() + 1));

    QCOMPARE(pages->verticalScrollBar()->value(), before);

    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 100));

    QVERIFY2(pages->verticalScrollBar()->value() == before,
             "the grab ended with the release, so a pointer wandering afterwards moves nothing");
}

void DocumentsPageTest::DraggingMovesNothingWhenTheReaderIsToldItShouldNot()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    QPdfView* pages = AManualOpenedIn(reader, manual);

    QVERIFY(pages != nullptr);

    reader.SayTheGesturesOf(DocumentKind::Chart, {.wheelZooms = false, .dragMovesThePage = false});
    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = false});

    const int before = pages->verticalScrollBar()->value();

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 200));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 200));

    QCOMPARE(pages->verticalScrollBar()->value(), before);
}

void DocumentsPageTest::ThePointerSaysWhichOfTheTwoOfficesTheLeftButtonIsOn()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    QPdfView* pages = AManualOpenedIn(reader, manual);

    QVERIFY(pages != nullptr);

    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = true});

    QCOMPARE(pages->viewport()->cursor().shape(), Qt::OpenHandCursor);

    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY2(pages->viewport()->cursor().shape() != Qt::OpenHandCursor,
             "the hand is what says the drag walks the page, so it cannot stay on when the drag marks text instead");

    reader.SayTheGesturesOf(DocumentKind::Document, {.wheelZooms = false, .dragMovesThePage = true});

    QVERIFY2(pages->viewport()->cursor().shape() == Qt::OpenHandCursor,
             "turning it back on has to repaint the pointer, and the path that only stopped the grabbing did not");
}

void DocumentsPageTest::ThePaneMarksTheSectionThatHoldsThePageTheReadingIsOn()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(manual, 13, DocumentKind::Document, {});

    QTreeWidget* pane = ThePaneOf(reader);

    QVERIFY(pane != nullptr);
    QCOMPARE(pane->topLevelItemCount(), 4);
    QVERIFY2(SectionNamed(*pane, QStringLiteral("Hydraulics"))->font(0).bold(),
             "the reading is on page 14 of a manual whose Hydraulics starts on 13, and the panel is what says so "
             "without the reader counting pages");
    QVERIFY2(!SectionNamed(*pane, QStringLiteral("Systems"))->font(0).bold(), "only one section holds the page");
    QVERIFY2(pane->currentItem() == nullptr,
             "where the reading is and what the context menu will act on are two things, and giving both to the "
             "selection makes each one destroy the other");
}

void DocumentsPageTest::AMarkHangsUnderItsSectionOnABranchBornOpenAndLightsTheButton()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(manual, 13, DocumentKind::Document, {});

    QVERIFY(!TheMarkButtonOf(reader)->isChecked());

    reader.ShowTheBookmarks({{.page = 13, .name = {}}});

    QTreeWidgetItem* hydraulics = SectionNamed(*ThePaneOf(reader), QStringLiteral("Hydraulics"));

    QVERIFY(hydraulics != nullptr);
    QCOMPARE(hydraulics->childCount(), 1);
    QVERIFY2(hydraulics->child(0)->text(0) == QString::fromUtf8("● Page 14"),
             "the disc is what tells a mark from the star of the index, and the page is what tells it from the "
             "heading it hangs under");
    QVERIFY2(hydraulics->isExpanded(), "a mark inside a closed chapter is a mark nobody finds");
    QVERIFY(TheMarkButtonOf(reader)->isChecked());
    QVERIFY2(TheMarkButtonOf(reader)->property("toggle").toString() == QStringLiteral("true"),
             "a checkable button carries no look of its own in this theme, and this property is what the stylesheet "
             "rule for a toggle keys on");
}

void DocumentsPageTest::TheButtonSaysWhichPageIsBeingMarkedAndWhichWayItWent()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(manual, 13, DocumentKind::Document, {});

    QSignalSpy turned(&reader, &DocumentReader::TheMarkOfThePageWasTurned);

    QTest::mouseClick(TheMarkButtonOf(reader), Qt::LeftButton);

    QCOMPARE(turned.count(), 1);
    QCOMPARE(turned.front().at(0).toInt(), 13);
    QVERIFY(turned.front().at(1).toBool());

    reader.ShowTheBookmarks({{.page = 13, .name = {}}});

    QVERIFY2(TheMarkButtonOf(reader)->isChecked(),
             "the page it lands back on carries the mark that was just made, so the button stays lit without the "
             "press being what lit it");

    QTest::mouseClick(TheMarkButtonOf(reader), Qt::LeftButton);

    QCOMPARE(turned.count(), 2);
    QVERIFY2(!turned.back().at(1).toBool(), "the button toggles, so the second press on a marked page takes it away");

    reader.ShowTheBookmarks({});

    QVERIFY(!TheMarkButtonOf(reader)->isChecked());
}

void DocumentsPageTest::ADerivedMarkIsNamedByItsPageAndANameTheUserGaveWins()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(manual, 13, DocumentKind::Document,
                {{.page = 14, .name = {}}, {.page = 13, .name = "Where I stopped"}});

    QTreeWidgetItem* hydraulics = SectionNamed(*ThePaneOf(reader), QStringLiteral("Hydraulics"));

    QVERIFY(hydraulics != nullptr);
    QCOMPARE(hydraulics->childCount(), 2);
    QVERIFY2(hydraulics->child(0)->text(0) == QString::fromUtf8("● Where I stopped"),
             "a name the user gave is the one thing here that is written down, so it wins over anything derived");
    QVERIFY2(hydraulics->child(1)->text(0) == QString::fromUtf8("● Page 15"),
             "the nesting already says which section holds the mark, so the name carries the one fact it does not, "
             "and the page is unique because two marks never share one");
}

void DocumentsPageTest::ADocumentWithoutAnOutlineShowsThePaneOnceItCarriesAMark()
{
    const QTemporaryDir folder;
    const std::filesystem::path volume = WrittenInto(folder, L"vol1.pdf", AManualOf(59, {}));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(volume, 40, DocumentKind::Document, {});

    QTreeWidget* pane = ThePaneOf(reader);

    QVERIFY2(!pane->parentWidget()->isVisibleTo(&reader),
             "with neither outline nor mark the panel says nothing, and the page is what the reader is for");

    reader.ShowTheBookmarks({{.page = 40, .name = {}}});

    QVERIFY2(pane->parentWidget()->isVisibleTo(&reader),
             "13 of the 26 long documents of the library carry no outline, and this is the only place a mark can "
             "live in them");
    QCOMPARE(pane->topLevelItemCount(), 1);
    QCOMPARE(pane->topLevelItem(0)->text(0), QString::fromUtf8("● Page 41"));
    QCOMPARE(reader.findChild<QLabel*>(QStringLiteral("PanelSubHeading"))->text(), QStringLiteral("Bookmarks"));
}

void DocumentsPageTest::TheMarkMenuOpensOnTheMarkAndNotOnTheEmptySpaceBelowIt()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);
    reader.show();

    reader.Read(manual, 13, DocumentKind::Document, {{.page = 13, .name = {}}});

    QTreeWidget* pane = ThePaneOf(reader);
    QTreeWidgetItem* hydraulics = SectionNamed(*pane, QStringLiteral("Hydraulics"));
    hydraulics->setExpanded(true);

    QMenu* menu = reader.findChild<QMenu*>();

    QVERIFY(menu != nullptr);
    QVERIFY(!menu->isVisible());

    RightClickAt(*pane, pane->visualItemRect(hydraulics->child(0)).center());

    QVERIFY2(menu->isVisible(), "the mark is what the two entries act on, so asking on the mark offers them");

    menu->hide();
    pane->setCurrentItem(hydraulics->child(0));

    const QPoint below(pane->viewport()->width() / 2, pane->viewport()->height() - 4);

    QVERIFY2(pane->itemAt(below) == nullptr, "the second point has to be empty for the check below to mean anything");

    RightClickAt(*pane, below);

    QVERIFY2(!menu->isVisible(),
             "the empty space under the last line carries no mark, and the menu of whichever line happened to be "
             "chosen is not the menu of that space");
}

void DocumentsPageTest::TheLensesTakeTheReadingCloserAndFurtherAway()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(4, {}));

    DocumentReader reader;
    reader.resize(900, 600);
    reader.show();

    reader.Read(manual, 0, DocumentKind::Document, {});

    auto* shown = reader.findChild<QPdfView*>();
    auto* closer = reader.findChild<QPushButton*>(QStringLiteral("ZoomIn"));
    auto* further = reader.findChild<QPushButton*>(QStringLiteral("ZoomOut"));

    QVERIFY(shown != nullptr);
    QVERIFY(closer != nullptr);
    QVERIFY(further != nullptr);

    const qreal fitted = shown->zoomFactor();

    closer->click();

    QCOMPARE(shown->zoomMode(), QPdfView::ZoomMode::Custom);
    QVERIFY2(shown->zoomFactor() > fitted, "the lens carrying the plus takes the reading closer");

    const qreal nearer = shown->zoomFactor();

    further->click();

    QVERIFY2(shown->zoomFactor() < nearer,
             "and the one carrying the minus takes it back, which is the only way in until someone turns the wheel "
             "switch on");
}

void DocumentsPageTest::TheSearchStepsForwardAndBackThroughWhatItFound()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);
    reader.show();

    reader.Read(manual, 0, DocumentKind::Document, {});

    auto* found = reader.findChild<QLabel*>(QStringLiteral("PanelPromise"));
    auto* back = reader.findChild<QPushButton*>(QStringLiteral("PreviousMatch"));
    auto* forth = reader.findChild<QPushButton*>(QStringLiteral("NextMatch"));

    QVERIFY(found != nullptr);
    QVERIFY(back != nullptr);
    QVERIFY(forth != nullptr);
    QVERIFY2(!back->isEnabled(), "with nothing searched for there is nothing to step through");

    reader.findChild<QLineEdit*>()->setText(QString::fromStdString(kOnEveryPage));

    QTRY_COMPARE(found->text(), QStringLiteral("1 of 24"));
    QVERIFY(forth->isEnabled());

    forth->click();

    QCOMPARE(found->text(), QStringLiteral("2 of 24"));

    back->click();

    QCOMPARE(found->text(), QStringLiteral("1 of 24"));

    back->click();

    QVERIFY2(found->text() == QStringLiteral("24 of 24"),
             "the only way back was the wrap, because until now the one gesture the search had was the Enter key and "
             "it only ever went forward");
}

void DocumentsPageTest::SteppingToAMatchFurtherDownTheSamePageScrollsToIt()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"tall.pdf", ATallPageWhereTheTermRepeats(6));

    DocumentReader reader;
    reader.resize(900, 600);
    reader.show();

    reader.Read(manual, 0, DocumentKind::Document, {});

    auto* found = reader.findChild<QLabel*>(QStringLiteral("PanelPromise"));
    auto* forth = reader.findChild<QPushButton*>(QStringLiteral("NextMatch"));
    auto* back = reader.findChild<QPushButton*>(QStringLiteral("PreviousMatch"));
    auto* pages = reader.findChild<QPdfView*>();

    QVERIFY(found != nullptr);
    QVERIFY(forth != nullptr);
    QVERIFY(back != nullptr);
    QVERIFY(pages != nullptr);

    reader.findChild<QLineEdit*>()->setText(QString::fromStdString(kOnEveryPage));

    QTRY_COMPARE(found->text(), QStringLiteral("1 of 6"));
    QVERIFY2(pages->verticalScrollBar()->maximum() > 0, "a page that fits needs no scrolling and proves nothing");

    const int atTheFirst = pages->verticalScrollBar()->value();

    QVERIFY2(atTheFirst < pages->verticalScrollBar()->maximum() / 4,
             "the first match is near the top of the page, so the reading starts there");

    forth->click();
    forth->click();
    forth->click();

    QCOMPARE(found->text(), QStringLiteral("4 of 6"));

    const int atTheFourth = pages->verticalScrollBar()->value();

    QVERIFY2(atTheFourth > atTheFirst,
             "every match here is on the same page, and a step that only recolours it leaves the reader looking at "
             "the one it left behind");

    forth->click();
    forth->click();

    QVERIFY2(pages->verticalScrollBar()->value() > atTheFourth
                 && pages->verticalScrollBar()->value() > pages->verticalScrollBar()->maximum() / 2,
             "the last matches are near the foot of the page, so the reading has to have travelled most of it");

    back->click();

    QVERIFY2(pages->verticalScrollBar()->value() < pages->verticalScrollBar()->maximum(),
             "and stepping back walks the page up again instead of parking at the end");
}

void DocumentsPageTest::TheMenuAnswersOnAMarkAndOnNothingElse()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);

    reader.Read(manual, 13, DocumentKind::Document, {{.page = 13, .name = {}}});

    QTreeWidget* pane = ThePaneOf(reader);
    QTreeWidgetItem* hydraulics = SectionNamed(*pane, QStringLiteral("Hydraulics"));

    pane->setCurrentItem(hydraulics);

    QVERIFY2(!pane->actions().front()->isEnabled(),
             "renaming the line of a section would be renaming the section, which is what the ADR refused when it "
             "chose a mark over a star");

    pane->setCurrentItem(hydraulics->child(0));

    QSignalSpy turned(&reader, &DocumentReader::TheMarkOfThePageWasTurned);

    QVERIFY(pane->actions().front()->isEnabled());

    const bool wentThroughTheQuestion = WhatTheMenuAsks(QMessageBox::Yes, *pane->actions().back());

    QVERIFY2(wentThroughTheQuestion,
             "removing is the one action here with no way back, and the app asks before every other one that "
             "destroys something");
    QCOMPARE(turned.count(), 1);
    QCOMPARE(turned.front().at(0).toInt(), 13);
    QVERIFY(!turned.front().at(1).toBool());
}

void DocumentsPageTest::TheMarkTheReaderTurnsIsKeptWithTheDocumentThatIsOpen()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));
    crj->setExpanded(true);

    const QRect name = index->visualItemRect(crj->child(0));
    ClickAt(*index, QPoint(name.center().x(), name.center().y()));

    auto* reader = page.findChild<DocumentReader*>();

    QVERIFY(reader != nullptr);

    emit reader->TheMarkOfThePageWasTurned(12, true);

    QCOMPARE(f.settings.stored.documents.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.documents.front().bookmarks.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.documents.front().bookmarks.front().page, 12);

    emit reader->TheBookmarkWasNamed(12, QStringLiteral("Where I stopped"));

    QCOMPARE(f.settings.stored.documents.front().bookmarks.front().name, std::string{"Where I stopped"});
}

void DocumentsPageTest::SayingNoToTheQuestionLeavesTheMarkWhereItIs()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    reader.resize(900, 600);
    reader.Read(manual, 13, DocumentKind::Document, {{.page = 13, .name = "Where I stopped"}});

    QTreeWidget* pane = ThePaneOf(reader);
    pane->setCurrentItem(SectionNamed(*pane, QStringLiteral("Hydraulics"))->child(0));

    const QSignalSpy turned(&reader, &DocumentReader::TheMarkOfThePageWasTurned);

    QVERIFY(WhatTheMenuAsks(QMessageBox::No, *pane->actions().back()));
    QVERIFY2(turned.isEmpty(), "a question nobody can answer no to is not a question");
}

void DocumentsPageTest::AReaderOnItsWayOutStopsAnsweringThePagesItIsTakingWithIt()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    auto reader = std::make_unique<DocumentReader>();
    reader->resize(900, 600);
    reader->Read(manual, 13, DocumentKind::Document, {{.page = 13, .name = {}}});

    QVERIFY(TheMarkButtonOf(*reader)->isChecked());

    const QSignalSpy turned(reader.get(), &DocumentReader::ThePageChanged);
    const qsizetype beforeItWent = turned.count();

    reader.reset();

    QVERIFY2(turned.count() == beforeItWent,
             "the view walks the pages back on its way out, and answering it there both writes a page nobody read "
             "and lands on the parentless findChildren of QAbstractButton::setChecked");
}

QTEST_MAIN(DocumentsPageTest)

#include "tst_documents_page.moc"
