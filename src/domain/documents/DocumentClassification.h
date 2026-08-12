#ifndef FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_CLASSIFICATION_H
#define FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_CLASSIFICATION_H

#include <filesystem>
#include <string>
#include <vector>

enum class DocumentKind : int
{
    Document = 0,
    Chart = 1,
};

struct ClassifiedDocument
{
    DocumentKind kind = DocumentKind::Document;
    std::string code{};
};

[[nodiscard]] ClassifiedDocument ClassifyDocument(const std::filesystem::path& relativePath,
                                                  const std::vector<std::string>& knownCodes);

#endif // FS_ORGANIZER_DOMAIN_DOCUMENTS_DOCUMENT_CLASSIFICATION_H
