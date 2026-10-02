#ifndef FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_DRAFT_DIALOG_H
#define FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_DRAFT_DIALOG_H

#include <filesystem>
#include <functional>
#include <optional>

#include <QtCore/QString>
#include <QtWidgets/QDialog>

#include "application/StartupEditor.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class TextThatIsNeverCut;

class StartupDraftDialog final : public QDialog
{
    Q_OBJECT

public:
    using CheckOfTheFile = std::function<StartupDraftCheck(const std::filesystem::path&)>;

    StartupDraftDialog(CheckOfTheFile check, const std::optional<StartupDraft>& editing, QWidget* parent = nullptr);

    void TakeTheProgram(const std::filesystem::path& file);

    [[nodiscard]] StartupDraft Draft() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    [[nodiscard]] QWidget* CreateProgramRow();

    [[nodiscard]] QWidget* CreateWhatTheCheckSays();

    void ChooseTheProgram();

    void ShowWhatTheCheckSays();

    void ShowTheProgram();

    void ShowTheRefusal(const StartupDraftCheck& check);

    void ShowThePathWritten(const QString& path);

    void AllowTheConfirmation();

    void Fit();

    CheckOfTheFile check_;
    bool editing_ = false;
    bool nameFollowsTheFile_ = true;
    bool refused_ = false;
    std::filesystem::path file_{};
    QLabel* noProgram_ = nullptr;
    QVBoxLayout* programColumn_ = nullptr;
    TextThatIsNeverCut* program_ = nullptr;
    QVBoxLayout* checkColumn_ = nullptr;
    QLabel* insideTheAddon_ = nullptr;
    TextThatIsNeverCut* pathWritten_ = nullptr;
    QLabel* addonIsOff_ = nullptr;
    QLabel* refusal_ = nullptr;
    QLabel* presetsFollow_ = nullptr;
    QLabel* goesToRemoved_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* commandLine_ = nullptr;
    QPushButton* confirm_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_DRAFT_DIALOG_H
