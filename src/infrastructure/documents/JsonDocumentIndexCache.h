#ifndef FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_JSON_DOCUMENT_INDEX_CACHE_H
#define FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_JSON_DOCUMENT_INDEX_CACHE_H

#include <filesystem>
#include <optional>

#include "application/ports/DocumentIndexCache.h"

class JsonDocumentIndexCache final : public DocumentIndexCache
{
public:
    explicit JsonDocumentIndexCache(std::filesystem::path filePath);

    [[nodiscard]] std::optional<RememberedDocuments> Remember() const override;

    void Keep(const RememberedDocuments& index) override;

private:
    std::filesystem::path filePath_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_JSON_DOCUMENT_INDEX_CACHE_H
