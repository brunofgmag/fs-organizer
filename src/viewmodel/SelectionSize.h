#ifndef FS_ORGANIZER_VIEWMODEL_SELECTION_SIZE_H
#define FS_ORGANIZER_VIEWMODEL_SELECTION_SIZE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <QtCore/QMetaType>

#include "domain/model/DestinationEntry.h"

struct UnmeasuredEntries
{
    EntryClassification classification = EntryClassification::Unavailable;
    std::size_t count = 0;
};

struct SelectionSize
{
    std::uintmax_t bytes = 0;
    std::size_t measured = 0;
    std::size_t selected = 0;
    std::vector<UnmeasuredEntries> unmeasured{};
};

Q_DECLARE_METATYPE(SelectionSize)

#endif // FS_ORGANIZER_VIEWMODEL_SELECTION_SIZE_H
