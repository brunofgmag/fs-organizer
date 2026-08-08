#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H

#include <filesystem>
#include <vector>

#include "application/ports/StartupEntries.h"

class ExeXmlStartupEntries final : public StartupEntries
{
public:
    explicit ExeXmlStartupEntries(std::filesystem::path filePath);

    void Use(std::filesystem::path filePath);

    [[nodiscard]] std::vector<StartupEntry> Entries() const override;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, bool enabled) override;

private:
    std::filesystem::path filePath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_STARTUP_ENTRIES_H
