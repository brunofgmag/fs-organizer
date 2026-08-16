#include "view/documents/SelectablePages.h"

#include <QtCore/QLineF>
#include <QtCore/QTextBoundaryFinder>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfPageNavigator>
#include <QtWidgets/QScrollBar>

#include <algorithm>
#include <utility>

#include "view/theme/ModernistTones.h"

namespace
{
    constexpr qreal kPointsPerInch = 72.0;
    constexpr qreal kTooNarrowToAimInside = 2.0;
    constexpr char16_t kFirstLetterAPageCanShow = u' ';
    constexpr int kUnmappedHundredthsAPageMayCarry = 25;
    constexpr int kEveryHundredth = 100;

    [[nodiscard]] qreal AimedInside(const qreal wanted, const qreal from, const qreal to)
    {
        if (to - from <= kTooNarrowToAimInside)
        {
            return (from + to) / 2;
        }

        return std::clamp(wanted, from + 1, to - 1);
    }

    [[nodiscard]] QPointF ThePointOnTextNearest(const QPdfSelection& whole, const QPointF& onThePage)
    {
        QPointF closest = onThePage;
        qreal howFar = -1;

        for (const QPolygonF& shape : whole.bounds())
        {
            const QRectF box = shape.boundingRect();
            const QPointF inside(AimedInside(onThePage.x(), box.left(), box.right()),
                                 AimedInside(onThePage.y(), box.top(), box.bottom()));
            const qreal distance = QLineF(onThePage, inside).length();

            if (howFar < 0 || distance < howFar)
            {
                howFar = distance;
                closest = inside;
            }
        }

        return closest;
    }

    [[nodiscard]] bool ItBreaksTheLine(const QChar letter)
    {
        return letter == QChar(u'\r') || letter == QChar(u'\n');
    }

    [[nodiscard]] bool ItSeparatesLetters(const QChar letter)
    {
        return letter == QChar(u' ') || letter == QChar(u'\t') || ItBreaksTheLine(letter);
    }

    [[nodiscard]] bool TheEngineCouldNotMapThePage(const QString& text)
    {
        int unmapped = 0;
        int counted = 0;

        for (const QChar letter : text)
        {
            if (ItSeparatesLetters(letter))
            {
                continue;
            }

            ++counted;

            if (letter.unicode() < kFirstLetterAPageCanShow)
            {
                ++unmapped;
            }
        }

        return counted > 0 && unmapped * kEveryHundredth > counted * kUnmappedHundredthsAPageMayCarry;
    }

    [[nodiscard]] APieceOfTheSelection AsAPiece(const int page, const QPdfSelection& marked)
    {
        return {.page = page, .text = marked.text(), .shapes = marked.bounds()};
    }

    struct APageAsDrawn
    {
        QSize size{};
        qreal scale = 1.0;
    };

    [[nodiscard]] APageAsDrawn HowThePageIsDrawn(const QSizeF& inPoints,
                                                 const qreal resolution,
                                                 const QPdfView::ZoomMode mode,
                                                 const qreal zoom,
                                                 const QSize& room)
    {
        if (mode == QPdfView::ZoomMode::Custom)
        {
            return {.size = QSizeF(inPoints * resolution * zoom).toSize(), .scale = zoom};
        }

        const QSize unscaled = QSizeF(inPoints * resolution).toSize();

        if (unscaled.isEmpty())
        {
            return {};
        }

        if (mode == QPdfView::ZoomMode::FitToWidth)
        {
            const qreal scale = qreal(room.width()) / qreal(unscaled.width());

            return {.size = QSize(qRound(unscaled.width() * scale), qRound(unscaled.height() * scale)), .scale = scale};
        }

        const QSize fitted = unscaled.scaled(room, Qt::KeepAspectRatio);

        return {.size = fitted, .scale = qreal(fitted.width()) / qreal(unscaled.width())};
    }
}

