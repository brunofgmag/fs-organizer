#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_REMOVED_STARTUP_ENTRIES_FILE_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_REMOVED_STARTUP_ENTRIES_FILE_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "domain/model/SimulatorProfile.h"

struct RemovedStartupEntry
{
    std::string label{};
    std::filesystem::path path{};
    std::string commandLine{};
    bool enabled = true;
    std::string block{};
    std::filesystem::path followedBy{};
    std::chrono::system_clock::time_point removedAt{};
};

[[nodiscard]] std::filesystem::path RemovedStartupEntriesFileOf(const std::filesystem::path& folder,
                                                                SimulatorVariant variant);

class RemovedStartupEntriesFile
{
public:
    explicit RemovedStartupEntriesFile(std::filesystem::path filePath);

    [[nodiscard]] std::vector<RemovedStartupEntry> Entries() const;

    [[nodiscard]] bool Keep(const RemovedStartupEntry& entry) const;

    [[nodiscard]] bool Drop(const std::filesystem::path& entryPath) const;

private:
    [[nodiscard]] std::optional<std::vector<RemovedStartupEntry>> Read() const;

    [[nodiscard]] bool Write(const std::vector<RemovedStartupEntry>& entries) const;

    std::filesystem::path filePath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_REMOVED_STARTUP_ENTRIES_FILE_H
