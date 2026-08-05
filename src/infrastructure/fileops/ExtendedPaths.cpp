#include "infrastructure/fileops/ExtendedPaths.h"

#include <string>
#include <string_view>

namespace
{
    constexpr std::wstring_view kExtended = LR"(\\?\)";
    constexpr std::wstring_view kExtendedUnc = LR"(\\?\UNC\)";
    constexpr std::wstring_view kNetwork = LR"(\\)";

    [[nodiscard]] bool StartsWith(const std::wstring& text, const std::wstring_view head)
    {
        return text.size() >= head.size() && text.compare(0, head.size(), head) == 0;
    }

    [[nodiscard]] bool ReachableFromARoot(const std::wstring& text)
    {
        if (StartsWith(text, kNetwork))
        {
            return true;
        }

        return text.size() >= 3 && text[1] == L':' && text[2] == L'\\';
    }
}

std::filesystem::path WithExtendedPrefix(const std::filesystem::path& path)
{
    std::filesystem::path settled = path.lexically_normal();
    settled.make_preferred();

    const std::wstring text = settled.wstring();

    if (StartsWith(text, kExtended) || !ReachableFromARoot(text))
    {
        return settled;
    }

    if (StartsWith(text, kNetwork))
    {
        return std::wstring(kExtendedUnc) + text.substr(kNetwork.size());
    }

    return std::wstring(kExtended) + text;
}

std::filesystem::path WithoutExtendedPrefix(const std::filesystem::path& path)
{
    const std::wstring text = path.wstring();

    if (StartsWith(text, kExtendedUnc))
    {
        return std::wstring(kNetwork) + text.substr(kExtendedUnc.size());
    }

    if (StartsWith(text, kExtended))
    {
        return text.substr(kExtended.size());
    }

    return path;
}
