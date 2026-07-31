#ifndef FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
#define FS_ORGANIZER_SUPPORT_FILE_CLOCK_H

#include <chrono>
#include <filesystem>

[[nodiscard]] inline std::chrono::system_clock::time_point SystemTimeOf(const std::filesystem::file_time_type written)
{
    return std::chrono::system_clock::now()
        + std::chrono::duration_cast<std::chrono::system_clock::duration>(
               written - std::filesystem::file_time_type::clock::now());
}

#endif // FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
