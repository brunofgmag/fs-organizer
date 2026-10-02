#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/model/FileResult.h"
#include "infrastructure/sim/RemovedStartupEntriesFile.h"

struct StartupDocumentChange
{
    FileResult result = FileResult::Completed;
    std::string document{};
    std::optional<StartupEntry> was{};
    std::optional<RemovedStartupEntry> taken{};
};

[[nodiscard]] std::vector<StartupEntry> StartupEntriesIn(std::string_view document);

[[nodiscard]] bool StartupDocumentIsUtf16(std::string_view document);

[[nodiscard]] std::optional<std::string>
WithStartupEntrySwitched(std::string_view document, const std::filesystem::path& entryPath, bool enabled);

[[nodiscard]] std::string NewStartupDocument();

[[nodiscard]] StartupDocumentChange WithStartupEntryAdded(std::string_view document, const StartupAddition& addition);

[[nodiscard]] StartupDocumentChange WithStartupEntryRemoved(std::string_view document,
                                                            const std::filesystem::path& entryPath);

[[nodiscard]] StartupDocumentChange WithStartupEntryEdited(std::string_view document, const StartupEditing& editing);

[[nodiscard]] StartupDocumentChange WithStartupEntryRestored(std::string_view document,
                                                             const RemovedStartupEntry& removed);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_EXE_XML_DOCUMENT_H
