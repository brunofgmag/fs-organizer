#ifndef FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H
#define FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H

#include <cstdint>
#include <filesystem>

struct FileFingerprint
{
    std::filesystem::path relativePath;
    std::uintmax_t size = 0;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_FILE_FINGERPRINT_H
