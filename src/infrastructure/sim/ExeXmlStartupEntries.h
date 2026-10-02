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

    void KeepRemovedEntriesIn(std::filesystem::path removedFilePath);

    [[nodiscard]] std::vector<StartupEntry> Entries() const override;

    [[nodiscard]] std::vector<StartupRemovedEntry> Removed() const override;

    [[nodiscard]] StartupApplied Apply(const StartupChange& change, StartupBackup& backup) override;

private:
    struct FilesInUse
    {
        std::filesystem::path startup{};
        std::filesystem::path removed{};
    };

    [[nodiscard]] FilesInUse Files() const;

    mutable std::mutex guard_;
    std::mutex applying_;
    std::filesystem::path filePath_;
    std::filesystem::path removedFilePath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H
