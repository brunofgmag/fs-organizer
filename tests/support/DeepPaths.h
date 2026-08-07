#ifndef FS_ORGANIZER_TESTS_SUPPORT_DEEP_PATHS_H
#define FS_ORGANIZER_TESTS_SUPPORT_DEEP_PATHS_H

#include <filesystem>
#include <fstream>
#include <string>

inline constexpr std::size_t kOldPathCeiling = 260;

[[nodiscard]] inline std::filesystem::path BeyondTheCeiling(const std::filesystem::path& path)
{
    std::filesystem::path native = path;
    native.make_preferred();

    return LR"(\\?\)" + native.wstring();
}

[[nodiscard]] inline std::filesystem::path FolderPastTheCeiling(const std::filesystem::path& root,
                                                                const std::string& leafName)
{
    std::filesystem::path folder = root;
    const std::string segment(60, 'x');

    while (folder.wstring().size() + leafName.size() + 1 <= kOldPathCeiling + 20)
    {
        folder /= segment;
    }

    folder /= leafName;
    std::filesystem::create_directories(BeyondTheCeiling(folder));

    return folder;
}

inline void WriteFilePastTheCeiling(const std::filesystem::path& file, const std::string& content)
{
    std::filesystem::create_directories(BeyondTheCeiling(file.parent_path()));

    std::ofstream stream(BeyondTheCeiling(file), std::ios::binary);
    stream << content;
}

[[nodiscard]] inline bool ExistsPastTheCeiling(const std::filesystem::path& path)
{
    std::error_code error;

    return std::filesystem::exists(BeyondTheCeiling(path), error) && !error;
}

inline void RemovePastTheCeiling(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(BeyondTheCeiling(path), error);
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_DEEP_PATHS_H
