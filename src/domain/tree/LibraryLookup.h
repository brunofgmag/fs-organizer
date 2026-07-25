#ifndef FS_ORGANIZER_DOMAIN_TREE_LIBRARY_LOOKUP_H
#define FS_ORGANIZER_DOMAIN_TREE_LIBRARY_LOOKUP_H

#include <filesystem>
#include <vector>

#include "domain/model/Library.h"
#include "domain/model/SimulatorProfile.h"

[[nodiscard]] const Library* LibraryContaining(const std::vector<Library>& libraries,
                                               const std::filesystem::path& path);

[[nodiscard]] const Library* LibraryContaining(const SimulatorProfile& profile,
                                               const std::filesystem::path& path);

[[nodiscard]] std::filesystem::path RelativeToLibrary(const Library& library,
                                                      const std::filesystem::path& path);

#endif // FS_ORGANIZER_DOMAIN_TREE_LIBRARY_LOOKUP_H
