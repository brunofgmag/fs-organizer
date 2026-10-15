#ifndef FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
#define FS_ORGANIZER_SUPPORT_FILE_CLOCK_H

#include <chrono>
#include <filesystem>

[[nodiscard]] inline std::chrono::system_clock::time_point SystemTimeOf(const std::filesystem::file_time_type written)
{
    return std::chrono::clock_cast<std::chrono::system_clock>(written);
}

#endif // FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
