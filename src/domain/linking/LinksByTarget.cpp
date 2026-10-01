#include "domain/linking/LinksByTarget.h"

#include <atomic>

#include "domain/support/PathUtils.h"

namespace
{
    std::atomic<std::size_t> gTimesItWasBuilt{0};
}

LinksByTarget::LinksByTarget(const std::vector<DestinationEntry>& entries)
{
    gTimesItWasBuilt.fetch_add(1, std::memory_order_relaxed);

    for (const DestinationEntry& entry : entries)
    {
        if (CountsAsEnabled(entry.classification))
        {
            links_[ComparablePath(entry.target)].push_back(entry.path);
        }
    }
}

const std::vector<std::filesystem::path>& LinksByTarget::PointingAt(const std::filesystem::path& addonFolder) const
{
    return PointingAtComparable(ComparablePath(addonFolder));
}

const std::vector<std::filesystem::path>& LinksByTarget::PointingAtComparable(const std::string& folderKey) const
{
    static const std::vector<std::filesystem::path> none;

    const auto known = links_.find(folderKey);

    return known == links_.end() ? none : known->second;
}

std::size_t LinksByTarget::TimesItWasBuilt()
{
    return gTimesItWasBuilt.load(std::memory_order_relaxed);
}
