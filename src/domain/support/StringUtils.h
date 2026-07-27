#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H

#include <algorithm>
#include <cctype>
#include <string>

[[nodiscard]] inline bool EqualsIgnoringCase(const std::string& left, const std::string& right)
{
    return std::ranges::equal(left, right,
                              [](const unsigned char one, const unsigned char other)
                              {
                                  return std::tolower(one) == std::tolower(other);
                              });
}

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H
