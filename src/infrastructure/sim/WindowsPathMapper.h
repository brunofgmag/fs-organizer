#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PATH_MAPPER_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PATH_MAPPER_H

#include <algorithm>
#include <filesystem>

#include "application/ports/PathMapper.h"

class WindowsPathMapper final : public PathMapper
{
public:
    [[nodiscard]] std::filesystem::path ToHost(const SimulatorVariant,
                                               const std::filesystem::path& simulatorPath) const override
    {
        return simulatorPath;
    }

    [[nodiscard]] std::filesystem::path ToSimulator(const SimulatorVariant,
                                                    const std::filesystem::path& hostPath) const override
    {
        using Character = std::filesystem::path::value_type;

        std::filesystem::path::string_type text = hostPath.native();
        std::ranges::replace(text, Character{'/'}, Character{'\\'});

        return text;
    }
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_WINDOWS_PATH_MAPPER_H
