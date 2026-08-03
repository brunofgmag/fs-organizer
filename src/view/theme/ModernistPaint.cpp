#include "view/theme/ModernistPaint.h"

#include <algorithm>

#include <QtCore/QEvent>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtWidgets/QHeaderView>

#include "view/theme/ModernistTones.h"

namespace
{
    constexpr int kTagPaddingX = 10;
    constexpr int kTagPaddingY = 5;
    constexpr qreal kTagTextScale = 0.78;
    constexpr qreal kHeaderTextScale = 0.82;
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

    QFont HeaderFont(const QFont& base)
    {
        QFont label = ScaledFont(base, kHeaderTextScale);
        label.setWeight(QFont::DemiBold);
        label.setCapitalization(QFont::AllUppercase);
        label.setLetterSpacing(QFont::PercentageSpacing, 107);

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
        case TagTone::Outlined: return {.ground = Qt::transparent, .ink = tones.accentBright, .rule = tones.accent};
        case TagTone::Muted: return {.ground = tones.raised, .ink = tones.secondary, .rule = tones.raised};
        case TagTone::Line: break;
        }

        return {.ground = Qt::transparent, .ink = tones.secondary, .rule = tones.edge};
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

QFont TagFont(const QFont& base)
{
    QFont font = ScaledFont(base, kTagTextScale);
    font.setWeight(QFont::DemiBold);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::PercentageSpacing, 105);

    return font;
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
    const ModernistTones tones = TonesOf(CurrentColorScheme());

    QColor ground = tones.window;
    ground.setRed((ground.red() * 4 + tones.accent.red()) / 5);
    ground.setGreen((ground.green() * 4 + tones.accent.green()) / 5);
    ground.setBlue((ground.blue() * 4 + tones.accent.blue()) / 5);

    return ground;
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
    return TonesOf(CurrentColorScheme()).accentBright;
}

QIcon GearIcon(const int side)
{
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);

    const QPointF centre(side / 2.0, side / 2.0);
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

    return QIcon(pixmap);
}

void DressTheHeaderOf(QHeaderView* header)
{
    if (header == nullptr)
    {
        return;
    }

    new HeaderDresser(header, HeaderFont(header->font()));

    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setHighlightSections(false);
}
