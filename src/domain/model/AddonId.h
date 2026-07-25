#ifndef FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H
#define FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H

#include <algorithm>
#include <cctype>
#include <string>

#include "domain/model/LibraryId.h"

struct AddonId
{
    LibraryId libraryId;
    std::string folderName;
};

[[nodiscard]] inline bool EqualsIgnoringCase(const std::string& left, const std::string& right)
{
    return std::ranges::equal(left, right, [](const unsigned char one, const unsigned char other)
    {
        return std::tolower(one) == std::tolower(other);
    });
}

[[nodiscard]] inline bool operator==(const AddonId& left, const AddonId& right)
{
    return EqualsIgnoringCase(left.libraryId, right.libraryId)
        && EqualsIgnoringCase(left.folderName, right.folderName);
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_ADDON_ID_H
