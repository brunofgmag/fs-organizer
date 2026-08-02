#ifndef FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H
#define FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H

#include <QtWidgets/QWidget>

#include "viewmodel/AttentionBreakdown.h"

class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;

class TriageStrip final : public QWidget
{
    Q_OBJECT

public:
    explicit TriageStrip(QWidget* parent = nullptr);

    void ShowBreakdown(const AttentionBreakdown& breakdown);

    [[nodiscard]] bool HasAnythingToSay() const;

signals:
    void RepairRequested();

    void ResolveRequested();

    void DuplicatesRequested();

    void ImportRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    struct Item
    {
        QLabel* label = nullptr;
        QPushButton* action = nullptr;
    };

    void RetranslateUi();

    [[nodiscard]] Item AddItem(const char* tag, QHBoxLayout* into);

    [[nodiscard]] QFrame* AddSeparator(QHBoxLayout* into);

    static void ShowItem(const Item& item, bool shown);

    Item broken_;
    Item conflicts_;
    Item duplicated_;
    Item unmanaged_;
    QFrame* beforeConflicts_ = nullptr;
    QFrame* beforeDuplicated_ = nullptr;
    AttentionBreakdown shown_;
    bool anythingToSay_ = false;
};

#endif // FS_ORGANIZER_VIEW_SHELL_TRIAGE_STRIP_H
