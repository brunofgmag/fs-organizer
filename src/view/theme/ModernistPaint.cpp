#include "view/theme/ModernistPaint.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtGui/QPolygonF>
#include <QtGui/QTransform>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QStyle>

#include "view/theme/ModernistTones.h"

namespace
{
    constexpr int kTagPaddingX = 10;
    constexpr int kTagPaddingY = 5;
    constexpr int kSmallLabelPixelSize = 10;
    constexpr qreal kSmallLabelSpacing = 1.0;
    constexpr qreal kSpineTextScale = 0.85;

    class HeaderDresser final : public QObject
    {
    public:
        HeaderDresser(QHeaderView* header, const QFont& label) : QObject(header), header_(header), label_(label)
        {
            header_->installEventFilter(this);
            Dress();
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange
                || event->type() == QEvent::Polish)
            {
                Dress();
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        void Dress() const
        {
            if (header_->font() == label_)
            {
                return;
            }

            header_->setFont(label_);

            if (QWidget* viewport = header_->viewport(); viewport != nullptr)
            {
                viewport->setFont(label_);
            }

            header_->updateGeometry();
            header_->update();
        }

        QHeaderView* header_;
        QFont label_;
    };

    QFont ScaledFont(const QFont& base, qreal factor);

    QFont SmallLabelFont(const QFont& base)
    {
        QFont label = base;
        label.setPixelSize(kSmallLabelPixelSize);
        label.setWeight(QFont::ExtraBold);
        label.setCapitalization(QFont::AllUppercase);
        label.setLetterSpacing(QFont::AbsoluteSpacing, kSmallLabelSpacing);

        return label;
    }

    QFont ScaledFont(const QFont& base, const qreal factor)
    {
        QFont font = base;

        if (base.pointSizeF() > 0.0)
        {
            font.setPointSizeF(base.pointSizeF() * factor);
        }
        else if (base.pixelSize() > 0)
        {
            font.setPixelSize(std::max(1, qRound(base.pixelSize() * factor)));
        }

        return font;
    }

    struct TagPaint
    {
        QColor ground;
        QColor ink;
        QColor rule;
    };

    TagPaint PaintOf(const TagTone tone)
    {
        const ModernistTones tones = TonesOf(CurrentColorScheme());

        switch (tone)
        {
        case TagTone::Filled: return {.ground = tones.accent, .ink = tones.onAccent, .rule = tones.accent};
        case TagTone::Outlined: return {.ground = Qt::transparent, .ink = tones.accentInk, .rule = tones.accent};
        case TagTone::Muted: return {.ground = tones.raised, .ink = tones.secondary, .rule = tones.raised};
        case TagTone::Line: break;
        }

        return {.ground = Qt::transparent, .ink = tones.secondary, .rule = tones.edge};
    }

    constexpr qreal kArrowLength = 10.0;
    constexpr qreal kArrowSpan = 8.0;
    constexpr qreal kArrowDepth = 4.0;
    constexpr qreal kArrowStroke = 1.5;
    constexpr qreal kArrowDepthPerReach = kArrowDepth / (kArrowSpan / 2.0);

    struct ArrowOnTheGrid
    {
        qreal length{};
        qreal half{};
        qreal reach{};
        qreal depth{};
        qreal band{};
        bool shafted{};
    };

    [[nodiscard]] bool ItLies(const ArrowHeading heading)
    {
        return heading != ArrowHeading::Up && heading != ArrowHeading::Down;
    }

    [[nodiscard]] bool TheTipComesFirst(const ArrowHeading heading)
    {
        return heading != ArrowHeading::Right && heading != ArrowHeading::Down;
    }

    [[nodiscard]] qreal BandOf(const qreal stroke)
    {
        return stroke * std::hypot(kArrowDepthPerReach, 1.0);
    }

    [[nodiscard]] qreal HeadExtent()
    {
        return kArrowDepth + BandOf(kArrowStroke);
    }

