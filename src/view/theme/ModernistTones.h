#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_TONES_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_TONES_H

#include <QtGui/QColor>

struct ModernistTones
{
    QColor window;
    QColor chrome;
    QColor raised;
    QColor divider;
    QColor edge;
    QColor text;
    QColor secondary;
    QColor tertiary;
    QColor accent;
    QColor accentBright;
    QColor onAccent;
};

[[nodiscard]] ModernistTones TonesOf(Qt::ColorScheme scheme);

[[nodiscard]] Qt::ColorScheme CurrentColorScheme();

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_TONES_H
