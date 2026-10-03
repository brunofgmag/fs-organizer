#ifndef FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H
#define FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H

#include <optional>

#include <QtWidgets/QToolButton>

#include "view/theme/ModernistPaint.h"

class PageTab final : public QToolButton
{
    Q_OBJECT

public:
    explicit PageTab(const QString& label, QWidget* parent = nullptr);

    void ShowCount(std::optional<qsizetype> count);

    void Relabel(const QString& label);

    void LeadWith(ArrowHeading heading);

    [[nodiscard]] QString Label() const;

    void RememberSource(const char* source);

    [[nodiscard]] const char* Source() const;

    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] QString CountText() const;

    [[nodiscard]] int LeadWidth() const;

    QString label_;
    const char* source_ = nullptr;
    std::optional<qsizetype> count_;
    std::optional<ArrowHeading> lead_;
};

#endif // FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H
