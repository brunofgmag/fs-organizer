#ifndef FS_ORGANIZER_DOMAIN_PORTS_IMPORTED_FOLDERS_H
#define FS_ORGANIZER_DOMAIN_PORTS_IMPORTED_FOLDERS_H

#include <filesystem>
#include <vector>

class ImportedFolders
{
public:
    virtual ~ImportedFolders() = default;

    [[nodiscard]] virtual std::vector<std::filesystem::path> WhatTheImporterBrought() const = 0;
};

class NothingWasImported final : public ImportedFolders
{
public:
    [[nodiscard]] std::vector<std::filesystem::path> WhatTheImporterBrought() const override
    {
        return {};
    }
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_IMPORTED_FOLDERS_H
