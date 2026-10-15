#include "infrastructure/platform/LinuxKnownFolders.h"

#include <QtCore/QStandardPaths>

#include "support/PathText.h"

namespace
{
    constexpr auto kApplicationFolderName = "fs-organizer";
    constexpr auto kSettingsFileName = "settings.json";
    constexpr auto kJournalFolderName = "journal";
    constexpr auto kJournalFileName = "operations.jsonl";
    constexpr auto kPresetsFolderName = "presets";
    constexpr auto kRemovedStartupEntriesFolderName = "removed-startup-entries";
    constexpr auto kBisectionFolderName = "bisection";
    constexpr auto kDocumentIndexFileName = "document-index.json";
    constexpr auto kSceneryCacheFileName = "scenery-cache.json";
    constexpr auto kManualFolderName = "manual";

    std::filesystem::path Writable(const QStandardPaths::StandardLocation location)
    {
        return AsPath(QStandardPaths::writableLocation(location));
    }
}

std::filesystem::path HomeFolder()
{
    return Writable(QStandardPaths::HomeLocation);
}

std::filesystem::path ConfigHomeFolder()
{
    return Writable(QStandardPaths::GenericConfigLocation);
}

std::filesystem::path DataHomeFolder()
{
    return Writable(QStandardPaths::GenericDataLocation);
}

std::filesystem::path CacheHomeFolder()
{
    return Writable(QStandardPaths::GenericCacheLocation);
}

AppFolders LinuxAppFolders()
{
    const std::filesystem::path config = ConfigHomeFolder() / kApplicationFolderName;
    const std::filesystem::path data = DataHomeFolder() / kApplicationFolderName;
    const std::filesystem::path cache = CacheHomeFolder() / kApplicationFolderName;

    return AppFolders{.settingsFile = config / kSettingsFileName,
                      .journalFile = data / kJournalFolderName / kJournalFileName,
                      .presetsFolder = data / kPresetsFolderName,
                      .removedStartupEntriesFolder = data / kRemovedStartupEntriesFolderName,
                      .bisectionFolder = data / kBisectionFolderName,
                      .sceneryCacheFile = cache / kSceneryCacheFileName,
                      .documentIndexFile = cache / kDocumentIndexFileName,
                      .manualFolder = cache / kManualFolderName};
}
