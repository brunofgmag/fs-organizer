#include <QtTest/QtTest>

#include <QtCore/QTemporaryDir>
#include <QtGui/QAction>
#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfPageNavigator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "tests/support/APdf.h"
#include "tests/support/PathPrinting.h"
#include "view/documents/DocumentReader.h"
#include "view/documents/SelectablePages.h"

namespace
{
    class ReaderSelectionTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageSitsWhereTheViewItselfPutsIt();
        static void DraggingOverALineMarksItAndCopyTakesIt();
        static void TheSameDragMovesThePageWhenTheSwitchSaysSo();
        static void DoubleClickTakesTheWordAndTripleClickTheLine();
        static void SelectAllTakesThePageTheReadingIsOn();
        static void TheMarkCarriesOnPastThePageBreak();
        static void TheMenuOverThePageOffersTheCopyOnlyWhenThereIsSomething();
        static void APointerThatDoesNotWanderLeavesTheReadingAlone();
    };

    [[nodiscard]] std::filesystem::path
    WrittenInto(const QTemporaryDir& folder, const std::wstring& named, const std::string& bytes)
    {
        const std::filesystem::path file = std::filesystem::path(folder.path().toStdWString()) / named;

        std::ofstream written(file, std::ios::binary);
        written.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

        return file;
    }

    [[nodiscard]] SelectablePages* TheReadingOf(const DocumentReader& reader)
    {
        return reader.findChild<SelectablePages*>();
    }

    [[nodiscard]] QPdfDocument* TheDocumentOf(const DocumentReader& reader)
    {
        return reader.findChild<QPdfDocument*>();
    }

    [[nodiscard]] QPoint OverTheLine(const SelectablePages& pages,
                                     const QPdfDocument& document,
                                     const int page,
                                     const int line,
                                     const qreal across)
    {
        const QPdfSelection whole = const_cast<QPdfDocument&>(document).getAllText(page);
        const QRectF box = whole.bounds().at(line).boundingRect();
        const WhereAPageSits sits = pages.WhereThePageSits(page);

        const QPointF onThePage(box.left() + box.width() * across, box.center().y());
        const QPointF inTheDocument = onThePage * sits.scale + QPointF(sits.box.topLeft());

        return (inTheDocument - QPointF(pages.horizontalScrollBar()->value(), pages.verticalScrollBar()->value()))
            .toPoint();
    }

    void PointerAt(SelectablePages& pages, const QEvent::Type what, const QPoint& where)
    {
        QMouseEvent acted(what, QPointF(where), pages.viewport()->mapToGlobal(QPointF(where)), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);

        QCoreApplication::sendEvent(pages.viewport(), &acted);
    }

    void DragBetween(SelectablePages& pages, const QPoint& from, const QPoint& to)
    {
        PointerAt(pages, QEvent::MouseButtonPress, from);
        PointerAt(pages, QEvent::MouseMove, QPoint((from.x() + to.x()) / 2, (from.y() + to.y()) / 2));
        PointerAt(pages, QEvent::MouseMove, to);
        PointerAt(pages, QEvent::MouseButtonRelease, to);
    }

    [[nodiscard]] SelectablePages*
    AManualOpenedIn(DocumentReader& reader, const std::filesystem::path& manual, const ReadingGestures gestures)
    {
        reader.resize(900, 600);
        reader.show();
        reader.SayTheGesturesOf(DocumentKind::Document, gestures);
        reader.Read(manual, 0, DocumentKind::Document, {});

        return TheReadingOf(reader);
    }
}

void ReaderSelectionTest::ThePageSitsWhereTheViewItselfPutsIt()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"widening.pdf", AManualWhosePagesWidenHalfway(12));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    auto* fitWidth = reader.findChild<QPushButton*>(QStringLiteral("FitTheWidth"));

    QVERIFY(fitWidth != nullptr);

    fitWidth->click();

    QVERIFY2(pages->zoomMode() == QPdfView::ZoomMode::FitToWidth,
             "the mixed widths only pull the two routes apart when each page is scaled to the width");

    int compared = 0;

    for (int page = 0; page < TheDocumentOf(reader)->pageCount(); ++page)
    {
        pages->pageNavigator()->jump(page, {});

        const int weSayItSits = pages->WhereThePageSits(page).box.y();

        if (weSayItSits > pages->verticalScrollBar()->maximum())
        {
            continue;
        }

        QCOMPARE(weSayItSits, pages->verticalScrollBar()->value());

        ++compared;
    }

    QVERIFY2(compared > 6,
             "the check is worth nothing if the scrollbar clamped on every page it looked at, so it says how "
             "many it really compared");
}

