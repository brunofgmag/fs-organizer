#ifndef FS_ORGANIZER_VIEW_WRAPPING_ROW_H
#define FS_ORGANIZER_VIEW_WRAPPING_ROW_H

#include <QtCore/QList>
#include <QtWidgets/QLayout>

class WrappingRow final : public QLayout
{
public:
    explicit WrappingRow(QWidget* parent);

    ~WrappingRow() override;

    void AddSpring();

    void addItem(QLayoutItem* item) override;

    [[nodiscard]] int count() const override;

    [[nodiscard]] QLayoutItem* itemAt(int index) const override;

    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;

    [[nodiscard]] bool hasHeightForWidth() const override;

    [[nodiscard]] int heightForWidth(int width) const override;

    void setGeometry(const QRect& rect) override;

    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QSize minimumSize() const override;

private:
    [[nodiscard]] int Gap() const;

    [[nodiscard]] QList<QList<QLayoutItem*>> LinesThatFit(int width) const;

    void PlaceTheLine(const QList<QLayoutItem*>& row, const QRect& where) const;

    QList<QLayoutItem*> items_;
};

#endif // FS_ORGANIZER_VIEW_WRAPPING_ROW_H
