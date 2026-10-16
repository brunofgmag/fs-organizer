#include "infrastructure/sim/UserCfgPackagesPath.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
    constexpr std::string_view kInstalledPackagesKey = "InstalledPackagesPath";
}

std::optional<std::filesystem::path> InstalledPackagesPathIn(const std::filesystem::path& userCfg)
{
    std::ifstream file(userCfg, std::ios::binary);

    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t key = line.find(kInstalledPackagesKey);
        if (key == std::string::npos)
        {
            continue;
        }

        const std::size_t opening = line.find('"', key + kInstalledPackagesKey.size());
        if (opening == std::string::npos)
        {
            continue;
        }

        const std::size_t closing = line.find('"', opening + 1);
        if (closing == std::string::npos)
        {
            continue;
        }

        return line.substr(opening + 1, closing - opening - 1);
    }

    return std::nullopt;
}
