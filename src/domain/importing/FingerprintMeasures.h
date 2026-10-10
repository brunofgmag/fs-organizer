#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_FINGERPRINT_MEASURES_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_FINGERPRINT_MEASURES_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "domain/model/FileFingerprint.h"
#include "domain/model/FileResult.h"
#include "domain/ports/FilesystemProbe.h"

[[nodiscard]] std::uintmax_t TotalSizeOf(const std::vector<FileFingerprint>& files);

[[nodiscard]] bool FingerprintsMatch(std::vector<FileFingerprint> left, std::vector<FileFingerprint> right);

[[nodiscard]] FileResult
RoomFor(const FilesystemProbe& probe, const std::filesystem::path& folder, std::uintmax_t bytes, std::size_t files);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_FINGERPRINT_MEASURES_H
