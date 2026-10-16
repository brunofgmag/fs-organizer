#ifndef FS_ORGANIZER_TESTS_SUPPORT_PROTON_TREE_H
#define FS_ORGANIZER_TESTS_SUPPORT_PROTON_TREE_H

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <QtCore/QTemporaryDir>

struct ProtonTree
{
    static constexpr std::string_view kMsfs2020 = "1250410";
    static constexpr std::string_view kMsfs2024 = "2537590";

    QTemporaryDir directory;

    [[nodiscard]] std::filesystem::path Root() const
    {
        return directory.path().toStdString();
    }

    [[nodiscard]] std::filesystem::path Home() const
    {
        return Root() / "home";
    }

    [[nodiscard]] std::filesystem::path SteamRoot() const
    {
        return Home() / ".local" / "share" / "Steam";
    }

    [[nodiscard]] std::filesystem::path WriteText(const std::filesystem::path& file, const std::string& text) const
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary);
        stream << text;

        return file;
    }

    [[nodiscard]] std::filesystem::path AddPrefix(const std::filesystem::path& library,
                                                  const std::string_view appId) const
    {
        const std::filesystem::path steamApps = library / "steamapps";
        const std::filesystem::path prefix = steamApps / "compatdata" / appId / "pfx";

        static_cast<void>(WriteText(steamApps / (std::string("appmanifest_").append(appId).append(".acf")),
                                    "\"AppState\"\n{\n\t\"appid\"\t\t\"" + std::string(appId) + "\"\n}\n"));
        std::filesystem::create_directories(prefix / "drive_c");
        std::filesystem::create_directories(prefix / "dosdevices");
        std::filesystem::create_directory_symlink("../drive_c", prefix / "dosdevices" / "c:");
        std::filesystem::create_directory_symlink("/", prefix / "dosdevices" / "z:");

        return prefix;
    }

    void Drive(const std::filesystem::path& prefix, const char letter, const std::filesystem::path& target) const
    {
        std::filesystem::create_directory_symlink(target, prefix / "dosdevices" / (std::string(1, letter) + ":"));
    }

    void WriteLibraryFolders(const std::filesystem::path& steamRoot,
                             const std::vector<std::filesystem::path>& libraries) const
    {
        std::string text = "\"libraryfolders\"\n{\n";

        for (std::size_t index = 0; index < libraries.size(); ++index)
        {
            text += "\t\"" + std::to_string(index) + "\"\n\t{\n\t\t\"path\"\t\t\"" + libraries[index].string()
                + "\"\n\t\t\"label\"\t\t\"\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"228980\"\t\t\"123\"\n\t\t}\n\t}\n";
        }

        static_cast<void>(WriteText(steamRoot / "steamapps" / "libraryfolders.vdf", text + "}\n"));
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_PROTON_TREE_H
