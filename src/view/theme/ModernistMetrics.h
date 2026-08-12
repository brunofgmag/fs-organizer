#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H

#include <algorithm>

#include <QtGui/QScreen>
#include <QtWidgets/QLayout>
#include <QtWidgets/QWidget>

inline constexpr int kPageGutter = 10;

[[nodiscard]] inline int TheTallestADialogMayBe(const QWidget& dialog)
{
    const QWidget* window = dialog.parentWidget() == nullptr ? nullptr : dialog.parentWidget()->window();

    if (window != nullptr)
    {
        return window->height();
    }

    const QScreen* screen = dialog.screen();

    if (screen != nullptr)
    {
        return screen->availableGeometry().height() * 4 / 5;
    }

    return 0;
}

inline void SizeToTheContent(QWidget& dialog, const int wide, const int tall)
{
    QLayout* layout = dialog.layout();

    const int ceiling = TheTallestADialogMayBe(dialog);
    const int floor = layout == nullptr ? 0 : layout->minimumSize().height();

    if (ceiling == 0 || (tall <= ceiling && floor <= ceiling))
    {
        dialog.resize(wide, tall);

        return;
    }

    if (layout != nullptr && floor > ceiling)
    {
        layout->setSizeConstraint(QLayout::SetNoConstraint);
        dialog.setMinimumHeight(0);
    }

    dialog.resize(wide, std::min(tall, ceiling));
}

inline void SizeToTheContent(QWidget& dialog, const int wide)
{
    QLayout* layout = dialog.layout();

    if (layout != nullptr)
    {
        layout->activate();
    }

    const bool alongTheWidth = layout != nullptr && layout->hasHeightForWidth();

    SizeToTheContent(dialog, wide, alongTheWidth ? layout->totalHeightForWidth(wide) : dialog.sizeHint().height());
}

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_METRICS_H
