#ifndef FS_ORGANIZER_APPLICATION_MODEL_CONFLICT_DETAILS_H
#define FS_ORGANIZER_APPLICATION_MODEL_CONFLICT_DETAILS_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/Manifest.h"

struct ConflictSide
{
    std::filesystem::path path;
    Manifest manifest;
    std::uintmax_t sizeBytes = 0;
    std::optional<std::chrono::system_clock::time_point> modified;
};

struct ConflictDetails
{
    ConflictSide destination;
    ConflictSide library;
    std::vector<std::filesystem::path> linksToTheLibraryCopy;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_CONFLICT_DETAILS_H
