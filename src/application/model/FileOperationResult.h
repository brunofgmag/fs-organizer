#ifndef FS_ORGANIZER_APPLICATION_MODEL_FILE_OPERATION_RESULT_H
#define FS_ORGANIZER_APPLICATION_MODEL_FILE_OPERATION_RESULT_H

#include <filesystem>

#include "domain/model/FileResult.h"
#include "domain/model/WriteAccess.h"

struct FileOperationResult
{
    std::filesystem::path path{};
    FileResult result = FileResult::Completed;
    std::filesystem::path occupant{};
    WriteAccess writeAccess = WriteAccess::ItAccepts;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_FILE_OPERATION_RESULT_H
