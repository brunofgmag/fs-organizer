#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_PAINT_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_PAINT_H

#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QIcon>

#include "viewmodel/TagTone.h"

class QFont;
class QHeaderView;
class QPainter;

[[nodiscard]] qreal OneDevicePixel(const QPainter& painter);

[[nodiscard]] QRectF OutlineInside(const QPainter& painter, const QRectF& box);

[[nodiscard]] QFont TagFont(const QFont& base);

[[nodiscard]] QFont SpineFont(const QFont& base);

[[nodiscard]] QSize TagSizeOf(const QString& text, const QFont& base);

void PaintTag(QPainter& painter, const QRect& box, const QString& text, TagTone tone, const QFont& base);

[[nodiscard]] QColor AlarmingRowGround();

[[nodiscard]] QColor PointedAtRowGround();

[[nodiscard]] QColor QuietInk();

[[nodiscard]] QColor AlertInk();

[[nodiscard]] QIcon GearIcon(int side);

void DressTheHeaderOf(QHeaderView* header);

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_PAINT_H
