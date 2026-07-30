#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_PATH_SEGMENT_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_PATH_SEGMENT_H

#include <optional>
#include <string>
#include <string_view>
#include <utility>

class PathSegment
{
public:
    [[nodiscard]] static std::optional<PathSegment> From(std::string text)
    {
        constexpr std::string_view forbidden = R"(<>:"/\|?*)";

        if (text.empty() || text == "." || text == "..")
        {
            return std::nullopt;
        }

        if (text.back() == ' ' || text.back() == '.')
        {
            return std::nullopt;
        }

        for (const char character : text)
        {
            if (forbidden.find(character) != std::string_view::npos || static_cast<unsigned char>(character) < ' ')
            {
                return std::nullopt;
            }
        }

        return PathSegment{std::move(text)};
    }

    [[nodiscard]] const std::string& Text() const
    {
        return text_;
    }

private:
    explicit PathSegment(std::string text) : text_(std::move(text))
    {
    }

    std::string text_;
};

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_PATH_SEGMENT_H
