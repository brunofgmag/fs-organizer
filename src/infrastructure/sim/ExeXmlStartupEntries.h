#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H

#include <filesystem>
#include <mutex>
#include <vector>

#include "application/ports/StartupEntries.h"

class ExeXmlStartupEntries final : public StartupEntries
{
public:
    explicit ExeXmlStartupEntries(std::filesystem::path filePath);

    void Use(std::filesystem::path filePath);

    [[nodiscard]] std::vector<StartupEntry> Entries() const override;

    using StartupEntries::Switch;

    [[nodiscard]] FileResult
    Switch(const std::filesystem::path& entryPath, bool enabled, StartupBackup& backup) override;

private:
    [[nodiscard]] std::filesystem::path FilePath() const;

    mutable std::mutex guard_;
    std::filesystem::path filePath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H
