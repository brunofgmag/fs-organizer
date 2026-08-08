#include "domain/importing/OriginSidecar.h"

#include <charconv>
#include <cstdint>
#include <map>

#include "domain/importing/SidecarFile.h"

namespace
{
    constexpr auto kOriginKey = "origin";
    constexpr auto kQuarantinedKey = "quarantined";

    std::optional<std::chrono::system_clock::time_point> MomentFromMilliseconds(const std::string& text)
    {
        std::int64_t milliseconds = 0;
        const char* end = text.data() + text.size();

        const std::from_chars_result read = std::from_chars(text.data(), end, milliseconds);
        if (read.ec != std::errc{} || read.ptr != end)
        {
            return std::nullopt;
        }

        return std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
    }
}

std::filesystem::path SidecarPathFor(const std::filesystem::path& item)
{
    return SidecarBeside(item, kOriginSidecarSuffix);
}

std::string TextOfTheOrigin(const QuarantineOrigin& origin)
{
    std::string text = SidecarHeader() + SidecarLine(kOriginKey, AsUtf8(origin.origin));

    if (origin.quarantinedAt.has_value())
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(origin.quarantinedAt->time_since_epoch()).count();

        text += SidecarLine(kQuarantinedKey, std::to_string(milliseconds));
    }

    return text;
}

std::optional<QuarantineOrigin> OriginFromText(const std::string& text)
{
    const std::optional<std::map<std::string, std::string>> fields = FieldsOfTheSidecar(text);
    if (!fields.has_value())
    {
        return std::nullopt;
    }

    const std::string origin = FieldNamed(*fields, kOriginKey);
    if (origin.empty())
    {
        return std::nullopt;
    }

    return QuarantineOrigin{.origin = PathFromUtf8(origin),
                            .quarantinedAt = MomentFromMilliseconds(FieldNamed(*fields, kQuarantinedKey))};
}
