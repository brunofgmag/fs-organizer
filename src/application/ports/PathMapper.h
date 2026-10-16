#ifndef FS_ORGANIZER_APPLICATION_PORTS_PATH_MAPPER_H
#define FS_ORGANIZER_APPLICATION_PORTS_PATH_MAPPER_H

#include <filesystem>

#include "domain/model/SimulatorProfile.h"

class PathMapper
{
public:
    virtual ~PathMapper() = default;

    [[nodiscard]] virtual std::filesystem::path ToHost(SimulatorVariant variant,
                                                       const std::filesystem::path& simulatorPath) const = 0;

    [[nodiscard]] virtual std::filesystem::path ToSimulator(SimulatorVariant variant,
                                                            const std::filesystem::path& hostPath) const = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_PATH_MAPPER_H
