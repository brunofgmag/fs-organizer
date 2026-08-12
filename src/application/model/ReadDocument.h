#ifndef FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H
#define FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H

#include <string>

struct ReadDocument
{
    std::string addon{};
    std::string document{};
    int page = 0;
    bool favourite = false;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_READ_DOCUMENT_H
