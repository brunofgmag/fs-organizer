#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_SIDECAR_FILE_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_SIDECAR_FILE_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

[[nodiscard]] std::filesystem::path SidecarBeside(const std::filesystem::path& item, std::string_view suffix);

[[nodiscard]] std::string SidecarHeader();

[[nodiscard]] std::string SidecarLine(std::string_view key, const std::string& value);

[[nodiscard]] std::optional<std::map<std::string, std::string>> FieldsOfTheSidecar(const std::string& text);

[[nodiscard]] std::string FieldNamed(const std::map<std::string, std::string>& fields, std::string_view key);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_SIDECAR_FILE_H
