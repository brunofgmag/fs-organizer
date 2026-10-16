#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PREFIX_PATH_MAPPER_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PREFIX_PATH_MAPPER_H

#include <filesystem>
#include <string>
#include <string_view>

#include "application/ports/PathMapper.h"

class FakePrefixPathMapper final : public PathMapper
{
public:
    [[nodiscard]] std::filesystem::path ToHost(const SimulatorVariant,
                                               const std::filesystem::path& simulatorPath) const override
    {
        const std::string text = simulatorPath.generic_string();

        if (!text.starts_with(kDrive))
        {
            return simulatorPath;
        }

        return driveC / std::filesystem::path(text.substr(kDrive.size())).relative_path();
    }

    [[nodiscard]] std::filesystem::path ToSimulator(const SimulatorVariant,
                                                    const std::filesystem::path& hostPath) const override
    {
        const std::filesystem::path inside = hostPath.lexically_relative(driveC);

        if (inside.empty() || *inside.begin() == "..")
        {
            return hostPath;
        }

        return std::filesystem::path(std::string(kDrive) + "/") / inside;
    }

    std::filesystem::path driveC{"/home/pilot/.steam/steam/steamapps/compatdata/2537590/pfx/drive_c"};

private:
    static constexpr std::string_view kDrive = "C:";
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PREFIX_PATH_MAPPER_H
