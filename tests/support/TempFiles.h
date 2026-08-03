#ifndef FS_ORGANIZER_TESTS_SUPPORT_TEMP_FILES_H
#define FS_ORGANIZER_TESTS_SUPPORT_TEMP_FILES_H

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <QtCore/QTemporaryDir>

struct TempFiles
{
    QTemporaryDir directory;

    [[nodiscard]] std::filesystem::path Root() const
    {
        return std::filesystem::path(directory.path().toStdString());
    }

    [[nodiscard]] std::filesystem::path Write(const std::string& name, const std::vector<unsigned char>& bytes) const
    {
        const std::filesystem::path file = Root() / name;
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

        return file;
    }

    [[nodiscard]] std::filesystem::path WriteText(const std::string& name, const std::string& text) const
    {
        return Write(name, std::vector<unsigned char>(text.begin(), text.end()));
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_TEMP_FILES_H
