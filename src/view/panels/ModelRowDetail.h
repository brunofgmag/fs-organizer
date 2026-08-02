#ifndef FS_ORGANIZER_VIEW_PANELS_MODEL_ROW_DETAIL_H
#define FS_ORGANIZER_VIEW_PANELS_MODEL_ROW_DETAIL_H

#include <QtCore/QModelIndex>
#include <QtCore/QPair>
#include <QtWidgets/QWidget>

class QGridLayout;

class ModelRowDetail final : public QWidget
{
    Q_OBJECT

public:
    using Field = QPair<QString, QString>;

    explicit ModelRowDetail(QWidget* parent = nullptr);

    void Show(const QModelIndex& index);

    void ShowFields(const QList<Field>& fields);

protected:
    void changeEvent(QEvent* event) override;

private:
    void Clear() const;

    QGridLayout* rows_ = nullptr;
    QPersistentModelIndex shown_;
};

#endif // FS_ORGANIZER_VIEW_PANELS_MODEL_ROW_DETAIL_H
