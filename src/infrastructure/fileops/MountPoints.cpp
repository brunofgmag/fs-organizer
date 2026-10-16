#include "infrastructure/fileops/MountPoints.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <sstream>
#include <string_view>

namespace
{
    constexpr std::array<std::string_view, 2> kRemovableMediaRoots{"/run/media", "/media"};
    constexpr std::size_t kFieldOfTheMountPoint = 4;
    constexpr std::size_t kFieldOfTheFstabMountPoint = 1;
    constexpr std::size_t kFieldOfTheFstabType = 2;
    constexpr std::size_t kDigitsOfAnEscape = 3;
    constexpr int kOctal = 8;

    [[nodiscard]] std::string Unescaped(const std::string& field)
    {
        std::string text;

        for (std::size_t at = 0; at < field.size(); ++at)
        {
            const bool escapes = field[at] == '\\' && at + kDigitsOfAnEscape < field.size()
                && std::ranges::all_of(field.substr(at + 1, kDigitsOfAnEscape),
                                       [](const char digit)
                                       {
                                           return digit >= '0' && digit <= '7';
                                       });

            if (!escapes)
            {
                text.push_back(field[at]);
                continue;
            }

            text.push_back(static_cast<char>(std::stoi(field.substr(at + 1, kDigitsOfAnEscape), nullptr, kOctal)));
            at += kDigitsOfAnEscape;
        }

        return text;
    }

    [[nodiscard]] std::vector<std::string> FieldsOf(const std::string& line)
    {
        std::istringstream words(line);

        return {std::istream_iterator<std::string>(words), std::istream_iterator<std::string>()};
    }

    [[nodiscard]] bool IsAtOrBelow(const std::filesystem::path& path, const std::filesystem::path& mount)
    {
        return std::ranges::mismatch(path, mount).in2 == mount.end();
    }

    [[nodiscard]] std::size_t DepthOf(const std::filesystem::path& path)
    {
        return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
    }

    [[nodiscard]] bool Holds(const std::vector<std::filesystem::path>& mounts, const std::filesystem::path& mount)
    {
        return std::ranges::find(mounts, mount) != mounts.end();
    }

    [[nodiscard]] std::vector<std::filesystem::path>
    SlotsOfRemovableMedia(const std::filesystem::path& path, const std::vector<std::filesystem::path>& mounted)
    {
        std::vector<std::filesystem::path> places;

        for (const std::string_view root : kRemovableMediaRoots)
        {
            const std::filesystem::path media = root;
            if (!IsAtOrBelow(path, media))
            {
                continue;
            }

            auto part = std::next(path.begin(), static_cast<std::ptrdiff_t>(DepthOf(media)));
            if (part == path.end() || std::next(part) == path.end() || std::next(part)->empty())
            {
                continue;
            }

            const std::filesystem::path user = media / *part;
            if (!Holds(mounted, user))
            {
                places.push_back(user / *std::next(part));
            }
        }

        return places;
    }
}

std::vector<std::filesystem::path> MountedPointsIn(const std::string& mountInfo)
{
    std::vector<std::filesystem::path> points;
    std::istringstream lines(mountInfo);

    for (std::string line; std::getline(lines, line);)
    {
        const std::vector<std::string> fields = FieldsOf(line);
        if (fields.size() > kFieldOfTheMountPoint)
        {
            points.emplace_back(Unescaped(fields[kFieldOfTheMountPoint]));
        }
    }

    return points;
}

std::vector<std::filesystem::path> ConfiguredPointsIn(const std::string& fstab)
{
    std::vector<std::filesystem::path> points;
    std::istringstream lines(fstab);

    for (std::string line; std::getline(lines, line);)
    {
        const std::vector<std::string> fields = FieldsOf(line);
        if (fields.size() <= kFieldOfTheFstabType || fields.front().starts_with('#'))
        {
            continue;
        }

        const std::filesystem::path point = Unescaped(fields[kFieldOfTheFstabMountPoint]);
        if (point.is_absolute() && fields[kFieldOfTheFstabType] != "swap")
        {
            points.push_back(point);
        }
    }

    return points;
}

bool TheMountIsMissing(const std::filesystem::path& path,
                       const std::vector<std::filesystem::path>& mounted,
                       const std::vector<std::filesystem::path>& configured)
{
    if (mounted.empty())
    {
        return false;
    }

    const std::filesystem::path wanted = path.lexically_normal();

    std::size_t deepestMounted = 0;
    for (const std::filesystem::path& mount : mounted)
    {
        if (IsAtOrBelow(wanted, mount))
        {
            deepestMounted = std::max(deepestMounted, DepthOf(mount));
        }
    }

    std::vector<std::filesystem::path> expected = SlotsOfRemovableMedia(wanted, mounted);
    expected.insert(expected.end(), configured.begin(), configured.end());

    return std::ranges::any_of(expected,
                               [&wanted, deepestMounted](const std::filesystem::path& mount)
                               {
                                   return IsAtOrBelow(wanted, mount) && DepthOf(mount) > deepestMounted;
                               });
}
