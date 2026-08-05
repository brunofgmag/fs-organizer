#ifndef FS_ORGANIZER_VIEW_PANELS_DEPENDENCY_SECTION_H
#define FS_ORGANIZER_VIEW_PANELS_DEPENDENCY_SECTION_H

#include <QtWidgets/QWidget>

#include "application/DependencyReport.h"

class QLabel;
class QVBoxLayout;

class DependencySection final : public QWidget
{
    Q_OBJECT

public:
    explicit DependencySection(QWidget* parent = nullptr);

    void Show(const DependencyReport& report);

protected:
    void changeEvent(QEvent* event) override;

private:
    void Rebuild() const;

    [[nodiscard]] QWidget* LineFor(const DependencyAnswer& answer) const;

    QLabel* title_ = nullptr;
    QWidget* lines_ = nullptr;
    QVBoxLayout* stack_ = nullptr;
    QLabel* note_ = nullptr;
    DependencyReport shown_;
};

#endif // FS_ORGANIZER_VIEW_PANELS_DEPENDENCY_SECTION_H
