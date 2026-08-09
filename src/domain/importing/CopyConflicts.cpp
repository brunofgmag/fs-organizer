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
        byProvenance_.emplace(ComparablePath(found_[position].provenancePath), position);
        byLibrary_.emplace(ComparablePath(found_[position].libraryPath), position);
    }
}

const CopyConflict* CopyConflicts::OverTheProvenance(const std::filesystem::path& provenance) const
{
    return Lookup(found_, byProvenance_, provenance);
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
        if (entry.theOtherProgramTookItsFolderBack)
        {
            found.push_back(CopyConflict{.provenancePath = entry.externalOrigin,
                                         .libraryPath = entry.libraryCopy,
                                         .theProvenanceIsAnotherProgram = true});
            continue;
        }

        if (entry.classification != EntryClassification::Unmanaged)
        {
            continue;
        }

        if (const TreeNode* addon = AddonNamed(libraries, AsUtf8(entry.path.filename())))
        {
            found.push_back(CopyConflict{.provenancePath = entry.path, .libraryPath = addon->path});
        }
    }

    return CopyConflicts{std::move(found)};
}
