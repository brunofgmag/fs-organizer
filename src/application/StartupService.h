#ifndef FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H
#define FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H

#include <filesystem>
#include <vector>

#include "application/ports/ProcessProbe.h"
#include "application/ports/StartupEntries.h"

class StartupService
{
public:
    StartupService(StartupEntries& entries, const ProcessProbe& processProbe);

    [[nodiscard]] std::vector<StartupEntry> Entries() const;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, bool enabled);

private:
    StartupEntries& entries_;
    const ProcessProbe& processProbe_;
};

#endif // FS_ORGANIZER_APPLICATION_STARTUP_SERVICE_H
