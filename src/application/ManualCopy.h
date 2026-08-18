#ifndef FS_ORGANIZER_APPLICATION_MANUAL_COPY_H
#define FS_ORGANIZER_APPLICATION_MANUAL_COPY_H

#include <filesystem>
#include <string>

[[nodiscard]] std::string ManualLanguageFor(const std::string& interfaceLanguage);

[[nodiscard]] std::string ManualUrlFor(const std::string& version, const std::string& interfaceLanguage);

[[nodiscard]] std::filesystem::path
ManualFileIn(const std::filesystem::path& folder, const std::string& version, const std::string& interfaceLanguage);

[[nodiscard]] bool ItIsAManualCopy(const std::filesystem::path& file);

#endif // FS_ORGANIZER_APPLICATION_MANUAL_COPY_H