    [[nodiscard]] ArrowOnTheGrid FittedToTheGrid(const ArrowHeading heading, const qreal scale)
    {
        const int stroke = std::max(1, qRound(kArrowStroke * scale));
        const int span = stroke + 2 * std::max(1, qRound((kArrowSpan * scale - stroke) / 2.0));
        const qreal reach = span / 2.0;
        const qreal depth = reach * kArrowDepthPerReach;
        const qreal band = BandOf(stroke);
        const qreal head = std::ceil(depth + band);
        const qreal single = std::max(std::round(kArrowLength * scale), head);
        const qreal length = heading == ArrowHeading::LeftAndRight ? single + head : single;

        return {
            .length = length,
            .half = stroke / 2.0,
            .reach = reach,
            .depth = depth,
            .band = band,
            .shafted = single > head,
        };
    }

    [[nodiscard]] QPolygonF HeadFromTheTip(const ArrowOnTheGrid& arrow)
    {
        const qreal knee = arrow.band + arrow.half * arrow.depth / arrow.reach;
        const QPointF joins = arrow.shafted ? QPointF(knee, -arrow.half) : QPointF(arrow.band, 0.0);

        return {{0.0, 0.0}, {arrow.depth, -arrow.reach}, {arrow.depth + arrow.band, -arrow.reach}, joins};
    }

