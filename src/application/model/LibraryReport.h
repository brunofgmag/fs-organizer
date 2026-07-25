#ifndef FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H

#include <cstddef>

enum class LibraryCheck : int
{
    Accepted = 0,
    RejectedInsideAnotherLibrary = 1,
};

struct LibraryReport
{
    LibraryCheck check = LibraryCheck::Accepted;
    std::size_t categories = 0;
    std::size_t addons = 0;

    [[nodiscard]] bool Accepted() const
    {
        return check == LibraryCheck::Accepted;
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LIBRARY_REPORT_H
