#include "domain/importing/CopyConflicts.h"

#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    const CopyConflict* Lookup(const std::vector<CopyConflict>& found,
                               const std::map<std::string, std::size_t>& index,
                               const std::filesystem::path& path)
    {
        const auto match = index.find(ComparablePath(path));

        return match == index.end() ? nullptr : &found[match->second];
    }
}

CopyConflicts::CopyConflicts(std::vector<CopyConflict> found) : found_(std::move(found))
{
    for (std::size_t position = 0; position < found_.size(); ++position)
    {
        byDestination_.emplace(ComparablePath(found_[position].destinationPath), position);
        byLibrary_.emplace(ComparablePath(found_[position].libraryPath), position);
    }
}

const CopyConflict* CopyConflicts::OverTheDestinationEntry(const std::filesystem::path& entry) const
{
    return Lookup(found_, byDestination_, entry);
}

const CopyConflict* CopyConflicts::OverTheLibraryAddon(const std::filesystem::path& addonFolder) const
{
    return Lookup(found_, byLibrary_, addonFolder);
}

const std::vector<CopyConflict>& CopyConflicts::All() const
{
    return found_;
}

std::size_t CopyConflicts::Count() const
{
    return found_.size();
}

CopyConflicts FindCopyConflicts(const std::vector<DestinationEntry>& entries, const std::vector<TreeNode>& libraries)
{
    std::vector<CopyConflict> found;

    for (const DestinationEntry& entry : entries)
    {
        if (entry.classification != EntryClassification::Unmanaged)
        {
            continue;
        }

        if (const TreeNode* addon = AddonNamed(libraries, entry.path.filename().string()))
        {
            found.push_back(CopyConflict{.destinationPath = entry.path, .libraryPath = addon->path});
        }
    }

    return CopyConflicts{std::move(found)};
}
