#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_STYLE_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_STYLE_H

#include <QtWidgets/QProxyStyle>

#include "view/theme/ModernistTones.h"

class ModernistStyle final : public QProxyStyle
{
    Q_OBJECT

public:
    explicit ModernistStyle(Qt::ColorScheme scheme);

    [[nodiscard]] int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override;

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption* option,
                       QPainter* painter,
                       const QWidget* widget) const override;

private:
    void DrawCheckBox(const QStyleOption& option, QPainter& painter) const;

    void DrawRadioButton(const QStyleOption& option, QPainter& painter) const;

    void DrawBranch(const QStyleOption& option, QPainter& painter) const;

    void DrawSelectionHairline(const QStyleOption& option, QPainter& painter) const;

    void DrawFocusRing(const QStyleOption& option, QPainter& painter, const QWidget* widget) const;

    ModernistTones tones_;
};

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_STYLE_H
