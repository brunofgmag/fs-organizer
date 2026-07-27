#ifndef FS_ORGANIZER_APPLICATION_SESSION_H
#define FS_ORGANIZER_APPLICATION_SESSION_H

#include <filesystem>
#include <optional>
#include <string>

#include "application/ProfileService.h"
#include "application/model/LibraryReport.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/SessionObserver.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

class Session
{
public:
    Session(ProfileService& service, SettingsRepository& settings, BackgroundRunner& runner, SessionObserver& observer);

    void ShowActiveProfile();

    void ChooseProfile(const std::string& profileId);

    [[nodiscard]] const SimulatorProfile& Profile() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

    [[nodiscard]] bool Scanning() const;

    void RefreshEntries();

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path);

    void OverrideDestination(const TreeNode& node, const std::filesystem::path& destination);

private:
    void Scan(SimulatorProfile profile);

    void Adopt();

    void Save(const SimulatorProfile& profile) const;

    ProfileService& service_;
    SettingsRepository& settings_;
    BackgroundRunner& runner_;
    SessionObserver& observer_;
    SimulatorProfile profile_;
    ProfileSnapshot snapshot_;
    SimulatorProfile scanning_;
    ProfileSnapshot scanned_;
    std::optional<SimulatorProfile> queued_;
    bool running_ = false;
};

#endif // FS_ORGANIZER_APPLICATION_SESSION_H
