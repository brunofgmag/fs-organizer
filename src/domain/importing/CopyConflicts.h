#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_COPY_CONFLICTS_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_COPY_CONFLICTS_H

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/model/CopyConflict.h"
#include "domain/model/DestinationEntry.h"
#include "domain/model/TreeNode.h"

class CopyConflicts
{
public:
    CopyConflicts() = default;

    explicit CopyConflicts(std::vector<CopyConflict> found);

    [[nodiscard]] const CopyConflict* OverTheProvenance(const std::filesystem::path& provenance) const;

    [[nodiscard]] const CopyConflict* OverTheLibraryAddon(const std::filesystem::path& addonFolder) const;

    [[nodiscard]] const std::vector<CopyConflict>& All() const;

    [[nodiscard]] std::size_t Count() const;

private:
    std::vector<CopyConflict> found_;
    std::map<std::string, std::size_t> byProvenance_;
    std::map<std::string, std::size_t> byLibrary_;
};

[[nodiscard]] CopyConflicts FindCopyConflicts(const std::vector<DestinationEntry>& entries,
                                              const std::vector<TreeNode>& libraries);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_COPY_CONFLICTS_H
