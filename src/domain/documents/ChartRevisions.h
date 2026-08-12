#ifndef FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_REVISIONS_H
#define FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_REVISIONS_H

#include <filesystem>
#include <vector>

struct PageRevision
{
    int page = 0;
    std::filesystem::path file{};
    long long version = 0;
};

struct WhichRevisionOfEachPage
{
    std::vector<PageRevision> inForce{};
    std::vector<PageRevision> previous{};
};

[[nodiscard]] WhichRevisionOfEachPage TheRevisionInForceOfEachPage(const std::vector<PageRevision>& pages);

#endif // FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_REVISIONS_H