SelectablePages::SelectablePages(QWidget* parent) : QPdfView(parent)
{
}

void SelectablePages::FollowTheDocument(QPdfDocument* document)
{
    document_ = document;
    setDocument(document);

    if (document_ != nullptr)
    {
        connect(document_, &QPdfDocument::statusChanged, this,
                [this]
                {
                    ForgetWhatWasReadFromTheDocument();
                    ForgetTheSelection();
                });
    }
}

void SelectablePages::ForgetWhatWasReadFromTheDocument()
{
    pointSizes_.clear();
    wholePages_.clear();
}

QSizeF SelectablePages::ThePointSizeOf(const int page) const
{
    if (document_ == nullptr || page < 0 || page >= document_->pageCount())
    {
        return {};
    }

    if (pointSizes_.size() != static_cast<std::size_t>(document_->pageCount()))
    {
        pointSizes_.assign(static_cast<std::size_t>(document_->pageCount()), QSizeF());
    }

    QSizeF& remembered = pointSizes_[static_cast<std::size_t>(page)];

    if (remembered.isEmpty())
    {
        remembered = document_->pagePointSize(page);
    }

    return remembered;
}

WhereAPageSits SelectablePages::WhereThePageSits(const int wanted) const
{
    if (document_ == nullptr || wanted < 0 || wanted >= document_->pageCount())
    {
        return {};
    }

    const qreal resolution = QGuiApplication::primaryScreen()->logicalDotsPerInch() / kPointsPerInch;
    const QMargins margins = documentMargins();

    const bool oneAtATime = pageMode() == PageMode::SinglePage;
    const int from = oneAtATime ? pageNavigator()->currentPage() : 0;
    const int until = oneAtATime ? pageNavigator()->currentPage() + 1 : document_->pageCount();

    int widest = 0;
    std::vector<QSize> sizes;
    std::vector<qreal> scales;
    sizes.reserve(static_cast<std::size_t>(until - from));
    scales.reserve(static_cast<std::size_t>(until - from));

    const QSize room(viewport()->width() - margins.left() - margins.right(), viewport()->height() - pageSpacing());

    for (int page = from; page < until; ++page)
    {
        const APageAsDrawn drawn = HowThePageIsDrawn(ThePointSizeOf(page), resolution, zoomMode(), zoomFactor(), room);

        widest = std::max(widest, drawn.size.width());
        sizes.push_back(drawn.size);
        scales.push_back(drawn.scale);
    }

    const int across = widest + margins.left() + margins.right();

    int y = margins.top();
    WhereAPageSits found;

    for (int page = from; page < until; ++page)
    {
        const QSize size = sizes[static_cast<std::size_t>(page - from)];
        const int x = (std::max(across, viewport()->width()) - size.width()) / 2;

        if (page == wanted)
        {
            found.box = QRect(QPoint(x, y), size);
            found.scale = resolution * scales[static_cast<std::size_t>(page - from)];
        }

        y += size.height() + pageSpacing();
    }

    return found;
}

int SelectablePages::PageUnder(const QPoint& where) const
{
    if (document_ == nullptr)
    {
        return -1;
    }

    const int y = where.y() + verticalScrollBar()->value();
    int nearest = -1;
    int howFar = -1;

    for (int page = 0; page < document_->pageCount(); ++page)
    {
        const QRect box = WhereThePageSits(page).box;

        if (box.isEmpty())
        {
            continue;
        }

        if (y >= box.top() && y <= box.bottom())
        {
            return page;
        }

        const int distance = y < box.top() ? box.top() - y : y - box.bottom();

        if (howFar < 0 || distance < howFar)
        {
            howFar = distance;
            nearest = page;
        }
    }

    return nearest;
}

