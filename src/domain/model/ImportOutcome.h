#ifndef FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H

#include "domain/model/ImportResult.h"

class ImportOutcome
{
public:
    [[nodiscard]] static ImportOutcome Completed()
    {
        return ImportOutcome{ImportResult::Completed};
    }

    [[nodiscard]] static ImportOutcome Stopped(const ImportResult result)
    {
        return ImportOutcome{result};
    }

    [[nodiscard]] bool Succeeded() const
    {
        return result_ == ImportResult::Completed;
    }

    [[nodiscard]] ImportResult Result() const
    {
        return result_;
    }

private:
    explicit ImportOutcome(const ImportResult result) : result_(result)
    {
    }

    ImportResult result_;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_IMPORT_OUTCOME_H
