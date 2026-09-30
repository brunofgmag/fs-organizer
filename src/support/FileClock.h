#ifndef FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
#define FS_ORGANIZER_SUPPORT_FILE_CLOCK_H

#include <chrono>
#include <filesystem>

[[nodiscard]] inline std::chrono::system_clock::time_point SystemTimeOf(const std::filesystem::file_time_type written)
{
    constexpr std::chrono::seconds kFileTimeEpochBeforeTheUnixEpoch{11'644'473'600};

    return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(
        written.time_since_epoch() - kFileTimeEpochBeforeTheUnixEpoch));
}

#endif // FS_ORGANIZER_SUPPORT_FILE_CLOCK_H
