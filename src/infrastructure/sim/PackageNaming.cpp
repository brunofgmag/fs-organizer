#include "infrastructure/sim/PackageNaming.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace
{
    constexpr std::string_view kShippedPrefixes[] = {"fs24-", "fs20-"};
    constexpr std::string_view kGenerationPrefixes[] = {"communityfs24-", "communityfs20-", "fs24-", "fs20-"};
    constexpr std::string_view kAirportMarker = "-airport-";
    constexpr std::string_view kAirportSuffix = "-airport";
    constexpr std::size_t kShortestCode = 3;
    constexpr std::size_t kLongestCode = 4;

    [[nodiscard]] bool ItIsAnIdentifierCharacter(const char character)
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0;
    }

    [[nodiscard]] std::string Uppercased(std::string_view text)
    {
        std::string code;
        code.reserve(text.size());

        std::ranges::transform(text, std::back_inserter(code),
                               [](const char character)
                               {
                                   return static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
                               });

        return code;
    }
}

bool ItIsContentTheSimulatorShips(const std::string_view packageName)
{
    return std::ranges::any_of(kShippedPrefixes,
                               [packageName](const std::string_view prefix)
                               {
                                   return packageName.starts_with(prefix);
                               });
}

std::string WithoutTheGenerationPrefix(const std::string_view packageName)
{
    for (const std::string_view prefix : kGenerationPrefixes)
    {
        if (packageName.size() > prefix.size() && packageName.starts_with(prefix))
        {
            return std::string(packageName.substr(prefix.size()));
        }
    }

    return std::string(packageName);
}

namespace
{
    [[nodiscard]] std::string CodeAt(const std::string_view name, const std::size_t from)
    {
        std::size_t to = from;

        while (to < name.size() && ItIsAnIdentifierCharacter(name[to]))
        {
            ++to;
        }

        const std::size_t wide = to - from;
        if (wide < kShortestCode || wide > kLongestCode)
        {
            return {};
        }

        return Uppercased(name.substr(from, wide));
    }

    [[nodiscard]] std::string CodeAfterTheMarker(const std::string_view packageName)
    {
        const std::size_t marker = packageName.find(kAirportMarker);
        if (marker == std::string_view::npos)
        {
            return {};
        }

        return CodeAt(packageName, marker + kAirportMarker.size());
    }

    [[nodiscard]] std::string CodeAfterTheVendor(const std::string_view packageName)
    {
        if (!packageName.ends_with(kAirportSuffix))
        {
            return {};
        }

        const std::string named = WithoutTheGenerationPrefix(packageName);
        const std::size_t vendorEnds = named.find('-');
        if (vendorEnds == std::string::npos)
        {
            return {};
        }

        std::string code = CodeAt(named, vendorEnds + 1);
        return code.size() == kLongestCode ? code : std::string{};
    }
}

std::string AirportCodeInAPackageName(const std::string_view packageName)
{
    std::string code = CodeAfterTheMarker(packageName);
    return code.empty() ? CodeAfterTheVendor(packageName) : code;
}
