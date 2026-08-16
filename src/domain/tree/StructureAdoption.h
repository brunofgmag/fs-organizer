#ifndef FS_ORGANIZER_DOMAIN_TREE_STRUCTURE_ADOPTION_H
#define FS_ORGANIZER_DOMAIN_TREE_STRUCTURE_ADOPTION_H

#include <filesystem>
#include <vector>

#include "domain/ports/FilesystemProbe.h"

struct LibraryGrouping
{
    std::vector<std::filesystem::path> notYetDeclared{};
    std::vector<std::filesystem::path> alreadyDeclared{};
};

[[nodiscard]] LibraryGrouping HowTheLibraryIsGrouped(const FilesystemProbe& filesystemProbe,
                                                     const std::filesystem::path& libraryRoot,
                                                     const std::vector<std::filesystem::path>& theAppBroughtIn);

#endif // FS_ORGANIZER_DOMAIN_TREE_STRUCTURE_ADOPTION_H
