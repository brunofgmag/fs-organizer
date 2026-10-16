#include "infrastructure/legacy/ProtonLegacyConfigSource.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "domain/support/PathUtils.h"
#include "infrastructure/legacy/IniLegacyConfigReader.h"
#include "infrastructure/legacy/LegacyPresetReader.h"
#include "infrastructure/sim/ChildNamedIgnoringCase.h"
#include "infrastructure/sim/PrefixPathMapper.h"

namespace
{
    constexpr auto kInstallationFolderPrefix = "msfs addons linker";
    constexpr auto kConfigFileName = "MSFS_Addons_Linker.ini";

    bool NamesAnInstallation(const std::filesystem::path& folder)
    {
        return ComparablePath(folder.filename()).starts_with(kInstallationFolderPrefix);
    }

    std::optional<LegacyInstallation> InTheHostsPaths(std::optional<LegacyInstallation> installation,
                                                      const PrefixPathMapper& mapper)
    {
        if (!installation.has_value())
        {
            return installation;
        }

        for (std::filesystem::path& addonPath : installation->addonPaths)
        {
            addonPath = mapper.ToHost(addonPath);
        }

        installation->presetsPath = mapper.ToHost(installation->presetsPath);

        return installation;
    }

    std::vector<FoundLegacyInstallation> InstallationsIn(const ProtonPrefix& prefix)
    {
        std::error_code failure;
        std::vector<FoundLegacyInstallation> found;
        const PrefixPathMapper mapper(prefix.path);

        const std::optional<std::filesystem::path> programData =
            ChildNamedIgnoringCase(prefix.path / "drive_c", "ProgramData");

        if (!programData.has_value())
        {
            return found;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(*programData, failure))
        {
            if (!entry.is_directory(failure) || !NamesAnInstallation(entry.path()))
            {
                continue;
            }

            const std::optional<std::filesystem::path> configuration =
                ChildNamedIgnoringCase(entry.path(), kConfigFileName);

            if (configuration.has_value())
            {
                found.push_back(FoundLegacyInstallation{
                    .folder = entry.path(), .configuration = InTheHostsPaths(ReadLegacyIni(*configuration), mapper)});
            }
        }

        return found;
    }

    bool PresetsOf(const FoundLegacyInstallation& installation, const std::filesystem::path& folder)
    {
        return installation.configuration.has_value()
            && ComparablePath(installation.configuration->presetsPath) == ComparablePath(folder);
    }

    const ProtonPrefix* PrefixHoldingThePresets(const std::vector<ProtonPrefix>& prefixes,
                                                const std::filesystem::path& folder)
    {
        for (const ProtonPrefix& prefix : prefixes)
        {
            const std::vector<FoundLegacyInstallation> found = InstallationsIn(prefix);

            if (std::ranges::any_of(found,
                                    [&folder](const FoundLegacyInstallation& installation)
                                    {
                                        return PresetsOf(installation, folder);
                                    }))
            {
                return &prefix;
            }
        }

        const auto inside = std::ranges::find_if(prefixes,
                                                 [&folder](const ProtonPrefix& prefix)
                                                 {
                                                     return PathIsInside(folder, prefix.path);
                                                 });

        return inside == prefixes.end() ? nullptr : &*inside;
    }
}

ProtonLegacyConfigSource::ProtonLegacyConfigSource(std::vector<ProtonPrefix> prefixes) : prefixes_(std::move(prefixes))
{
}

std::vector<FoundLegacyInstallation> ProtonLegacyConfigSource::Installations() const
{
    std::vector<FoundLegacyInstallation> found;

    for (const ProtonPrefix& prefix : prefixes_)
    {
        std::vector<FoundLegacyInstallation> inThePrefix = InstallationsIn(prefix);

        std::ranges::move(inThePrefix, std::back_inserter(found));
    }

    std::ranges::sort(found,
                      [](const FoundLegacyInstallation& left, const FoundLegacyInstallation& right)
                      {
                          return ComparablePath(left.folder) < ComparablePath(right.folder);
                      });

    return found;
}

std::vector<LegacyPresetSelection> ProtonLegacyConfigSource::PresetsIn(const std::filesystem::path& folder) const
{
    std::vector<LegacyPresetSelection> presets = ReadLegacyPresetsIn(folder);
    const ProtonPrefix* owner = PrefixHoldingThePresets(prefixes_, folder);

    if (owner == nullptr)
    {
        return presets;
    }

    const PrefixPathMapper mapper(owner->path);

    for (LegacyPresetSelection& preset : presets)
    {
        for (std::filesystem::path& enabledFolder : preset.enabledFolders)
        {
            enabledFolder = mapper.ToHost(enabledFolder);
        }
    }

    return presets;
}
