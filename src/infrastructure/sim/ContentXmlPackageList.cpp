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
    listPath_ = std::move(listPath);
}

std::vector<PackageEntry> ContentXmlPackageList::Entries() const
{
    const std::optional<std::string> document = BytesOf(listPath_);

    return document.has_value() ? PackageEntriesIn(*document) : std::vector<PackageEntry>{};
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

FileResult ContentXmlPackageList::Switch(const std::string_view packageName, const bool activated)
{
    const std::optional<std::string> before = BytesOf(listPath_);
    if (!before.has_value())
    {
        return FileResult::CouldNotReadThePackageList;
    }

    const std::optional<std::string> after = WithPackageSwitched(*before, packageName, activated);
    if (!after.has_value())
    {
        return FileResult::TheDiskDisagreesWithTheScan;
    }

    if (*after == *before)
    {
        return FileResult::Completed;
    }

    if (!WriteFileReplacing(BackupOfPackageList(listPath_), *before))
    {
        return FileResult::CouldNotWriteThePackageList;
    }

    return WriteFileReplacing(listPath_, *after) ? FileResult::Completed : FileResult::CouldNotWriteThePackageList;
}
