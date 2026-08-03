#ifndef FS_ORGANIZER_VIEWMODEL_ATTENTION_BREAKDOWN_H
#define FS_ORGANIZER_VIEWMODEL_ATTENTION_BREAKDOWN_H

#include <cstddef>

struct AttentionBreakdown
{
    std::size_t broken = 0;
    std::size_t conflicts = 0;
    std::size_t duplicated = 0;
    std::size_t unmanaged = 0;

    [[nodiscard]] bool operator==(const AttentionBreakdown&) const = default;
};

#endif // FS_ORGANIZER_VIEWMODEL_ATTENTION_BREAKDOWN_H
