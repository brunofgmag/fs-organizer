#ifndef FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
#define FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H

#include <functional>
#include <vector>

#include <QtWidgets/QDialog>

#include "application/model/RestorePlan.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;

using AskAboutTheCollision = std::function<bool(const RestoreCheck&)>;

class RestoreDialog final : public QDialog
{
    Q_OBJECT

public:
    RestoreDialog(const std::vector<RestoreOffer>& offers,
                  AskAboutTheCollision askAboutTheCollision,
                  QWidget* parent = nullptr);

    [[nodiscard]] std::vector<QuarantinedItem> Restorable() const;

    [[nodiscard]] std::vector<QuarantinedItem> TheOnesReplacingWhatIsThere() const;

private:
    struct Choice
    {
        RestoreOffer offer{};
        QComboBox* places = nullptr;
    };

    struct Collision
    {
        RestoreOffer offer{};
        QPushButton* compare = nullptr;
        QLabel* chosen = nullptr;
        bool agreed = false;
    };

    void AddTheSettledRow(QGridLayout& grid, const RestoreOffer& offer, int row);

    void AddTheQuestionRow(QGridLayout& grid, const Choice& choice, int row);

    void AddTheCollisionRow(QGridLayout& grid, const Collision& collision, int row);

    void AskAbout(Collision& collision);

    void ShowHowManyWillGoBack() const;

    AskAboutTheCollision askAboutTheCollision_;
    std::vector<RestoreOffer> settled_;
    std::vector<Choice> asked_;
    std::vector<Collision> collided_;
    QLabel* counted_ = nullptr;
    QPushButton* restore_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_QUARANTINE_RESTORE_DIALOG_H
