#ifndef FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H
#define FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H

#include "domain/model/ImportRequest.h"
#include "domain/model/ImportResult.h"

struct ImportOperationResult
{
    ImportRequest request;
    ImportResult result = ImportResult::Completed;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_IMPORT_OPERATION_RESULT_H
