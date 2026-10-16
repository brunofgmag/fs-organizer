#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PREFIX_PATH_MAPPER_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PREFIX_PATH_MAPPER_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class PrefixPathMapper
{
public:
    explicit PrefixPathMapper(std::filesystem::path prefix);

    [[nodiscard]] std::filesystem::path ToHost(const std::filesystem::path& simulatorPath) const;

    [[nodiscard]] std::filesystem::path ToSimulator(const std::filesystem::path& hostPath) const;

private:
    struct Mounted
    {
        char letter = 'Z';
        std::vector<std::string> parts{};
    };

    [[nodiscard]] std::optional<std::filesystem::path> TargetOfDrive(char letter) const;

    [[nodiscard]] std::optional<Mounted> BestDriveFor(const std::vector<std::string>& hostParts) const;

    std::filesystem::path prefix_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PREFIX_PATH_MAPPER_H
