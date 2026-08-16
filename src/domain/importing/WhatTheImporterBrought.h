#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H

#include <filesystem>
#include <vector>

#include "domain/model/OperationRecord.h"

[[nodiscard]] std::vector<std::filesystem::path> FoldersTheImporterBrought(const std::vector<OperationRecord>& history);

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_THE_IMPORTER_BROUGHT_H
