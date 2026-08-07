#ifndef FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H
#define FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H

#include <string>

#include "domain/model/LibraryId.h"
#include "domain/support/StringUtils.h"

struct AddonId
{
    LibraryId libraryId{};
    std::string folderName{};
};

[[nodiscard]] inline bool operator==(const AddonId& left, const AddonId& right)
{
    return EqualsIgnoringCase(left.libraryId, right.libraryId) && EqualsIgnoringCase(left.folderName, right.folderName);
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H
