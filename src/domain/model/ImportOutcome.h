#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H

#include "domain/model/FileResult.h"
#include "domain/model/WriteAccess.h"

class ImportOutcome
{
public:
    [[nodiscard]] static ImportOutcome Completed()
    {
        return ImportOutcome{FileResult::Completed, WriteAccess::ItAccepts};
    }

    [[nodiscard]] static ImportOutcome Stopped(const FileResult result)
    {
        return ImportOutcome{result, WriteAccess::ItAccepts};
    }

    [[nodiscard]] static ImportOutcome Stopped(const FileResult result, const WriteAccess access)
    {
        return ImportOutcome{result, access};
    }

    [[nodiscard]] bool Succeeded() const
    {
        return ::Succeeded(result_);
    }

    [[nodiscard]] FileResult Result() const
    {
        return result_;
    }

    [[nodiscard]] WriteAccess Access() const
    {
        return access_;
    }

private:
    ImportOutcome(const FileResult result, const WriteAccess access) : result_(result), access_(access)
    {
    }

    FileResult result_;
    WriteAccess access_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