QPointF SelectablePages::WhereOnThePage(const int page, const QPoint& where) const
{
    const WhereAPageSits sits = WhereThePageSits(page);

    if (sits.box.isEmpty() || qFuzzyIsNull(sits.scale))
    {
        return {};
    }

    const QPointF inTheDocument(where.x() + horizontalScrollBar()->value(), where.y() + verticalScrollBar()->value());

    return (inTheDocument - QPointF(sits.box.topLeft())) / sits.scale;
}

const QPdfSelection& SelectablePages::TheWholeOf(const int page) const
{
    const auto remembered = wholePages_.constFind(page);

    if (remembered != wholePages_.cend())
    {
        return remembered.value();
    }

    const QPdfSelection whole = document_->getAllText(page);

    if (TheEngineCouldNotMapThePage(whole.text()))
    {
        return *wholePages_.insert(page, document_->getSelectionAtIndex(page, 0, 0));
    }

    return *wholePages_.insert(page, whole);
}

APlaceInTheText SelectablePages::ThePlaceUnder(const QPoint& where) const
{
    if (document_ == nullptr)
    {
        return {};
    }

    const int page = PageUnder(where);

    if (page < 0)
    {
        return {};
    }

    const QPdfSelection& whole = TheWholeOf(page);

    if (!whole.isValid() || whole.bounds().isEmpty())
    {
        return {};
    }

    const QPointF aimed = ThePointOnTextNearest(whole, WhereOnThePage(page, where));
    const QPointF last = whole.bounds().last().boundingRect().center();
    const QPdfSelection towardsTheEnd = document_->getSelection(page, aimed, last);

    if (towardsTheEnd.isValid())
    {
        return {.page = page, .index = towardsTheEnd.startIndex()};
    }

    const QPointF first = whole.bounds().first().boundingRect().center();
    const QPdfSelection towardsTheStart = document_->getSelection(page, first, aimed);

    if (towardsTheStart.isValid())
    {
        return {.page = page, .index = towardsTheStart.endIndex() - 1};
    }

    return {.page = page, .index = whole.text().isEmpty() ? -1 : 0};
}

void SelectablePages::Mark(std::vector<APieceOfTheSelection> pieces)
{
    pieces_ = std::move(pieces);

    viewport()->update();
}

void SelectablePages::MarkFrom(const APlaceInTheText& from, const APlaceInTheText& to)
{
    if (document_ == nullptr || from.index < 0 || to.index < 0)
    {
        return;
    }

    APlaceInTheText first = from;
    APlaceInTheText last = to;

    const bool backwards = first.page > last.page || (first.page == last.page && first.index > last.index);

    if (backwards)
    {
        std::swap(first, last);
    }

    std::vector<APieceOfTheSelection> pieces;

    if (first.page == last.page)
    {
        pieces.push_back(AsAPiece(
            first.page, document_->getSelectionAtIndex(first.page, first.index, last.index - first.index + 1)));

        Mark(std::move(pieces));

        return;
    }

    const int leftOnTheFirst = static_cast<int>(TheWholeOf(first.page).text().size()) - first.index;

    pieces.push_back(AsAPiece(first.page, document_->getSelectionAtIndex(first.page, first.index, leftOnTheFirst)));

    for (int page = first.page + 1; page < last.page; ++page)
    {
        pieces.push_back(AsAPiece(page, TheWholeOf(page)));
    }

    pieces.push_back(AsAPiece(last.page, document_->getSelectionAtIndex(last.page, 0, last.index + 1)));

    Mark(std::move(pieces));
}

void SelectablePages::StartSelectingAt(const QPoint& where)
{
    anchor_ = ThePlaceUnder(where);

    ForgetTheSelection();
}

void SelectablePages::ExtendTheSelectionTo(const QPoint& where)
{
    if (anchor_.index < 0)
    {
        anchor_ = ThePlaceUnder(where);

        return;
    }

    const APlaceInTheText now = ThePlaceUnder(where);

    if (now.index < 0)
    {
        return;
    }

    MarkFrom(anchor_, now);
}

