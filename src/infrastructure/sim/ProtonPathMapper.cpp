#include "infrastructure/sim/ProtonPathMapper.h"

#include <algorithm>
#include <utility>

#include "infrastructure/sim/PrefixPathMapper.h"

ProtonPathMapper::ProtonPathMapper(std::vector<ProtonPrefix> prefixes) : prefixes_(std::move(prefixes))
{
}

std::filesystem::path ProtonPathMapper::ToHost(const SimulatorVariant variant,
                                               const std::filesystem::path& simulatorPath) const
{
    const auto prefix = std::ranges::find(prefixes_, variant, &ProtonPrefix::variant);

    return prefix == prefixes_.end() ? simulatorPath : PrefixPathMapper(prefix->path).ToHost(simulatorPath);
}

std::filesystem::path ProtonPathMapper::ToSimulator(const SimulatorVariant variant,
                                                    const std::filesystem::path& hostPath) const
{
    const auto prefix = std::ranges::find(prefixes_, variant, &ProtonPrefix::variant);

    return prefix == prefixes_.end() ? hostPath : PrefixPathMapper(prefix->path).ToSimulator(hostPath);
}
