#ifndef FS_ORGANIZER_APPLICATION_MODEL_ADDON_DOCUMENTS_H
#define FS_ORGANIZER_APPLICATION_MODEL_ADDON_DOCUMENTS_H

#include <filesystem>
#include <vector>

#include "domain/documents/ChartIndex.h"
#include "domain/model/AddonId.h"

struct DocumentsOfAnAddon
{
    AddonId addon{};
    std::filesystem::path folder{};
    bool itWasWalked = true;
    std::vector<std::filesystem::path> documents{};
    std::vector<ChartsOfAnAirport> airports{};
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_ADDON_DOCUMENTS_H
