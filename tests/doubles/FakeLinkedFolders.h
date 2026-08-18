#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINKED_FOLDERS_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINKED_FOLDERS_H

#include <filesystem>
#include <vector>

#include "domain/ports/LinkedFolders.h"

class FakeLinkedFolders final : public LinkedFolders
{
public:
    [[nodiscard]] std::vector<LinkTheAppMade> WhatTheAppLinked() const override
    {
        return made;
    }

    void Remember(const std::filesystem::path& place, const std::filesystem::path& libraryCopy)
    {
        made.push_back(LinkTheAppMade{.place = place, .libraryCopy = libraryCopy});
    }

    std::vector<LinkTheAppMade> made{};
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_LINKED_FOLDERS_H
