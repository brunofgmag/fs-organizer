#ifndef FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H
#define FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H

#include <cstddef>

#include <QtWidgets/QWidget>

class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;

class TriageStrip final : public QWidget
{
    Q_OBJECT

public:
    explicit TriageStrip(QWidget* parent = nullptr);

    void ShowBreakdown(std::size_t broken, std::size_t conflicts, std::size_t duplicated, std::size_t unmanaged);

    [[nodiscard]] bool HasAnythingToSay() const;

signals:
    void RepairRequested();

    void ResolveRequested();

    void DuplicatesRequested();

    void ImportRequested();

private:
    struct Item
    {
        QLabel* label = nullptr;
        QPushButton* action = nullptr;
    };

    [[nodiscard]] Item AddItem(const char* tag, const QString& action, QHBoxLayout* into);

    [[nodiscard]] QFrame* AddSeparator(QHBoxLayout* into);

    static void ShowItem(const Item& item, bool shown);

    Item broken_;
    Item conflicts_;
    Item duplicated_;
    Item unmanaged_;
    QFrame* beforeConflicts_ = nullptr;
    QFrame* beforeDuplicated_ = nullptr;
    bool anythingToSay_ = false;
};

#endif // FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H
