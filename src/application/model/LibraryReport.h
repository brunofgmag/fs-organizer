#ifndef FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H

#include <cstddef>

struct LibraryReport
{
    bool accepted = false;
    std::size_t categories = 0;
    std::size_t addons = 0;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H
