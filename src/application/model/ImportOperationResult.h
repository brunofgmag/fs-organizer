#ifndef FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H
#define FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H

#include <filesystem>

#include "domain/model/ImportRequest.h"
#include "domain/model/FileResult.h"

struct ImportOperationResult
{
    ImportRequest request;
    FileResult result = FileResult::Completed;
    std::filesystem::path occupant{};
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H
