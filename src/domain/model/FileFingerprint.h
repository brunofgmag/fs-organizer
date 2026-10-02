#ifndef FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H
#define FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

struct FileFingerprint
{
    std::filesystem::path relativePath{};
    std::uintmax_t size = 0;
    std::chrono::system_clock::time_point lastWriteTime{};
};

struct TreeFingerprint
{
    std::vector<FileFingerprint> files{};
    std::size_t longestEntry = 0;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H
