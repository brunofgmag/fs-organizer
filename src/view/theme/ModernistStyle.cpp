#include "view/theme/ModernistStyle.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QWidget>

#include "view/theme/ModernistPaint.h"

namespace
{
    constexpr int kCheckSide = 15;
    constexpr int kRadioSide = 14;
    constexpr qreal kHairline = 1.5;
    constexpr qreal kFocusRing = 2.0;

    class PainterState
    {
    public:
        explicit PainterState(QPainter& painter) : painter_(painter)
        {
            painter_.save();
        }

        ~PainterState()
        {
            painter_.restore();
        }

        PainterState(const PainterState&) = delete;
        PainterState(PainterState&&) = delete;
        PainterState& operator=(const PainterState&) = delete;
        PainterState& operator=(PainterState&&) = delete;

    private:
        QPainter& painter_;
    };

    QRectF SquareInside(const QRect& given, const int side)
    {
        QRect box(0, 0, side, side);
        box.moveCenter(given.center());

        return box;
    }

    QPainterPath TickInside(const QRectF& box)
    {
        const qreal unitX = box.width() / 10.0;
        const qreal unitY = box.height() / 8.0;

        QPainterPath tick;
        tick.moveTo(box.left() + 1.6 * unitX, box.top() + 4.2 * unitY);
        tick.lineTo(box.left() + 4.1 * unitX, box.top() + 6.5 * unitY);
        tick.lineTo(box.left() + 8.6 * unitX, box.top() + 1.8 * unitY);

        return tick;
    }

    QPainterPath TriangleIn(const QRectF& box, const bool open)
    {
        const QPointF centre = box.center();
        const qreal reach = 3.4;

        QPainterPath triangle;

        if (open)
        {
            triangle.moveTo(centre.x() - reach, centre.y() - reach * 0.6);
            triangle.lineTo(centre.x() + reach, centre.y() - reach * 0.6);
            triangle.lineTo(centre.x(), centre.y() + reach * 0.7);
        }
        else
        {
            triangle.moveTo(centre.x() - reach * 0.6, centre.y() - reach);
            triangle.lineTo(centre.x() - reach * 0.6, centre.y() + reach);
            triangle.lineTo(centre.x() + reach * 0.7, centre.y());
        }

        triangle.closeSubpath();

        return triangle;
    }
}

ModernistStyle::ModernistStyle(const Qt::ColorScheme scheme)
    : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))), tones_(TonesOf(scheme))
{
}

int ModernistStyle::pixelMetric(const PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
    switch (metric)
    {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight: return kCheckSide;
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight: return kRadioSide;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing: return 8;
    default: break;
    }

    return QProxyStyle::pixelMetric(metric, option, widget);
}

void ModernistStyle::drawPrimitive(const PrimitiveElement element,
                                   const QStyleOption* option,
                                   QPainter* painter,
                                   const QWidget* widget) const
{
    if (option == nullptr || painter == nullptr)
    {
        return;
    }

    switch (element)
    {
    case PE_IndicatorCheckBox:
    case PE_IndicatorItemViewItemCheck: DrawCheckBox(*option, *painter); return;
    case PE_IndicatorRadioButton: DrawRadioButton(*option, *painter); return;
    case PE_IndicatorBranch: DrawBranch(*option, *painter); return;
    case PE_FrameFocusRect:
        if (option->state.testFlag(State_KeyboardFocusChange))
        {
            DrawFocusRing(*option, *painter, widget);
        }
        return;
    case PE_PanelItemViewItem:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        DrawSelectionHairline(*option, *painter);
        return;
    default: break;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void ModernistStyle::DrawCheckBox(const QStyleOption& option, QPainter& painter) const
{
    const QRectF box = SquareInside(option.rect, kCheckSide);
    const bool off = option.state.testFlag(State_Off);
    const bool live = option.state.testFlag(State_Enabled);

    const PainterState guard(painter);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (off)
    {
        painter.setPen(QPen(live ? tones_.secondary : tones_.tertiary, kHairline));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box.adjusted(kHairline / 2, kHairline / 2, -kHairline / 2, -kHairline / 2));
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(live ? tones_.accent : tones_.tertiary);
    painter.drawRect(box);

    painter.setPen(QPen(tones_.onAccent, 2.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);

    if (option.state.testFlag(State_NoChange))
    {
        const QPointF middle = box.center();
        painter.drawLine(QPointF(middle.x() - 3.5, middle.y()), QPointF(middle.x() + 3.5, middle.y()));
        return;
    }

    painter.drawPath(TickInside(box));
}

void ModernistStyle::DrawRadioButton(const QStyleOption& option, QPainter& painter) const
{
    const QRectF box = SquareInside(option.rect, kRadioSide);
    const bool live = option.state.testFlag(State_Enabled);

    const PainterState guard(painter);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (option.state.testFlag(State_Off))
    {
        painter.setPen(QPen(live ? tones_.secondary : tones_.tertiary, kHairline));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(box.adjusted(kHairline / 2, kHairline / 2, -kHairline / 2, -kHairline / 2));
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(live ? tones_.accent : tones_.tertiary);
    painter.drawEllipse(box);

    painter.setPen(QPen(tones_.window, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(box.adjusted(3.0, 3.0, -3.0, -3.0));
}

void ModernistStyle::DrawBranch(const QStyleOption& option, QPainter& painter) const
{
    if (!option.state.testFlag(State_Children))
    {
        return;
    }

    const PainterState guard(painter);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tones_.secondary);
    painter.drawPath(TriangleIn(option.rect, option.state.testFlag(State_Open)));
}

void ModernistStyle::DrawSelectionHairline(const QStyleOption& option, QPainter& painter) const
{
    const auto* cell = qstyleoption_cast<const QStyleOptionViewItem*>(&option);
    if (cell == nullptr || !cell->state.testFlag(State_Selected))
    {
        return;
    }

    const QRectF box = OutlineInside(painter, cell->rect);
    const PainterState guard(painter);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(tones_.secondary, OneDevicePixel(painter)));

    painter.drawLine(box.topLeft(), box.topRight());
    painter.drawLine(box.bottomLeft(), box.bottomRight());

    const QStyleOptionViewItem::ViewItemPosition where = cell->viewItemPosition;

    if (where == QStyleOptionViewItem::Beginning || where == QStyleOptionViewItem::OnlyOne
        || where == QStyleOptionViewItem::Invalid)
    {
        painter.drawLine(box.topLeft(), box.bottomLeft());
    }

    if (where == QStyleOptionViewItem::End || where == QStyleOptionViewItem::OnlyOne
        || where == QStyleOptionViewItem::Invalid)
    {
        painter.drawLine(box.topRight(), box.bottomRight());
    }
}

void ModernistStyle::DrawFocusRing(const QStyleOption& option, QPainter& painter, const QWidget* widget) const
{
    const auto* button = qobject_cast<const QAbstractButton*>(widget);
    const QRect around = button != nullptr ? button->rect() : option.rect;

    const PainterState guard(painter);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(tones_.accent, kFocusRing, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(around).adjusted(kFocusRing / 2, kFocusRing / 2, -kFocusRing / 2, -kFocusRing / 2));
}
