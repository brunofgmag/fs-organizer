#ifndef FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_BOOKMARKS_H
#define FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_BOOKMARKS_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct DocumentSection
{
    std::string title{};
    int page = 0;
};

struct DocumentBookmark
{
    int page = 0;
    std::string name{};
};

[[nodiscard]] std::optional<std::size_t> TheSectionHolding(const std::vector<DocumentSection>& sections, int page);

#endif // FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_BOOKMARKS_H
