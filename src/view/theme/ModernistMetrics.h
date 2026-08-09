#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H

#include <algorithm>

#include <QtGui/QScreen>
#include <QtWidgets/QLayout>
#include <QtWidgets/QWidget>

inline constexpr int kPageGutter = 10;

inline void SizeToTheContent(QWidget& dialog, const int wide)
{
    QLayout* layout = dialog.layout();

    if (layout != nullptr)
    {
        layout->activate();
    }

    const bool alongTheWidth = layout != nullptr && layout->hasHeightForWidth();
    const int tall = alongTheWidth ? layout->totalHeightForWidth(wide) : dialog.sizeHint().height();

    const QScreen* screen = dialog.screen();

    dialog.resize(wide, screen == nullptr ? tall : std::min(tall, screen->availableGeometry().height() * 4 / 5));
}

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
