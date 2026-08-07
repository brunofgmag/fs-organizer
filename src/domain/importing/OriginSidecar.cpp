#include "domain/importing/OriginSidecar.h"

#include <charconv>
#include <cstdint>
#include <sstream>
#include <string_view>

namespace
{
    constexpr auto kVersionKey = "version";
    constexpr auto kOriginKey = "origin";
    constexpr auto kQuarantinedKey = "quarantined";
    constexpr auto kWrittenVersion = "1";

    std::string WithoutTheCarriageReturn(std::string line)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        return line;
    }

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
    std::filesystem::path sidecar = item;
    sidecar += kOriginSidecarSuffix;

    return sidecar;
}

std::string TextOfTheOrigin(const QuarantineOrigin& origin)
{
    std::string text;
    text.append(kVersionKey).append("=").append(kWrittenVersion).append("\n");
    text.append(kOriginKey).append("=").append(AsUtf8(origin.origin)).append("\n");

    if (origin.quarantinedAt.has_value())
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(origin.quarantinedAt->time_since_epoch()).count();

        text.append(kQuarantinedKey).append("=").append(std::to_string(milliseconds)).append("\n");
    }

    return text;
}

std::optional<QuarantineOrigin> OriginFromText(const std::string& text)
{
    std::istringstream lines(text);

    bool versionMatched = false;
    QuarantineOrigin read;

    for (std::string line; std::getline(lines, line);)
    {
        const std::string entry = WithoutTheCarriageReturn(std::move(line));
        const std::size_t separator = entry.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string_view key(entry.data(), separator);
        const std::string value = entry.substr(separator + 1);

        if (key == kVersionKey)
        {
            versionMatched = value == kWrittenVersion;
        }
        else if (key == kOriginKey)
        {
            read.origin = PathFromUtf8(value);
        }
        else if (key == kQuarantinedKey)
        {
            read.quarantinedAt = MomentFromMilliseconds(value);
        }
    }

    if (!versionMatched || read.origin.empty())
    {
        return std::nullopt;
    }

    return read;
}
