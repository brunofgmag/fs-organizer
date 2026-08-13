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
        static void TheProgressAppearsWhenTheReadingStartsAndGoesWhenItEnds();
        static void TheWheelZoomsAChartAndScrollsADocument();
        static void TheWheelStopsZoomingWhenTheReaderIsToldItShouldNot();
        static void DraggingMovesADocumentAndNotOnlyAChart();
        static void APointerThatDoesNotWanderLeavesThePageWhereItWas();
        static void DraggingMovesNothingWhenTheReaderIsToldItShouldNot();
        static void ThePaneMarksTheSectionThatHoldsThePageTheReadingIsOn();
        static void AMarkHangsUnderItsSectionOnABranchBornOpenAndLightsTheButton();
        static void TheButtonSaysWhichPageIsBeingMarkedAndWhichWayItWent();
        static void ADerivedMarkIsNamedByItsPageAndANameTheUserGaveWins();
        static void ADocumentWithoutAnOutlineShowsThePaneOnceItCarriesAMark();
        static void TheMenuAnswersOnAMarkAndOnNothingElse();
        static void TheSearchStepsForwardAndBackThroughWhatItFound();
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

void DocumentsPageTest::TheWheelZoomsAChartAndScrollsADocument()
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
    roll();

    QCOMPARE(pages->zoomMode(), QPdfView::ZoomMode::FitToWidth);

    reader.Read(chart, 0, DocumentKind::Chart, {});
    const qreal before = pages->zoomFactor();
    roll();

    QCOMPARE(pages->zoomMode(), QPdfView::ZoomMode::Custom);
    QVERIFY2(pages->zoomFactor() > before,
             "a chart is read by getting closer to it, and the wheel is the gesture that does that");
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
    reader.SayTheWheelZooms(false);

    QPdfView* pages = reader.findChild<QPdfView*>();

    QVERIFY(pages != nullptr);

    reader.Read(chart, 0, DocumentKind::Chart, {});
    Roll(*pages);

    QCOMPARE(pages->zoomMode(), QPdfView::ZoomMode::FitToWidth);

    reader.SayTheWheelZooms(true);
    Roll(*pages);

    QCOMPARE(pages->zoomMode(), QPdfView::ZoomMode::Custom);
}

void DocumentsPageTest::DraggingMovesADocumentAndNotOnlyAChart()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"manual.pdf", AManualOf(24, kChapters));

    DocumentReader reader;
    QPdfView* pages = AManualOpenedIn(reader, manual);

    QVERIFY2(pages != nullptr, "the manual has to be taller than the reader for a drag to have anywhere to go");

    const int before = pages->verticalScrollBar()->value();

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 200));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 200));

    QVERIFY2(pages->verticalScrollBar()->value() > before,
             "dragging the page upwards walks the reading forward, and it is a manual, not a chart");
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

    reader.SayTheDragMovesThePage(false);

    const int before = pages->verticalScrollBar()->value();

    PointerAt(*pages, QEvent::MouseButtonPress, QPointF(100, 300));
    PointerAt(*pages, QEvent::MouseMove, QPointF(100, 200));
    PointerAt(*pages, QEvent::MouseButtonRelease, QPointF(100, 200));

    QCOMPARE(pages->verticalScrollBar()->value(), before);
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
             "and the one carrying the minus takes it back, which on a document no gesture could do, because the "
             "wheel only zooms a chart");
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
