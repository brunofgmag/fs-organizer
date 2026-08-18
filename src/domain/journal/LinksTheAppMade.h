#ifndef FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H
#define FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H

#include <filesystem>
#include <vector>

#include "domain/model/OperationRecord.h"

struct LinkTheAppMade
{
    std::filesystem::path place{};
    std::filesystem::path libraryCopy{};
};

[[nodiscard]] std::vector<LinkTheAppMade> WhereTheAppMadeLinks(const std::vector<OperationRecord>& history);

#endif // FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H
