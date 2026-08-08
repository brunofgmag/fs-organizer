#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/StartupEntries.h"

[[nodiscard]] std::vector<StartupEntry> StartupEntriesIn(std::string_view document);

[[nodiscard]] std::optional<std::string>
WithStartupEntrySwitched(std::string_view document, const std::filesystem::path& entryPath, bool enabled);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H
