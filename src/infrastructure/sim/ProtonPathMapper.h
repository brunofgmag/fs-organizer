#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PATH_MAPPER_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PATH_MAPPER_H

#include <vector>

#include "application/ports/PathMapper.h"
#include "infrastructure/sim/ProtonPrefix.h"

class ProtonPathMapper final : public PathMapper
{
public:
    explicit ProtonPathMapper(std::vector<ProtonPrefix> prefixes);

    [[nodiscard]] std::filesystem::path ToHost(SimulatorVariant variant,
                                               const std::filesystem::path& simulatorPath) const override;

    [[nodiscard]] std::filesystem::path ToSimulator(SimulatorVariant variant,
                                                    const std::filesystem::path& hostPath) const override;

private:
    std::vector<ProtonPrefix> prefixes_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROTON_PATH_MAPPER_H
