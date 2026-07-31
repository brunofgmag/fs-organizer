#ifndef FS_ORGANIZER_APPLICATION_SESSION_H
#define FS_ORGANIZER_APPLICATION_SESSION_H

#include <filesystem>
#include <optional>
#include <string>

#include "application/LibraryOrganizer.h"
#include "application/ProfileService.h"
#include "application/model/FileOperationResult.h"
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
            SettingsRepository& settings,
            const ProcessProbe& probe,
            BackgroundRunner& runner,
            SessionObserver& observer);

    void NoteLinkResults(const std::vector<LinkOperationResult>& results);

    void ShowActiveProfile();

    void ChooseProfile(const std::string& profileId);

    [[nodiscard]] const SimulatorProfile& Profile() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

    [[nodiscard]] bool Scanning() const;

    void RefreshEntries();

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path);

    void UnregisterLibrary(const LibraryId& libraryId);

    void RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to);

    void OverrideDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination);

    [[nodiscard]] FileOperationResult CreateCategory(const std::filesystem::path& parent, const std::string& name);

    [[nodiscard]] FileOperationResult RemoveCategory(const std::filesystem::path& category);

    [[nodiscard]] FileOperationResult RenameCategory(const std::filesystem::path& category, const std::string& name);

    [[nodiscard]] std::vector<FileOperationResult> MoveAddons(const std::vector<AddonMove>& moves);

private:
    [[nodiscard]] bool RememberTheDestination(const TreeNode& node, const std::filesystem::path& destination);

    void Scan(SimulatorProfile profile);

    void Adopt();

    void Save(const SimulatorProfile& profile) const;

    ProfileService& service_;
    const LibraryOrganizer& organizer_;
    SettingsRepository& settings_;
    const ProcessProbe& probe_;
    BackgroundRunner& runner_;
    SessionObserver& observer_;
    SimulatorProfile profile_;
    ProfileSnapshot snapshot_;
    SimulatorProfile scanning_;
    ProfileSnapshot scanned_;
    std::optional<SimulatorProfile> queued_;
    bool running_ = false;
    bool warnedAboutSimulator_ = false;
    bool restartPending_ = false;
};

#endif // FS_ORGANIZER_APPLICATION_SESSION_H
