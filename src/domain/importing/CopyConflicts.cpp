#include "domain/importing/CopyConflicts.h"

#include <optional>
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

    class AddonsByName
    {
    public:
        explicit AddonsByName(const std::vector<TreeNode>& libraries) : libraries_(libraries)
        {
        }

        [[nodiscard]] const TreeNode* Named(const std::filesystem::path& folderName) const
        {
            if (!byName_.has_value())
            {
                byName_ = Indexed(libraries_);
            }

            const auto match = byName_->find(ComparableFileName(folderName));

            return match == byName_->end() ? nullptr : match->second;
        }

    private:
        [[nodiscard]] static std::map<std::string, const TreeNode*> Indexed(const std::vector<TreeNode>& libraries)
        {
            std::map<std::string, const TreeNode*> byName;

            for (const TreeNode& library : libraries)
            {
                for (const TreeNode* addon : AddonsUnder(library))
                {
                    byName.emplace(ComparableFileName(addon->path), addon);
                }
            }

            return byName;
        }

        const std::vector<TreeNode>& libraries_;
        mutable std::optional<std::map<std::string, const TreeNode*>> byName_{};
    };
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
    const AddonsByName addons(libraries);

    for (const DestinationEntry& entry : entries)
    {
        if (entry.theOtherProgramTookItsFolderBack)
        {
            found.push_back(CopyConflict{.provenancePath = entry.externalOrigin,
                                         .libraryPath = entry.libraryCopy,
                                         .theProvenanceIsAnotherProgram = true});
            continue;
        }

        if (entry.classification == EntryClassification::Substituted)
        {
            found.push_back(CopyConflict{
                .provenancePath = entry.path, .libraryPath = entry.libraryCopy, .ourLinkWasReplaced = true});
            continue;
        }

        if (entry.classification != EntryClassification::Unmanaged)
        {
            continue;
        }

        if (const TreeNode* addon = addons.Named(entry.path.filename()))
        {
            found.push_back(CopyConflict{.provenancePath = entry.path, .libraryPath = addon->path});
        }
    }

    return CopyConflicts{std::move(found)};
}
