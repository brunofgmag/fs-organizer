#include "infrastructure/legacy/WindowsLegacyConfigSource.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <utility>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/IniLegacyConfigReader.h"
#include "infrastructure/legacy/LegacyPresetReader.h"

namespace
{
    constexpr auto kInstallationFolderPrefix = "msfs addons linker";
    constexpr auto kConfigFileName = "MSFS_Addons_Linker.ini";

    bool NamesAnInstallation(const std::filesystem::path& folder)
    {
        return ComparablePath(folder.filename()).starts_with(kInstallationFolderPrefix);
    }
}

WindowsLegacyConfigSource::WindowsLegacyConfigSource(std::filesystem::path programData)
    : programData_(std::move(programData))
{
}

std::vector<FoundLegacyInstallation> WindowsLegacyConfigSource::Installations() const
{
    std::error_code failure;
    std::vector<FoundLegacyInstallation> found;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(programData_, failure))
    {
        if (!entry.is_directory(failure) || !NamesAnInstallation(entry.path()))
        {
            continue;
        }

        const std::filesystem::path configuration = entry.path() / kConfigFileName;
        if (!std::filesystem::exists(configuration, failure))
        {
            continue;
        }

        found.push_back(FoundLegacyInstallation{.folder = entry.path(), .configuration = ReadLegacyIni(configuration)});
    }

    std::ranges::sort(found,
                      [](const FoundLegacyInstallation& left, const FoundLegacyInstallation& right)
                      {
                          return ComparablePath(left.folder) < ComparablePath(right.folder);
                      });

    return found;
}

std::vector<LegacyPresetSelection> WindowsLegacyConfigSource::PresetsIn(const std::filesystem::path& folder) const
{
    return ReadLegacyPresetsIn(folder);
}
