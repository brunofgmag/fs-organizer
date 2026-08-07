#ifndef FS_ORGANIZER_APPLICATION_MODEL_LINK_BATCH_REPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LINK_BATCH_REPORT_H

#include <cstddef>
#include <vector>

#include "application/model/LinkOperationResult.h"

struct LinkBatchReport
{
    std::vector<LinkOperationResult> results;
    std::size_t drifted = 0;
    std::size_t leftAlone = 0;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LINK_BATCH_REPORT_H
