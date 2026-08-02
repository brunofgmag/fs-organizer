#ifndef FS_ORGANIZER_VIEW_PANELS_EMPTY_STATE_H
#define FS_ORGANIZER_VIEW_PANELS_EMPTY_STATE_H

#include <QtWidgets/QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

class EmptyState final : public QWidget
{
    Q_OBJECT

public:
    EmptyState(const QString& headline, const QString& explanation, QWidget* parent = nullptr);

    QPushButton* OfferTheOnlyAction(const QString& label);

    void Retell(const QString& headline, const QString& explanation) const;

private:
    QVBoxLayout* column_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_PANELS_EMPTY_STATE_H
