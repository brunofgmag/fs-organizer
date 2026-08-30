#include "infrastructure/sim/ContentXmlPackageList.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "infrastructure/sim/ContentXmlDocument.h"
#include "infrastructure/sim/PackageNaming.h"
#include "support/FileWriting.h"

namespace
{
    constexpr std::string_view kBackupSuffix = ".fsorg-backup";

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

std::filesystem::path BackupOfPackageList(const std::filesystem::path& listPath)
{
    std::filesystem::path backup = listPath;
    backup += kBackupSuffix;

    return backup;
}

ContentXmlPackageList::ContentXmlPackageList(std::filesystem::path listPath) : listPath_(std::move(listPath))
{
}

void ContentXmlPackageList::Use(std::filesystem::path listPath)
{
    const std::lock_guard lock(guard_);

    listPath_ = std::move(listPath);
    parsedAt_.reset();
    parsed_.clear();
}

std::vector<PackageEntry> ContentXmlPackageList::Entries() const
{
    const std::lock_guard lock(guard_);

    std::error_code failed;
    const std::filesystem::file_time_type writtenAt = std::filesystem::last_write_time(listPath_, failed);

    if (failed)
    {
        parsedAt_.reset();
        parsed_.clear();

        return {};
    }

    if (parsedAt_.has_value() && *parsedAt_ == writtenAt)
    {
        return parsed_;
    }

    const std::optional<std::string> document = BytesOf(listPath_);

    parsed_ = document.has_value() ? PackageEntriesIn(*document) : std::vector<PackageEntry>{};
    parsedAt_ = writtenAt;

    return parsed_;
}

std::vector<SimulatorAirport> ContentXmlPackageList::AirportsTheSimulatorShips() const
{
    std::vector<SimulatorAirport> airports;

    for (const PackageEntry& entry : Entries())
    {
        if (!ItIsContentTheSimulatorShips(entry.name))
        {
            continue;
        }

        std::string code = AirportCodeInAPackageName(entry.name);
        if (code.empty())
        {
            continue;
        }

        airports.push_back({.packageName = entry.name,
                            .code = std::move(code),
                            .activated = entry.activation == PackageActivation::Activated});
    }

    return airports;
}

FileResult ContentXmlPackageList::SwitchAll(const std::vector<std::string>& packageNames, const bool activated)
{
    const std::optional<std::string> before = BytesOf(listPath_);
    if (!before.has_value())
    {
        return FileResult::CouldNotReadThePackageList;
    }

    std::string after = *before;

    for (const std::string& packageName : packageNames)
    {
        std::optional<std::string> switched = WithPackageSwitched(after, packageName, activated);
        if (!switched.has_value())
        {
            return FileResult::TheDiskDisagreesWithTheScan;
        }

        after = std::move(*switched);
    }

    if (after == *before)
    {
        return FileResult::Completed;
    }

    if (!WriteFileReplacing(BackupOfPackageList(listPath_), *before))
    {
        return FileResult::CouldNotWriteThePackageList;
    }

    return WriteFileReplacing(listPath_, after) ? FileResult::Completed : FileResult::CouldNotWriteThePackageList;
}