void ReaderSelectionTest::DraggingOverALineMarksItAndCopyTakesIt()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(3));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    const QPdfDocument* document = TheDocumentOf(reader);

    QVERIFY(document != nullptr);

    QGuiApplication::clipboard()->clear();

    DragBetween(*pages, OverTheLine(*pages, *document, 0, 1, 0.02), OverTheLine(*pages, *document, 0, 1, 0.98));

    QVERIFY2(pages->CarriesASelection(), "dragging over a line of a manual marks it");

    pages->CopyWhatIsSelected();

    const std::vector<std::string> drawn = TheLinesDrawnOn(0);

    QVERIFY2(QGuiApplication::clipboard()->text().contains(QString::fromStdString(drawn.at(1))),
             qPrintable(QStringLiteral("the clipboard carries the line that was dragged over, and it carries [%1]")
                            .arg(QGuiApplication::clipboard()->text())));
}

void ReaderSelectionTest::TheSameDragMovesThePageWhenTheSwitchSaysSo()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(24));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = true});

    QVERIFY(pages != nullptr);
    QVERIFY2(pages->verticalScrollBar()->maximum() > 0, "the manual has to be taller than the reader to be moved");

    const QPdfDocument* document = TheDocumentOf(reader);
    const QPoint from = OverTheLine(*pages, *document, 0, 1, 0.02);
    const QPoint to = OverTheLine(*pages, *document, 0, 1, 0.98);
    const int born = pages->verticalScrollBar()->value();

    DragBetween(*pages, from, QPoint(to.x(), to.y() - 40));

    QVERIFY2(!pages->CarriesASelection(),
             "with the switch on, the same drag is the one that moves the page, so nothing is marked");
    QVERIFY2(pages->verticalScrollBar()->value() != born, "and the page did move");

    auto* drag = reader.findChild<QPushButton*>(QStringLiteral("DragMovesThePage"));

    QVERIFY(drag != nullptr);

    drag->click();

    DragBetween(*pages, from, to);

    QVERIFY2(pages->CarriesASelection(), "and turning it off hands the same button to the selection");
}

void ReaderSelectionTest::DoubleClickTakesTheWordAndTripleClickTheLine()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(3));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    const QPdfDocument* document = TheDocumentOf(reader);
    const QPoint middleOfTheLine = OverTheLine(*pages, *document, 0, 1, 0.5);

    PointerAt(*pages, QEvent::MouseButtonPress, middleOfTheLine);
    PointerAt(*pages, QEvent::MouseButtonRelease, middleOfTheLine);
    PointerAt(*pages, QEvent::MouseButtonDblClick, middleOfTheLine);

    const QString word = pages->WhatIsSelected();

    QVERIFY2(!word.isEmpty(), "a double click marks something");
    QVERIFY2(!word.contains(QChar(u' ')),
             qPrintable(QStringLiteral("and what it marks is one word, not [%1]").arg(word)));

    PointerAt(*pages, QEvent::MouseButtonRelease, middleOfTheLine);
    PointerAt(*pages, QEvent::MouseButtonPress, middleOfTheLine);

    const QString line = pages->WhatIsSelected();
    const std::vector<std::string> drawn = TheLinesDrawnOn(0);

    QCOMPARE(line, QString::fromStdString(drawn.at(1)));
}

