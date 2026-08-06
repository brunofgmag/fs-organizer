#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H

#include <algorithm>
#include <filesystem>
#include <string>

#include "domain/support/CaseFolding.h"

[[nodiscard]] inline std::string WithGenericSeparators(std::string text)
{
    std::ranges::replace(text, '\\', '/');

    return text;
}

[[nodiscard]] inline std::string ComparablePath(const std::filesystem::path& path)
{
    std::string key = LoweredForComparison(
        std::filesystem::path(WithGenericSeparators(path.generic_string())).lexically_normal().generic_string());

    while (key.size() > 1 && key.back() == '/' && key[key.size() - 2] != ':')
    {
        key.pop_back();
    }

    return key;
}

[[nodiscard]] inline std::string ComparableFileName(const std::filesystem::path& path)
{
    const std::string key = ComparablePath(path);
    const std::size_t separator = key.find_last_of('/');

    return separator == std::string::npos ? key : key.substr(separator + 1);
}

[[nodiscard]] inline bool PathIsInside(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const std::string candidate = ComparablePath(path);
    const std::string base = ComparablePath(root);

    if (candidate == base)
    {
        return true;
    }

    if (base.empty())
    {
        return false;
    }

    if (candidate.size() <= base.size() || candidate.compare(0, base.size(), base) != 0)
    {
        return false;
    }

    return base.back() == '/' || candidate[base.size()] == '/';
}

[[nodiscard]] inline std::filesystem::path NormalizeReparseTarget(const std::filesystem::path& target)
{
    std::string text = target.string();

    for (const std::string_view prefix : {R"(\??\)", R"(\\?\)"})
    {
        if (text.compare(0, prefix.size(), prefix) == 0)
        {
            text.erase(0, prefix.size());
            break;
        }
    }

    for (const std::string_view unc : {"UNC\\", "unc\\"})
    {
        if (text.compare(0, unc.size(), unc) == 0)
        {
            text.replace(0, unc.size(), R"(\\)");
            break;
        }
    }

    return std::filesystem::path(WithGenericSeparators(text)).lexically_normal();
}

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H
