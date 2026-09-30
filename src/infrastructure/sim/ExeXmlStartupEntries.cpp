#include "infrastructure/sim/ExeXmlStartupEntries.h"

#include <fstream>
#include <iterator>
#include <mutex>
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
    const std::lock_guard lock(guard_);

    filePath_ = std::move(filePath);
    batchHasBackedUp_ = false;
}

std::filesystem::path ExeXmlStartupEntries::FilePath() const
{
    const std::lock_guard lock(guard_);

    return filePath_;
}

bool ExeXmlStartupEntries::BackupIsDue() const
{
    const std::lock_guard lock(guard_);

    return !batchIsOpen_ || !batchHasBackedUp_;
}

void ExeXmlStartupEntries::BackupWasTaken()
{
    const std::lock_guard lock(guard_);

    batchHasBackedUp_ = batchIsOpen_;
}

void ExeXmlStartupEntries::OpenBatch()
{
    const std::lock_guard lock(guard_);

    batchIsOpen_ = true;
    batchHasBackedUp_ = false;
}

void ExeXmlStartupEntries::CloseBatch()
{
    const std::lock_guard lock(guard_);

    batchIsOpen_ = false;
    batchHasBackedUp_ = false;
}

std::vector<StartupEntry> ExeXmlStartupEntries::Entries() const
{
    const std::optional<std::string> document = BytesOf(FilePath());

    return document.has_value() ? StartupEntriesIn(*document) : std::vector<StartupEntry>{};
}

FileResult ExeXmlStartupEntries::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    const std::filesystem::path filePath = FilePath();

    const std::optional<std::string> before = BytesOf(filePath);
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

    if (BackupIsDue())
    {
        if (!WriteFileReplacing(BackupOfStartupFile(filePath), *before))
        {
            return FileResult::CouldNotWriteTheStartupFile;
        }

        BackupWasTaken();
    }

    return WriteFileReplacing(filePath, *after) ? FileResult::Completed : FileResult::CouldNotWriteTheStartupFile;
}
