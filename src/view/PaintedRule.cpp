#include "view/PaintedRule.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QPainter>

namespace
{
    constexpr int kRoomForARule = 2;
}

PaintedRule::PaintedRule(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ListRule"));
    setFixedHeight(kRoomForARule);
}

void PaintedRule::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    const QTransform toTheGrid = painter.deviceTransform();
    const QRectF room = toTheGrid.mapRect(QRectF(rect()));
    const qreal left = std::ceil(room.left());
    const qreal thick = std::max(1.0, std::round(toTheGrid.m22()));
    const QRectF line(left, std::ceil(room.top()), std::floor(room.right()) - left, thick);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(toTheGrid.inverted().mapRect(line), palette().color(QPalette::Mid));
}
