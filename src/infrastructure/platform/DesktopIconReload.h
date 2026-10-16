#ifndef FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_DESKTOP_ICON_RELOAD_H
#define FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_DESKTOP_ICON_RELOAD_H

#include <filesystem>

#include <QtCore/QString>

void AskTheDesktopToReloadItsIcons();

[[nodiscard]] QString IconThemeCacheTool();

void RefreshTheIconThemeCacheIn(const std::filesystem::path& theme);

#endif // FS_ORGANIZER_INFRASTRUCTURE_PLATFORM_DESKTOP_ICON_RELOAD_H
