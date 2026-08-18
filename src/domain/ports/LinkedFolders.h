#ifndef FS_ORGANIZER_DOMAIN_PORTS_LINKED_FOLDERS_H
#define FS_ORGANIZER_DOMAIN_PORTS_LINKED_FOLDERS_H

#include <vector>

#include "domain/journal/LinksTheAppMade.h"

class LinkedFolders
{
public:
    virtual ~LinkedFolders() = default;

    [[nodiscard]] virtual std::vector<LinkTheAppMade> WhatTheAppLinked() const = 0;
};

class NothingWasLinked final : public LinkedFolders
{
public:
    [[nodiscard]] std::vector<LinkTheAppMade> WhatTheAppLinked() const override
    {
        return {};
    }
};

[[nodiscard]] inline const LinkedFolders& NoLinkWasEverMade()
{
    static const NothingWasLinked nothing;

    return nothing;
}

#endif // FS_ORGANIZER_DOMAIN_PORTS_LINKED_FOLDERS_H
