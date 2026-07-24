#include "infrastructure/sim/WindowsUserCfgLocations.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shlobj.h>

#include <string>

namespace
{
    std::filesystem::path KnownFolder(const KNOWNFOLDERID& folderId)
    {
        PWSTR raw = nullptr;
        if (SHGetKnownFolderPath(folderId, 0, nullptr, &raw) != S_OK)
        {
            CoTaskMemFree(raw);
            return {};
        }

        std::filesystem::path folder(raw);
        CoTaskMemFree(raw);

        return folder;
    }
}

std::vector<UserCfgLocation> WindowsUserCfgLocations()
{
    const std::filesystem::path roaming = KnownFolder(FOLDERID_RoamingAppData);
    const std::filesystem::path local = KnownFolder(FOLDERID_LocalAppData);

    return {
        {
            SimulatorVariant::MSFS2020,
            local / "Packages" / "Microsoft.FlightSimulator_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"
        },
        {
            SimulatorVariant::MSFS2024,
            local / "Packages" / "Microsoft.Limitless_8wekyb3d8bbwe" / "LocalCache" / "UserCfg.opt"
        },
        {SimulatorVariant::MSFS2020, roaming / "Microsoft Flight Simulator" / "UserCfg.opt"},
        {SimulatorVariant::MSFS2024, roaming / "Microsoft Flight Simulator 2024" / "UserCfg.opt"},
    };
}
