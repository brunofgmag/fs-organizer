#ifndef FS_ORGANIZER_SUPPORT_FILE_WRITING_H
#define FS_ORGANIZER_SUPPORT_FILE_WRITING_H

#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

[[nodiscard]] inline bool WriteFileReplacing(const std::filesystem::path& file, const std::string_view bytes)
{
    std::filesystem::path beside = file;
    beside += ".fsorg-writing";

    {
        std::ofstream stream(beside, std::ios::binary | std::ios::trunc);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();

        if (!stream.good())
        {
            std::error_code discarded;
            std::filesystem::remove(beside, discarded);

            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(beside, file, error);

    if (error)
    {
        std::error_code discarded;
        std::filesystem::remove(beside, discarded);

        return false;
    }

    return true;
}

#endif // FS_ORGANIZER_SUPPORT_FILE_WRITING_H
