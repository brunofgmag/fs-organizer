#ifndef FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H
#define FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/FileResult.h"

struct StartupEntry
{
    std::string label{};
    std::filesystem::path path{};
    bool enabled = true;
};

class StartupEntries
{
public:
    virtual ~StartupEntries() = default;

    [[nodiscard]] virtual std::vector<StartupEntry> Entries() const = 0;

    [[nodiscard]] virtual FileResult Switch(const std::filesystem::path& entryPath, bool enabled) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H
