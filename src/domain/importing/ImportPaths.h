#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_PATHS_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_PATHS_H

#include <filesystem>
#include <string>

#include "domain/support/PathUtils.h"

inline constexpr auto kStagingSuffix = ".fsorg-partial";
inline constexpr auto kSwapSlotSuffix = ".fsorg-swap";
inline constexpr auto kQuarantineFolderName = "_fsorganizer-quarantine";

[[nodiscard]] inline std::filesystem::path SwapSlotFor(const std::filesystem::path& item)
{
    std::filesystem::path room = item;
    room += kSwapSlotSuffix;

    return room;
}

[[nodiscard]] inline std::filesystem::path StagingPathFor(const std::filesystem::path& target)
{
    std::filesystem::path staging = target;
    staging += kStagingSuffix;

    return staging;
}

[[nodiscard]] inline bool IsStagingPath(const std::filesystem::path& path)
{
    return ComparablePath(path.filename()).ends_with(kStagingSuffix);
}

[[nodiscard]] inline std::filesystem::path ImportedPathFor(const std::filesystem::path& staging)
{
    const std::string name = AsUtf8(staging.filename());

    return IsStagingPath(staging)
        ? staging.parent_path() / PathFromUtf8(name.substr(0, name.size() - std::string_view(kStagingSuffix).size()))
        : staging;
}

[[nodiscard]] inline bool IsQuarantineFolder(const std::filesystem::path& path)
{
    return ComparablePath(path.filename()) == kQuarantineFolderName;
}

[[nodiscard]] inline bool CreatedByTheImporter(const std::filesystem::path& path)
{
    return IsStagingPath(path) || IsQuarantineFolder(path);
}

[[nodiscard]] inline std::filesystem::path QuarantineFolderBeside(const std::filesystem::path& destination)
{
    return destination.parent_path() / kQuarantineFolderName;
}

[[nodiscard]] inline std::filesystem::path QuarantineFolderInside(const std::filesystem::path& libraryRoot)
{
    return libraryRoot / kQuarantineFolderName;
}

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_PATHS_H
