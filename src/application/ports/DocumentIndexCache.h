#ifndef FS_ORGANIZER_APPLICATION_PORTS_DOCUMENT_INDEX_CACHE_H
#define FS_ORGANIZER_APPLICATION_PORTS_DOCUMENT_INDEX_CACHE_H

#include <chrono>
#include <optional>
#include <vector>

#include "application/model/AddonDocuments.h"

struct RememberedDocuments
{
    std::chrono::system_clock::time_point readAt{};
    std::vector<DocumentsOfAnAddon> addons{};
};

class DocumentIndexCache
{
public:
    virtual ~DocumentIndexCache() = default;

    [[nodiscard]] virtual std::optional<RememberedDocuments> Remember() const = 0;

    virtual void Keep(const RememberedDocuments& index) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_DOCUMENT_INDEX_CACHE_H
