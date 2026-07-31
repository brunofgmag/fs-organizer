#ifndef FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H
#define FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H

#include <optional>

#include <QtWidgets/QToolButton>

class PageTab final : public QToolButton
{
    Q_OBJECT

public:
    explicit PageTab(const QString& label, QWidget* parent = nullptr);

    void ShowCount(std::optional<qsizetype> count);

    void Relabel(const QString& label);

    [[nodiscard]] QString Label() const;

    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] QString CountText() const;

    QString label_;
    std::optional<qsizetype> count_;
};

#endif // FS_ORGANIZER_VIEW_THEME_PAGE_TAB_H
