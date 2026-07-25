#ifndef FS_ORGANIZER_DOMAIN_MODEL_COPY_OUTCOME_H
#define FS_ORGANIZER_DOMAIN_MODEL_COPY_OUTCOME_H

#include <cstdint>

enum class CopyOutcome
{
    Completed,
    Cancelled,
    Failed
};

struct CopyProgress
{
    std::uintmax_t copiedBytes = 0;
    std::uintmax_t totalBytes = 0;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_COPY_OUTCOME_H
