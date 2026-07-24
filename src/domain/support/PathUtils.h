#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_PATH_UTILS_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

[[nodiscard]] inline std::string ComparablePath(const std::filesystem::path& path)
{
    std::string key = path.lexically_normal().generic_string();
    std::ranges::transform(key, key.begin(), [](const unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });

    return key;
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
