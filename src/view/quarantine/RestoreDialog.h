#ifndef FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
#define FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/model/RestorePlan.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;

class RestoreDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDialog(const std::vector<RestoreOffer>& offers, QWidget* parent = nullptr);

    [[nodiscard]] std::vector<QuarantinedItem> Restorable() const;

private:
    struct Choice
    {
        RestoreOffer offer{};
        QComboBox* places = nullptr;
    };

    void AddTheSettledRow(QGridLayout& grid, const RestoreOffer& offer, int row);

    void AddTheQuestionRow(QGridLayout& grid, const Choice& choice, int row);

    void ShowHowManyWillGoBack() const;

    std::vector<RestoreOffer> settled_;
    std::vector<Choice> asked_;
    QLabel* counted_ = nullptr;
    QPushButton* restore_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
