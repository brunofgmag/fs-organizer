#ifndef FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H
#define FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "domain/model/FileResult.h"

struct StartupEntry
{
    std::string label{};
    std::filesystem::path path{};
    std::string commandLine{};
    bool enabled = true;
};

struct StartupBackup
{
    bool taken = false;
};

struct StartupSwitching
{
    std::filesystem::path path{};
    bool enabled = true;
};

struct StartupAddition
{
    std::string label{};
    std::filesystem::path path{};
    std::string commandLine{};
};

struct StartupRemoval
{
    std::filesystem::path path{};
    std::chrono::system_clock::time_point at{};
};

struct StartupEditing
{
    std::filesystem::path path{};
    std::string label{};
    std::filesystem::path newPath{};
    std::string commandLine{};
    std::chrono::system_clock::time_point at{};
};

struct StartupRemovedEntry
{
    StartupEntry entry{};
    std::chrono::system_clock::time_point removedAt{};
};

struct StartupRestoring
{
    std::filesystem::path path{};
};

struct StartupForgetting
{
    std::filesystem::path path{};
};

using StartupChange = std::
    variant<StartupSwitching, StartupAddition, StartupRemoval, StartupEditing, StartupRestoring, StartupForgetting>;

struct StartupApplied
{
    FileResult result = FileResult::Completed;
    std::optional<StartupEntry> was{};
    bool changedNothing = false;
};

class StartupEntries
{
public:
    virtual ~StartupEntries() = default;

    [[nodiscard]] virtual std::vector<StartupEntry> Entries() const = 0;

    [[nodiscard]] virtual std::vector<StartupRemovedEntry> Removed() const = 0;

    [[nodiscard]] virtual StartupApplied Apply(const StartupChange& change, StartupBackup& backup) = 0;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, const bool enabled, StartupBackup& backup)
    {
        return Apply(StartupSwitching{.path = entryPath, .enabled = enabled}, backup).result;
    }

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, const bool enabled)
    {
        StartupBackup alone;

        return Switch(entryPath, enabled, alone);
    }
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_STARTUP_ENTRIES_H
