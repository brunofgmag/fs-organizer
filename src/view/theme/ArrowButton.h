#ifndef FS_ORGANIZER_VIEW_THEME_ARROW_BUTTON_H
#define FS_ORGANIZER_VIEW_THEME_ARROW_BUTTON_H

#include <optional>

#include <QtWidgets/QPushButton>

#include "view/theme/ModernistPaint.h"

class ArrowButton final : public QPushButton
{
    Q_OBJECT

public:
    explicit ArrowButton(QWidget* parent = nullptr);

    void LeadWith(std::optional<ArrowHeading> heading);

    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] int LeadWidth() const;

    std::optional<ArrowHeading> lead_;
};

#endif // FS_ORGANIZER_VIEW_THEME_ARROW_BUTTON_H