void ReaderSelectionTest::SelectAllTakesThePageTheReadingIsOn()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(4));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    pages->pageNavigator()->jump(2, {});
    pages->SelectTheWholePage(pages->pageNavigator()->currentPage());

    const QString marked = pages->WhatIsSelected();

    for (const std::string& line : TheLinesDrawnOn(2))
    {
        QVERIFY2(marked.contains(QString::fromStdString(line)),
                 qPrintable(QStringLiteral("the page the reading is on comes whole, and [%1] is missing from [%2]")
                                .arg(QString::fromStdString(line), marked)));
    }

    QVERIFY2(!marked.contains(QString::fromStdString(TheLinesDrawnOn(1).at(0))),
             "and it stops at that page, because select all over three hundred pages paints what nobody reads");
}

void ReaderSelectionTest::TheMarkCarriesOnPastThePageBreak()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(4));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    const QPdfDocument* document = TheDocumentOf(reader);

    DragBetween(*pages, OverTheLine(*pages, *document, 0, 2, 0.5), OverTheLine(*pages, *document, 1, 0, 1.5));

    const QString marked = pages->WhatIsSelected();

    QVERIFY2(marked.contains(QString::fromStdString(TheLinesDrawnOn(1).at(0))),
             qPrintable(QStringLiteral("the drag that passes the page break carries on into the next page, and it "
                                       "carried [%1]")
                            .arg(marked)));
    QVERIFY2(marked.startsWith(QStringLiteral("o foxtrot")),
             qPrintable(QStringLiteral("and it starts where the pointer went down and not at the top of that page, "
                                       "which is what tells a stitch from a pair of whole pages, and it carried [%1]")
                            .arg(marked)));
    QVERIFY2(!marked.contains(QString::fromStdString(TheLinesDrawnOn(1).at(1))),
             "and it stops where the pointer came up");
}

void ReaderSelectionTest::TheMenuOverThePageOffersTheCopyOnlyWhenThereIsSomething()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(3));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    const QPdfDocument* document = TheDocumentOf(reader);
    const QPoint over = OverTheLine(*pages, *document, 0, 1, 0.5);

    const auto askForTheMenu = [&pages, &over]
    {
        QContextMenuEvent asked(QContextMenuEvent::Mouse, over, pages->viewport()->mapToGlobal(over));

        QCoreApplication::sendEvent(pages->viewport(), &asked);
    };

    askForTheMenu();

    const QList<QMenu*> menus = reader.findChildren<QMenu*>();

    QVERIFY(!menus.isEmpty());
    QVERIFY2(std::none_of(menus.cbegin(), menus.cend(),
                          [](const QMenu* menu)
                          {
                              return menu->isVisible();
                          }),
             "with nothing marked there is nothing to copy, so no menu appears at all");

    DragBetween(*pages, OverTheLine(*pages, *document, 0, 1, 0.02), OverTheLine(*pages, *document, 0, 1, 0.98));

    askForTheMenu();

    const auto shown = std::find_if(menus.cbegin(), menus.cend(),
                                    [](const QMenu* menu)
                                    {
                                        return menu->isVisible();
                                    });

    QVERIFY2(shown != menus.cend(), "and once something is marked the menu offers the copy");

    (*shown)->close();
}

void ReaderSelectionTest::APointerThatDoesNotWanderLeavesTheReadingAlone()
{
    const QTemporaryDir folder;
    const std::filesystem::path manual = WrittenInto(folder, L"known.pdf", AManualWhoseLinesAreKnown(3));

    DocumentReader reader;
    SelectablePages* pages = AManualOpenedIn(reader, manual, {.wheelZooms = false, .dragMovesThePage = false});

    QVERIFY(pages != nullptr);

    const QPdfDocument* document = TheDocumentOf(reader);
    const QPoint over = OverTheLine(*pages, *document, 0, 1, 0.5);

    DragBetween(*pages, OverTheLine(*pages, *document, 0, 1, 0.02), OverTheLine(*pages, *document, 0, 1, 0.98));

    QVERIFY(pages->CarriesASelection());

    PointerAt(*pages, QEvent::MouseButtonPress, over);
    PointerAt(*pages, QEvent::MouseButtonRelease, over);

    QVERIFY2(!pages->CarriesASelection(),
             "a click that goes nowhere clears the mark, which is also what lets the click reach a link");
}

QTEST_MAIN(ReaderSelectionTest)

#include "tst_reader_selection.moc"
