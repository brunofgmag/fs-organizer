#include "view/theme/ModernistTones.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>

namespace
{
    ModernistTones DarkTones()
    {
        return {
            .window = QColor("#1c1a19"),
            .chrome = QColor("#141312"),
            .raised = QColor("#262322"),
            .divider = QColor("#3a3634"),
            .edge = QColor("#4a4542"),
            .text = QColor("#f3f2f2"),
            .secondary = QColor("#a8a29e"),
            .tertiary = QColor("#6b6560"),
            .accent = QColor("#ec3013"),
            .accentBright = QColor("#ff6a4d"),
            .onAccent = QColor("#ffffff"),
        };
    }

    ModernistTones LightTones()
    {
        return {
            .window = QColor("#f3f2f2"),
            .chrome = QColor("#eae9e9"),
            .raised = QColor("#f8f4f4"),
            .divider = QColor("#bab6b6"),
            .edge = QColor("#9b9797"),
            .text = QColor("#201e1d"),
            .secondary = QColor("#605d5d"),
            .tertiary = QColor("#9b9797"),
            .accent = QColor("#ec3013"),
            .accentBright = QColor("#dd2b0f"),
            .onAccent = QColor("#ffffff"),
        };
    }
}

ModernistTones TonesOf(const Qt::ColorScheme scheme)
{
    return scheme == Qt::ColorScheme::Dark ? DarkTones() : LightTones();
}

Qt::ColorScheme CurrentColorScheme()
{
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();

    return scheme == Qt::ColorScheme::Unknown ? Qt::ColorScheme::Light : scheme;
}
