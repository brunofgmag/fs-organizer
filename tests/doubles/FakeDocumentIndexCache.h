#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_DOCUMENT_INDEX_CACHE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_DOCUMENT_INDEX_CACHE_H

#include <cstddef>
#include <optional>

#include "application/ports/DocumentIndexCache.h"

class FakeDocumentIndexCache final : public DocumentIndexCache
{
public:
    [[nodiscard]] std::optional<RememberedDocuments> Remember() const override
    {
        return kept;
    }

    void Keep(const RememberedDocuments& index) override
    {
        kept = index;
        ++writes;
    }

    std::optional<RememberedDocuments> kept{};
    std::size_t writes = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_DOCUMENT_INDEX_CACHE_H
