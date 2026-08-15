#ifndef FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H
#define FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H

#include <string>
#include <vector>

#include "domain/documents/DocumentBookmarks.h"

struct ReadDocument
{
    std::string addon{};
    std::string document{};
    int page = 0;
    bool favourite = false;
    std::vector<DocumentBookmark> bookmarks{};
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H
