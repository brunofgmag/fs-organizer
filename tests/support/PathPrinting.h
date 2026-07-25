#ifndef FS_ORGANIZER_TESTS_SUPPORT_PATH_PRINTING_H
#define FS_ORGANIZER_TESTS_SUPPORT_PATH_PRINTING_H

#include <QtTest/QtTest>

#include <filesystem>

namespace QTest
{
    template <>
    inline char* toString(const std::filesystem::path& t)
    {
        return qstrdup(t.generic_string().c_str());
    }
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_PATH_PRINTING_H
