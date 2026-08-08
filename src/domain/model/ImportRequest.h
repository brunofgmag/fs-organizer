#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H

#include <filesystem>

#include "domain/model/LandingPath.h"

struct ImportRequest
{
    std::filesystem::path source{};
    std::filesystem::path category{};
    std::filesystem::path externalSource{};

    [[nodiscard]] bool CameFromAnotherProgram() const
    {
        return !externalSource.empty();
    }

    [[nodiscard]] const std::filesystem::path& Bytes() const
    {
        return CameFromAnotherProgram() ? externalSource : source;
    }

    [[nodiscard]] std::filesystem::path Target() const
    {
        return LandingPathIn(category, source);
    }
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H
