#ifndef FS_ORGANIZER_TOOLS_SHARED_DISPOSABLE_STATE_H
#define FS_ORGANIZER_TOOLS_SHARED_DISPOSABLE_STATE_H

#include <filesystem>
#include <optional>
#include <string>

#include <QtCore/QDir>

#include "infrastructure/platform/WindowsKnownFolders.h"
#include "support/PathText.h"

struct DisposableState final
{
    std::filesystem::path settingsFile;
    std::filesystem::path journalFile;
    std::filesystem::path presetsFolder;
};

[[nodiscard]] inline std::optional<DisposableState> StageStateWhereWritingIsHarmless(const std::string& toolName)
{
    const std::filesystem::path folder = AsPath(QDir::tempPath()) / (toolName + "-state");

    std::error_code failure;
    std::filesystem::remove_all(folder, failure);

    const DisposableState staged{.settingsFile = folder / SettingsFilePath().filename(),
                                 .journalFile =
                                     folder / JournalFilePath().parent_path().filename() / JournalFilePath().filename(),
                                 .presetsFolder = folder / PresetsFolderPath().filename()};

    std::filesystem::create_directories(staged.journalFile.parent_path(), failure);
    if (failure)
    {
        return std::nullopt;
    }

    if (std::filesystem::exists(SettingsFilePath()))
    {
        std::filesystem::copy_file(SettingsFilePath(), staged.settingsFile,
                                   std::filesystem::copy_options::overwrite_existing, failure);
    }

    if (std::filesystem::exists(JournalFilePath()))
    {
        std::filesystem::copy_file(JournalFilePath(), staged.journalFile,
                                   std::filesystem::copy_options::overwrite_existing, failure);
    }

    if (std::filesystem::exists(PresetsFolderPath()))
    {
        std::filesystem::copy(
            PresetsFolderPath(), staged.presetsFolder,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, failure);
    }

    if (failure)
    {
        return std::nullopt;
    }

    return staged;
}

#endif // FS_ORGANIZER_TOOLS_SHARED_DISPOSABLE_STATE_H
