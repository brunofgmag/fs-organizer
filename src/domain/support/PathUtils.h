#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

[[nodiscard]] inline std::string ComparablePath(const std::filesystem::path& path)
{
    std::string text = path.generic_string();
    std::ranges::replace(text, '\\', '/');

    std::string key = std::filesystem::path(text).lexically_normal().generic_string();
    std::ranges::transform(key, key.begin(),
                           [](const unsigned char character)
                           {
                               return static_cast<char>(std::tolower(character));
                           });

    while (key.size() > 1 && key.back() == '/' && key[key.size() - 2] != ':')
    {
        key.pop_back();
    }

    return key;
}

[[nodiscard]] inline bool PathIsInside(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const std::string candidate = ComparablePath(path);
    const std::string base = ComparablePath(root);

    if (candidate == base)
    {
        return true;
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

    return std::filesystem::path(text).lexically_normal();
}

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H
