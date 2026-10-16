#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_IMAGE_DESKTOP_ENTRY_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_IMAGE_DESKTOP_ENTRY_H

#include <filesystem>
#include <string>
#include <string_view>

[[nodiscard]] std::string DesktopEntryLaunching(std::string_view shippedEntry, const std::filesystem::path& appImage);

[[nodiscard]] std::filesystem::path DesktopEntryUnder(const std::filesystem::path& dataHome);

[[nodiscard]] std::filesystem::path DesktopIconUnder(const std::filesystem::path& dataHome);

[[nodiscard]] std::filesystem::path IconThemeUnder(const std::filesystem::path& dataHome);

struct DesktopEntryInstall
{
    bool held{};
    bool iconWritten{};
};

[[nodiscard]] DesktopEntryInstall InstallTheDesktopEntry(const std::filesystem::path& appImage,
                                                         const std::filesystem::path& dataHome,
                                                         std::string_view shippedEntry,
                                                         std::string_view icon);

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_APP_IMAGE_DESKTOP_ENTRY_H
