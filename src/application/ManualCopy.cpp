#include "application/ManualCopy.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr std::array kLanguagesTheManualIsWrittenIn = {std::string_view("en"), std::string_view("pt_BR")};
    constexpr auto kFallbackLanguage = "en";
    constexpr auto kTagPrefix = "https://raw.githubusercontent.com/brunofgmag/fs-organizer/v";
    constexpr auto kFolderInTheRepository = "/docs/";
    constexpr auto kNamePrefix = "fs-organizer-";
    constexpr auto kNameSuffix = ".pdf";
}

std::string ManualLanguageFor(const std::string& interfaceLanguage)
{
    const bool written = std::ranges::any_of(kLanguagesTheManualIsWrittenIn,
                                             [&interfaceLanguage](const std::string_view offered)
                                             {
                                                 return interfaceLanguage == offered;
                                             });

    return written ? interfaceLanguage : kFallbackLanguage;
}

std::string ManualUrlFor(const std::string& version, const std::string& interfaceLanguage)
{
    return kTagPrefix + version + kFolderInTheRepository + kNamePrefix + ManualLanguageFor(interfaceLanguage)
        + kNameSuffix;
}

std::filesystem::path
ManualFileIn(const std::filesystem::path& folder, const std::string& version, const std::string& interfaceLanguage)
{
    const std::string named = kNamePrefix + ManualLanguageFor(interfaceLanguage) + "-" + version + kNameSuffix;

    return PathUnder(folder, PathFromUtf8(named));
}

bool ItIsAManualCopy(const std::filesystem::path& file)
{
    const std::string named = ComparableFileName(file);

    return named.starts_with(kNamePrefix) && named.ends_with(kNameSuffix);
}
