#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/model/OperationRecord.h"

class FoldersTheImporterBroughtSoFar
{
public:
    void Fold(const OperationRecord& record);

    [[nodiscard]] std::vector<std::filesystem::path> Folders() const;

private:
    std::map<std::string, std::filesystem::path> brought_{};
};

[[nodiscard]] std::vector<std::filesystem::path> FoldersTheImporterBrought(const std::vector<OperationRecord>& history);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H
