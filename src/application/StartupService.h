#ifndef FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H
#define FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "application/StartupReport.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/ProcessProbe.h"
#include "application/ports/StartupEntries.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/FilesystemProbe.h"

class StartupService
{
public:
    StartupService(StartupEntries& entries,
                   const ProcessProbe& processProbe,
                   const FilesystemProbe& filesystemProbe,
                   bool managing);

    void Manage(bool managing);

    [[nodiscard]] bool Managing() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] std::vector<StartupEntry> Entries() const;

    [[nodiscard]] StartupReport Report(const SimulatorProfile& profile, const ProfileSnapshot& snapshot) const;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, bool enabled);

private:
    StartupEntries& entries_;
    const ProcessProbe& processProbe_;
    const FilesystemProbe& filesystemProbe_;
    bool managing_;
};

#endif // FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H
