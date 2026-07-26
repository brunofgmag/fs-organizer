#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H

#include <filesystem>

struct ImportRequest
{
    std::filesystem::path source;
    std::filesystem::path category;

    [[nodiscard]] std::filesystem::path Target() const
    {
        return category / source.filename();
    }
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_REQUEST_H
