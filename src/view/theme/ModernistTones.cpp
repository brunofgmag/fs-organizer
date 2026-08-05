#include "view/theme/ModernistTones.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>

namespace
{
    constexpr int kGroundPartsPerTint = 6;

    QColor Tinted(const QColor& ground, const QColor& tint)
    {
        constexpr int kParts = kGroundPartsPerTint + 1;

        return {(ground.red() * kGroundPartsPerTint + tint.red()) / kParts,
                (ground.green() * kGroundPartsPerTint + tint.green()) / kParts,
                (ground.blue() * kGroundPartsPerTint + tint.blue()) / kParts};
    }

    ModernistTones WithTheGroundsItDerives(ModernistTones tones)
    {
        tones.alarming = Tinted(tones.window, tones.accent);

        return tones;
    }

    ModernistTones DarkTones()
    {
        return WithTheGroundsItDerives({
            .window = QColor("#1c1a19"),
            .chrome = QColor("#141312"),
            .raised = QColor("#262322"),
            .divider = QColor("#3a3634"),
            .edge = QColor("#4a4542"),
            .text = QColor("#f3f2f2"),
            .secondary = QColor("#a8a29e"),
            .faint = QColor("#8a8581"),
            .disabled = QColor("#6b6560"),
            .accent = QColor("#cc2c11"),
            .accentWarm = QColor("#e03112"),
            .accentInk = QColor("#ff6a4d"),
            .onAccent = QColor("#ffffff"),
        });
    }

    ModernistTones LightTones()
    {
        return WithTheGroundsItDerives({
            .window = QColor("#f3f2f2"),
            .chrome = QColor("#eae9e9"),
            .raised = QColor("#f8f4f4"),
            .divider = QColor("#bab6b6"),
            .edge = QColor("#9b9797"),
            .text = QColor("#201e1d"),
            .secondary = QColor("#504d4d"),
            .faint = QColor("#6c6868"),
            .disabled = QColor("#9b9797"),
            .accent = QColor("#cc2c11"),
            .accentWarm = QColor("#e03112"),
            .accentInk = QColor("#ae260e"),
            .onAccent = QColor("#ffffff"),
        });
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