void SelectablePages::SelectTheWordAt(const QPoint& where)
{
    const APlaceInTheText place = ThePlaceUnder(where);

    if (place.index < 0)
    {
        return;
    }

    const QString text = TheWholeOf(place.page).text();
    QTextBoundaryFinder words(QTextBoundaryFinder::Word, text);
    words.setPosition(place.index);

    const int from = words.isAtBoundary() ? place.index : words.toPreviousBoundary();
    const int to = words.toNextBoundary();

    if (from < 0 || to <= from)
    {
        return;
    }

    anchor_ = {.page = place.page, .index = from};

    MarkFrom(anchor_, {.page = place.page, .index = to - 1});
}

void SelectablePages::SelectTheLineAt(const QPoint& where)
{
    const APlaceInTheText place = ThePlaceUnder(where);

    if (place.index < 0)
    {
        return;
    }

    const QString text = TheWholeOf(place.page).text();

    int from = place.index;
    while (from > 0 && !ItBreaksTheLine(text.at(from - 1)))
    {
        --from;
    }

    int to = place.index;
    while (to < text.size() && !ItBreaksTheLine(text.at(to)))
    {
        ++to;
    }

    if (to <= from)
    {
        return;
    }

    anchor_ = {.page = place.page, .index = from};

    MarkFrom(anchor_, {.page = place.page, .index = to - 1});
}

void SelectablePages::SelectTheWholePage(const int page)
{
    if (document_ == nullptr || page < 0 || page >= document_->pageCount())
    {
        return;
    }

    const QPdfSelection& whole = TheWholeOf(page);

    if (!whole.isValid())
    {
        return;
    }

    anchor_ = {.page = page, .index = 0};

    Mark({AsAPiece(page, whole)});
}

void SelectablePages::ForgetTheSelection()
{
    if (pieces_.empty())
    {
        return;
    }

    Mark({});
}

bool SelectablePages::CarriesASelection() const
{
    return std::any_of(pieces_.cbegin(), pieces_.cend(),
                       [](const APieceOfTheSelection& piece)
                       {
                           return !piece.text.isEmpty();
                       });
}

QString SelectablePages::WhatIsSelected() const
{
    QString gathered;

    for (const APieceOfTheSelection& piece : pieces_)
    {
        if (!gathered.isEmpty() && !ItBreaksTheLine(gathered.back()))
        {
            gathered += QStringLiteral("\r\n");
        }

        gathered += piece.text;
    }

    return gathered;
}

void SelectablePages::CopyWhatIsSelected() const
{
    if (!CarriesASelection())
    {
        return;
    }

    QGuiApplication::clipboard()->setText(WhatIsSelected());
}

void SelectablePages::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy))
    {
        CopyWhatIsSelected();
        event->accept();

        return;
    }

    if (event->matches(QKeySequence::SelectAll))
    {
        SelectTheWholePage(pageNavigator()->currentPage());
        event->accept();

        return;
    }

    QPdfView::keyPressEvent(event);
}

void SelectablePages::paintEvent(QPaintEvent* event)
{
    QPdfView::paintEvent(event);

    if (pieces_.empty())
    {
        return;
    }

    QPainter painter(viewport());

    painter.setPen(Qt::NoPen);
    painter.setBrush(TheMarkThatSitsOnPaper());

    const QPoint scrolled(horizontalScrollBar()->value(), verticalScrollBar()->value());

    for (const APieceOfTheSelection& piece : pieces_)
    {
        const WhereAPageSits sits = WhereThePageSits(piece.page);

        if (sits.box.isEmpty())
        {
            continue;
        }

        const QPointF origin = QPointF(sits.box.topLeft() - scrolled);

        for (const QPolygonF& shape : piece.shapes)
        {
            const QRectF box = shape.boundingRect();

            painter.drawRect(QRectF(origin.x() + box.x() * sits.scale, origin.y() + box.y() * sits.scale,
                                    box.width() * sits.scale, box.height() * sits.scale));
        }
    }
}
