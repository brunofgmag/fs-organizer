#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H

#include <algorithm>

#include <QtGui/QScreen>
#include <QtWidgets/QLayout>
#include <QtWidgets/QWidget>

inline constexpr int kPageGutter = 10;

inline void SizeToTheContent(QWidget& dialog, QLayout& layout, const int wide)
{
    layout.activate();

    const QScreen* screen = dialog.screen();
    const int tall = layout.totalHeightForWidth(wide);

    dialog.resize(wide, screen == nullptr ? tall : std::min(tall, screen->availableGeometry().height() * 4 / 5));
}

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
