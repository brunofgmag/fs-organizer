#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H

#include "domain/model/FileResult.h"

class ImportOutcome
{
public:
    [[nodiscard]] static ImportOutcome Completed()
    {
        return ImportOutcome{FileResult::Completed};
    }

    [[nodiscard]] static ImportOutcome Stopped(const FileResult result)
    {
        return ImportOutcome{result};
    }

    [[nodiscard]] bool Succeeded() const
    {
        return result_ == FileResult::Completed;
    }

    [[nodiscard]] FileResult Result() const
    {
        return result_;
    }

private:
    explicit ImportOutcome(const FileResult result) : result_(result)
    {
    }

    FileResult result_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
