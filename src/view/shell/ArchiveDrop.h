#ifndef FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_H
#define FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_H

#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "viewmodel/ArchiveInstallViewModel.h"

class ArchiveDropLibrary;
class ArchiveDropSign;
class InstallDialog;
class QDropEvent;
class QMimeData;
class QTimer;
class QWidget;
class StepProgressDialog;

inline constexpr int kReadingIsSlowAfter = 400;

class ArchiveDrop final : public QObject
{
    Q_OBJECT

public:
    ArchiveDrop(QWidget& window, ArchiveInstallViewModel& viewModel, ArchiveDropLibrary& library);

signals:
    void LibraryRequested();

    void StatusChanged(const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Welcome
    {
        Yes,
        Busy,
        NotASource,
        NoLibrary,
    };

    [[nodiscard]] Welcome WelcomeFor(const QMimeData& dragged);

    [[nodiscard]] const std::vector<std::filesystem::path>& SourcesIn(const QMimeData& dragged);

    void ForgetTheDrag();

    void Invite(QDropEvent& event);

    void Refuse(QDropEvent& event, const QString& sentence) const;

    void InviteIntoWhatIsUnder(const QPoint& point) const;

    void Receive(QDropEvent& event);

    void ReadWhatWasDropped(const std::vector<std::filesystem::path>& files, const std::filesystem::path& category);

    [[nodiscard]] static QString SubjectOf(int archive, int of, const QString& fileName);

    void RememberWhatIsBeingRead(int archive, int of, const QString& fileName);

    void OpenTheReadingDialog();

    void OnReadingStopped();

    void OnOffer(const InstallOffer& offer);

    void ShowWhatCannotBeInstalled(const InstallOffer& offer);

    void OfferTheInstall(const InstallOffer& offer);

    void InstallWhatWasChosen(const InstallDialog& dialog);

    void ShowTheSubject(int archive, int of, const QString& fileName);

    void ShowTheStep(InstallStep step);

    void ShowTheBytes(qulonglong copied, qulonglong total);

    void ShowTheAddon(int addon, int of, const QString& folderName);

    void ShowItIsCancelling();

    void OnFinished(const InstallOutcome& outcome);

    [[nodiscard]] StepProgressDialog* ProgressDialog(const QString& title, const QStringList& steps);

    void Dismiss(QPointer<StepProgressDialog>& dialog);

    QWidget& window_;
    ArchiveInstallViewModel& viewModel_;
    ArchiveDropLibrary& library_;
    ArchiveDropSign* sign_ = nullptr;
    QTimer* readingIsSlow_ = nullptr;
    QString readingSubject_;
    QPointer<StepProgressDialog> reading_;
    QPointer<StepProgressDialog> installing_;
    QPointer<InstallDialog> offering_;
    std::optional<std::vector<std::filesystem::path>> sources_;
    const QMimeData* draggedFrom_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_H
