#include "view/theme/ArrowButton.h"

#include <algorithm>

#include <QtCore/QtMath>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStyleOptionFocusRect>
#include <QtWidgets/QStylePainter>

namespace
{
    constexpr int kBetweenArrowAndText = 6;
}

ArrowButton::ArrowButton(QWidget* parent) : QPushButton(parent)
{
}

void ArrowButton::LeadWith(const std::optional<ArrowHeading> heading)
{
    lead_ = heading;
    updateGeometry();
    update();
}

int ArrowButton::LeadWidth() const
{
    if (!lead_.has_value() || text().isEmpty())
    {
        return 0;
    }

    return qCeil(ArrowExtent(*lead_).width()) + kBetweenArrowAndText;
}

QSize ArrowButton::sizeHint() const
{
    const QSize plain = QPushButton::sizeHint();

    return {plain.width() + LeadWidth(), plain.height()};
}

QSize ArrowButton::minimumSizeHint() const
{
    return sizeHint();
}

void ArrowButton::paintEvent(QPaintEvent*)
{
    QStyleOptionButton option;
    initStyleOption(&option);

    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButtonBevel, option);

    const QRect inside = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
    const int words = option.fontMetrics.horizontalAdvance(option.text);
    const int spare = std::max(0, inside.width() - LeadWidth() - words) / 2;

    QStyleOptionButton label = option;
    label.rect = inside.adjusted(spare + LeadWidth(), 0, -spare, 0);
    painter.drawControl(QStyle::CE_PushButtonLabel, label);

    if (option.state.testFlag(QStyle::State_HasFocus))
    {
        QStyleOptionFocusRect ring;
        ring.QStyleOption::operator=(option);
        ring.rect = style()->subElementRect(QStyle::SE_PushButtonFocusRect, &option, this);
        painter.drawPrimitive(QStyle::PE_FrameFocusRect, ring);
    }

    if (!lead_.has_value())
    {
        return;
    }

    const QSizeF extent = ArrowExtent(*lead_);
    const QRectF beforeTheWords(inside.left() + spare, inside.top(), extent.width(), inside.height());

    PaintArrow(painter, text().isEmpty() ? QRectF(rect()) : beforeTheWords, *lead_,
               palette().color(QPalette::ButtonText));
}
