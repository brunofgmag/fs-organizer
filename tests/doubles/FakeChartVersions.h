#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_VERSIONS_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_VERSIONS_H

#include <filesystem>
#include <map>
#include <vector>

#include "domain/ports/ChartVersions.h"

class FakeChartVersions final : public ChartVersions
{
public:
    void Answer(const std::filesystem::path& chart, const long long version)
    {
        answers_[chart] = version;
    }

    [[nodiscard]] long long VersionOf(const std::filesystem::path& chart) const override
    {
        asked.push_back(chart);

        const auto known = answers_.find(chart);

        if (known == answers_.end())
        {
            return 0;
        }

        return known->second;
    }

    mutable std::vector<std::filesystem::path> asked;

private:
    std::map<std::filesystem::path, long long> answers_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_CHART_VERSIONS_H
