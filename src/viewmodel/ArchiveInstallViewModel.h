#ifndef FS_ORGANIZER_VIEWMODEL_ARCHIVE_INSTALL_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ARCHIVE_INSTALL_VIEW_MODEL_H

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/ArchiveInstallService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/ProcessProbe.h"
#include "viewmodel/GuardedRunner.h"
#include "viewmodel/InstallOffer.h"
#include "viewmodel/SessionNotifier.h"

struct InstallOutcome
{
    std::optional<InstallReport> report{};
    int installed = 0;
    std::vector<std::filesystem::path> landed{};
    bool enableAfter = false;
};

class ArchiveInstallViewModel final : public QObject
{
    Q_OBJECT

public:
    ArchiveInstallViewModel(const ArchiveInstallService& service,
                            ProfileService& profileService,
                            const ProcessProbe& probe,
                            Session& session,
                            const SessionNotifier& notifier,
                            BackgroundRunner& runner,
                            QObject* parent = nullptr);

    void Read(const std::vector<std::filesystem::path>& archives, const std::filesystem::path& droppedOnCategory);

    void CheckAgain();

    void Install(const std::vector<ChosenAddon>& chosen, const QString& categoryPath, bool enableAfter);

    void Cancel();

    [[nodiscard]] bool Busy() const;

    [[nodiscard]] bool IsAFolder(const std::filesystem::path& source) const;

    [[nodiscard]] static InstallReport ReportOfWhatCannotBeInstalled(const InstallOffer& offer);

signals:
    void ReadingArchive(int archive, int of, const QString& fileName);

    void OfferReady(const InstallOffer& offer);

    void ReadingStopped();

    void InstallingArchive(int archive, int of, const QString& fileName);

    void InstallingAddon(int addon, int of, const QString& folderName);

    void StepChanged(InstallStep step);

    void Progressed(qulonglong copiedBytes, qulonglong totalBytes);

    void Cancelling();

    void Finished(const InstallOutcome& outcome);

    void LandedAndReadable(const std::vector<std::filesystem::path>& folders);

private:
    struct ArchiveRead
    {
        std::filesystem::path archive{};
        WhatIsInTheArchive found{};
    };

    struct Standings
    {
        std::vector<std::vector<std::vector<std::vector<AdmissionCheck>>>> ofEachArchive{};
        std::string runningSimulator{};
    };

    [[nodiscard]] std::vector<std::vector<std::vector<AdmissionCheck>>>
    StandingOf(const SimulatorProfile& profile,
               const std::vector<InstallLibraryChoice>& libraries,
               const ArchiveRead& read) const;

    [[nodiscard]] Standings StandingsOfAll(const SimulatorProfile& profile,
                                           const std::vector<InstallLibraryChoice>& libraries,
                                           const std::vector<ArchiveRead>& reads) const;

    [[nodiscard]] InstallOffer OfferOf(const std::vector<InstallLibraryChoice>& libraries,
                                       const std::vector<ArchiveRead>& reads,
                                       const Standings& standings) const;

    [[nodiscard]] std::function<void(OperationKind)> OnStep(SourceKind source);

    [[nodiscard]] std::function<bool(const CopyProgress&)> OnProgress();

    void AdoptTheEnd(const InstallOutcome& outcome);

    void ReadTheLibraryAgain(const InstallOutcome& outcome);

    const ArchiveInstallService& service_;
    ProfileService& profileService_;
    const ProcessProbe& probe_;
    Session& session_;
    const SessionNotifier& notifier_;
    GuardedRunner working_;
    std::vector<ArchiveRead> read_;
    std::filesystem::path droppedOn_;
    std::atomic<bool> cancelled_{false};
    bool installing_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_ARCHIVE_INSTALL_VIEW_MODEL_H
