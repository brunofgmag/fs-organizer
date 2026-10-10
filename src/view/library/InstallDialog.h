#ifndef FS_ORGANIZER_VIEW_LIBRARY_INSTALL_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_INSTALL_DIALOG_H

#include <vector>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>

#include "viewmodel/InstallOffer.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

class CategoryField final : public QComboBox
{
    Q_OBJECT

public:
    using QComboBox::QComboBox;

    void SayInstead(const QString& text);

    [[nodiscard]] QString Shown() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString said_;
};

class InstallDialog final : public QDialog
{
    Q_OBJECT

public:
    enum Column
    {
        AddonColumn,
        VersionColumn,
        InstalledColumn,
        SizeColumn,
        CategoryColumn,
        ColumnCount,
    };

    explicit InstallDialog(InstallOffer offer, QWidget* parent = nullptr);

    void TakeAFreshOffer(InstallOffer offer);

    [[nodiscard]] std::vector<ChosenAddon> Chosen() const;

    [[nodiscard]] QString ChosenCategory() const;

    [[nodiscard]] bool EnablesAfterInstalling() const;

signals:
    void CheckAgainRequested();

private:
    enum class Discards
    {
        Forget,
        Keep,
    };

    void ListWhatWasFound();

    void ShowTheOffer();

    void FillTheLibraries();

    void KeepWhatWasChecked(const std::vector<ChosenAddon>& kept);

    void ShowTheSimulatorThatIsRunning();

    void ShowHowEachAddonStands(Discards discards);

    void ShowHowEachAddonStandsInTheChosenCategory();

    void GiveTheListItsHeight();

    void GiveTheCategoryColumnItsWidth();

    void ShowCategoriesOfTheChosenLibrary();

    void LandOnWhereItWasDropped();

    void FollowTheCheck(QStandardItem* changed);

    void CheckThemAll(bool checked);

    void ShowWhatIsChosen();

    [[nodiscard]] QString WhereTheCheckedReplacementsStay(const QStringList& installedIn) const;

    [[nodiscard]] const InstallStandingInALibrary& StandingOf(int archive, int addon) const;

    InstallOffer offer_;
    QStandardItemModel* model_ = nullptr;
    QTreeView* tree_ = nullptr;
    QCheckBox* all_ = nullptr;
    QLabel* total_ = nullptr;
    QComboBox* library_ = nullptr;
    CategoryField* category_ = nullptr;
    QLabel* hint_ = nullptr;
    QLabel* promise_ = nullptr;
    QCheckBox* enable_ = nullptr;
    QWidget* blocked_ = nullptr;
    QLabel* blockedBy_ = nullptr;
    QPushButton* install_ = nullptr;
    bool settling_ = false;
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_INSTALL_DIALOG_H
