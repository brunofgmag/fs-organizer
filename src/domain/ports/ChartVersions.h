#ifndef FS_ORGANIZER_DOMAIN_PORTS_CHART_VERSIONS_H
#define FS_ORGANIZER_DOMAIN_PORTS_CHART_VERSIONS_H

#include <filesystem>

class ChartVersions
{
public:
    virtual ~ChartVersions() = default;

    [[nodiscard]] virtual long long VersionOf(const std::filesystem::path& chart) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_CHART_VERSIONS_H
