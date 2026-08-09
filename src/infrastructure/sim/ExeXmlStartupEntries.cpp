#include "infrastructure/sim/ExeXmlStartupEntries.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "infrastructure/sim/ExeXmlDocument.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "support/FileWriting.h"

namespace
{
    [[nodiscard]] std::optional<std::string> BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return std::nullopt;
        }

        return std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
    }
}

ExeXmlStartupEntries::ExeXmlStartupEntries(std::filesystem::path filePath) : filePath_(std::move(filePath))
{
}

void ExeXmlStartupEntries::Use(std::filesystem::path filePath)
{
    filePath_ = std::move(filePath);
}

std::vector<StartupEntry> ExeXmlStartupEntries::Entries() const
{
    const std::optional<std::string> document = BytesOf(filePath_);

    return document.has_value() ? StartupEntriesIn(*document) : std::vector<StartupEntry>{};
}

FileResult ExeXmlStartupEntries::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    const std::optional<std::string> before = BytesOf(filePath_);
    if (!before.has_value())
    {
        return FileResult::CouldNotReadTheStartupFile;
    }

    const std::optional<std::string> after = WithStartupEntrySwitched(*before, entryPath, enabled);
    if (!after.has_value())
    {
        return FileResult::TheDiskDisagreesWithTheScan;
    }

    if (*after == *before)
    {
        return FileResult::Completed;
    }

    if (!WriteFileReplacing(BackupOfStartupFile(filePath_), *before))
    {
        return FileResult::CouldNotWriteTheStartupFile;
    }

    return WriteFileReplacing(filePath_, *after) ? FileResult::Completed : FileResult::CouldNotWriteTheStartupFile;
}