    [[nodiscard]] QPolygonF OutlineOf(const ArrowOnTheGrid& arrow, const ArrowHeading heading)
    {
        const QPolygonF head = HeadFromTheTip(arrow);
        const bool bothWays = heading == ArrowHeading::LeftAndRight;

        QPolygonF outline = head;

        if (bothWays)
        {
            for (const QPointF& corner : std::views::reverse(head))
            {
                outline << QPointF(arrow.length - corner.x(), corner.y());
            }

            for (const QPointF& corner : head)
            {
                if (corner.y() != 0.0)
                {
                    outline << QPointF(arrow.length - corner.x(), -corner.y());
                }
            }
        }

        if (!bothWays && arrow.shafted)
        {
            outline << QPointF(arrow.length, -arrow.half) << QPointF(arrow.length, arrow.half);
        }

        for (const QPointF& corner : std::views::reverse(head))
        {
            if (corner.y() != 0.0)
            {
                outline << QPointF(corner.x(), -corner.y());
            }
        }

        return outline;
    }
}

qreal OneDevicePixel(const QPainter& painter)
{
    const QPaintDevice* surface = painter.device();
    const qreal ratio = surface != nullptr ? surface->devicePixelRatioF() : 1.0;

    return ratio > 0.0 ? 1.0 / ratio : 1.0;
}

QRectF OutlineInside(const QPainter& painter, const QRectF& box)
{
    const qreal half = OneDevicePixel(painter) / 2.0;

    return box.adjusted(half, half, -half, -half);
}

QSizeF ArrowExtent(const ArrowHeading heading)
{
    const qreal single = std::max(kArrowLength, HeadExtent());
    const qreal along = heading == ArrowHeading::LeftAndRight ? single + HeadExtent() : single;

    return ItLies(heading) ? QSizeF(along, kArrowSpan) : QSizeF(kArrowSpan, along);
}

void PaintArrow(QPainter& painter, const QRectF& box, const ArrowHeading heading, const QColor& ink)
{
    const QTransform toTheGrid = painter.deviceTransform();
    const ArrowOnTheGrid arrow = FittedToTheGrid(heading, 1.0 / OneDevicePixel(painter));
    const QPointF middle = toTheGrid.map(box.center());
    const bool lying = ItLies(heading);
    const bool tipFirst = TheTipComesFirst(heading);

    const qreal starts = std::round((lying ? middle.x() : middle.y()) - arrow.length / 2.0);
    const qreal tip = tipFirst ? starts : starts + arrow.length;
    const qreal onwards = tipFirst ? 1.0 : -1.0;
    const qreal axis = std::round((lying ? middle.y() : middle.x()) - arrow.half) + arrow.half;

    QPolygonF outline;
    for (const QPointF& corner : OutlineOf(arrow, heading))
    {
        const qreal along = tip + onwards * corner.x();
        const qreal across = axis + corner.y();

        outline << (lying ? QPointF(along, across) : QPointF(across, along));
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawPolygon(toTheGrid.inverted().map(outline));
    painter.restore();
}

QFont TagFont(const QFont& base)
{
    return SmallLabelFont(base);
}

QFont SpineFont(const QFont& base)
{
    QFont font = ScaledFont(base, kSpineTextScale);
    font.setWeight(QFont::DemiBold);
    font.setLetterSpacing(QFont::PercentageSpacing, 104);

    return font;
}

QSize TagSizeOf(const QString& text, const QFont& base)
{
    const QFontMetrics measured(TagFont(base));

    return {measured.horizontalAdvance(text.toUpper()) + 2 * kTagPaddingX, measured.height() + 2 * kTagPaddingY};
}

void PaintTag(QPainter& painter, const QRect& box, const QString& text, const TagTone tone, const QFont& base)
{
    const TagPaint paint = PaintOf(tone);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setFont(TagFont(base));

    painter.setPen(Qt::NoPen);
    painter.setBrush(paint.ground == QColor(Qt::transparent) ? QBrush(Qt::NoBrush) : QBrush(paint.ground));
    painter.drawRect(box);

    if (paint.rule != paint.ground)
    {
        painter.setPen(QPen(paint.rule, OneDevicePixel(painter)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(OutlineInside(painter, box));
    }

    painter.setPen(paint.ink);
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
}

QColor AlarmingRowGround()
{
    return TonesOf(CurrentColorScheme()).alarming;
}

QColor PointedAtRowGround()
{
    return TonesOf(CurrentColorScheme()).raised;
}

QColor QuietInk()
{
    return TonesOf(CurrentColorScheme()).secondary;
}

QColor AlertInk()
{
    return TonesOf(CurrentColorScheme()).accentInk;
}

QIcon GearIcon(const int side, const qreal ratio)
{
    const int pixels = qCeil(side * ratio);

    QPixmap pixmap(pixels, pixels);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    const qreal middle = pixels / (2.0 * ratio);
    const QPointF centre(middle, middle);
    const qreal outer = side * 0.42;
    const qreal inner = side * 0.30;

    QPainterPath teeth;
    for (int tooth = 0; tooth < 8; ++tooth)
    {
        const qreal from = tooth * 45.0 - 9.0;
        QPainterPath wedge;
        wedge.moveTo(centre);
        wedge.arcTo(QRectF(centre.x() - outer, centre.y() - outer, outer * 2, outer * 2), from, 18.0);
        wedge.closeSubpath();
        teeth = teeth.united(wedge);
    }

    QPainterPath body;
    body.addEllipse(centre, inner, inner);

    QPainterPath hole;
    hole.addEllipse(centre, side * 0.12, side * 0.12);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(TonesOf(CurrentColorScheme()).secondary);
    painter.drawPath(teeth.united(body).subtracted(hole));
    painter.end();

    return {pixmap};
}

void DressTheHeaderOf(QHeaderView* header)
{
    if (header == nullptr)
    {
        return;
    }

    new HeaderDresser(header, SmallLabelFont(header->font()));

    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setHighlightSections(false);
}

void GiveItTheRole(QWidget* widget, const QString& role)
{
    if (widget == nullptr)
    {
        return;
    }

    widget->setProperty("role", role);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void LetTheRailBeAsWideAsItsEntries(QListWidget* rail, const int atLeast)
{
    if (rail == nullptr)
    {
        return;
    }

    rail->ensurePolished();
    rail->setFixedWidth(std::max(atLeast, rail->sizeHintForColumn(0) + 2 * rail->frameWidth()));
}
