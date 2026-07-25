#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LIBRARY_ID_GENERATOR_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LIBRARY_ID_GENERATOR_H

#include <string>

#include "application/ports/LibraryIdGenerator.h"

class FakeLibraryIdGenerator final : public LibraryIdGenerator
{
public:
    [[nodiscard]] LibraryId Generate() const override
    {
        return "library-" + std::to_string(++issued_);
    }

private:
    mutable int issued_ = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LIBRARY_ID_GENERATOR_H
