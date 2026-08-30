#ifndef FS_ORGANIZER_APPLICATION_SESSION_H
#define FS_ORGANIZER_APPLICATION_SESSION_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "application/LibraryOrganizer.h"
#include "application/ProfileService.h"
#include "application/model/AppSettings.h"
#include "application/model/FileOperationResult.h"
#include "application/model/LegacyImport.h"
#include "application/model/ImportOperationResult.h"
#include "application/model/LibraryReport.h"
#include "application/model/ProfileSnapshot.h"
#include "application/model/LinkOperationResult.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/ProcessProbe.h"
#include "application/ports/SessionObserver.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

class Session
{
public:
    Session(ProfileService& service,
            const LibraryOrganizer& organizer,
            SettingsRepository& repository,
            AppSettings stored,
            const ProcessProbe& probe,
            BackgroundRunner& runner,
            SessionObserver& observer);

    [[nodiscard]] const AppSettings& Settings() const;

    bool Rewrite(const std::function<bool(AppSettings&)>& change);

    void NoteLinkResults(const std::vector<LinkOperationResult>& results);

    void ShowActiveProfile();

    void ChooseProfile(const std::string& profileId);

    [[nodiscard]] bool RemoveProfile(const std::string& profileId);

    [[nodiscard]] const SimulatorProfile& Profile() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

    [[nodiscard]] bool Scanning() const;

    void CancelScan();

    void RefreshEntries();

    void RefreshStartupEntries();

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path);

    struct LibraryRegistration
    {
        SimulatorProfile profile{};
        LibraryReport report{};
    };

    [[nodiscard]] bool WouldAcceptLibrary(const std::filesystem::path& path) const;

    [[nodiscard]] LibraryRegistration RegisterLibraryOn(SimulatorProfile profile,
                                                        const std::filesystem::path& path) const;

    void AdoptTheRegistration(LibraryRegistration registered);

    [[nodiscard]] LibraryGrouping HowTheLibraryIsGrouped(const LibraryId& libraryId) const;

    [[nodiscard]] std::vector<FileOperationResult> AdoptTheStructureOf(const LibraryId& libraryId);

    [[nodiscard]] std::vector<FileOperationResult> TakeBackTheMarkersOf(const LibraryId& libraryId);

    void RememberWhatCameFromAnotherProgram(const std::vector<ImportOperationResult>& results);

    void ForgetWhatCameFromAnotherProgram(const std::vector<std::filesystem::path>& addonFolders);

    [[nodiscard]] LegacyImportReport ImportLegacy(const LegacyImportRequest& request);

    void UnregisterLibrary(const LibraryId& libraryId);

    void RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to);

    [[nodiscard]] std::vector<DestinationOverride> OverridesPointingNowhere() const;

    void DropOverridesPointingNowhere();

    void OverrideDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination);

    [[nodiscard]] FileOperationResult CreateCategory(const std::filesystem::path& parent, const std::string& name);

    [[nodiscard]] FileOperationResult RemoveCategory(const std::filesystem::path& category);

    [[nodiscard]] FileOperationResult RenameCategory(const std::filesystem::path& category, const std::string& name);

    [[nodiscard]] std::vector<FileOperationResult> MoveAddons(const std::vector<AddonMove>& moves);

    struct ReorganizedLibrary
    {
        SimulatorProfile profile{};
        std::vector<FileOperationResult> results{};
    };

    [[nodiscard]] FileOperationResult CheckRenameCategory(const std::filesystem::path& category,
                                                          const std::string& name) const;

    [[nodiscard]] ReorganizedLibrary MoveAddonsOn(SimulatorProfile profile, const std::vector<AddonMove>& moves) const;

    [[nodiscard]] ReorganizedLibrary
    RenameCategoryOn(SimulatorProfile profile, const std::filesystem::path& category, const std::string& name) const;

    [[nodiscard]] ReorganizedLibrary RemoveCategoryOn(SimulatorProfile profile,
                                                      const std::filesystem::path& category) const;

    void AdoptTheReorganization(SimulatorProfile next, bool landed);

private:
    [[nodiscard]] const Library* LibraryNamed(const LibraryId& libraryId) const;

    [[nodiscard]] bool RememberTheDestination(const TreeNode& node, const std::filesystem::path& destination);

    void Scan(SimulatorProfile profile);

    void ScanBeforeReturning(SimulatorProfile profile);

    void Adopt();

    void Save(const SimulatorProfile& profile);

    bool Commit(AppSettings next);

    ProfileService& service_;
    const LibraryOrganizer& organizer_;
    SettingsRepository& repository_;
    AppSettings settings_;
    const ProcessProbe& probe_;
    BackgroundRunner& runner_;
    SessionObserver& observer_;
    SimulatorProfile profile_;
    ProfileSnapshot snapshot_;
    SimulatorProfile scanning_;
    ProfileSnapshot scanned_;
    std::optional<SimulatorProfile> queued_;
    std::atomic<bool> cancelled_{false};
    bool running_ = false;
    bool warnedAboutSimulator_ = false;
    bool restartPending_ = false;
};

#endif // FS_ORGANIZER_APPLICATION_SESSION_H
