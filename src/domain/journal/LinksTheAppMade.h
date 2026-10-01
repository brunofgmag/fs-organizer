#ifndef FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H
#define FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "domain/model/OperationRecord.h"

struct LinkTheAppMade
{
    std::filesystem::path place{};
    std::filesystem::path libraryCopy{};
};

class LinksTheAppMadeSoFar
{
public:
    void Fold(const OperationRecord& record);

    [[nodiscard]] std::vector<LinkTheAppMade> Links() const;

private:
    struct Made
    {
        LinkTheAppMade link{};
        std::string comparableCopy{};
    };

    void FollowTheMove(const OperationRecord& record);

    std::map<std::string, Made> made_{};
};

[[nodiscard]] std::vector<LinkTheAppMade> WhereTheAppMadeLinks(const std::vector<OperationRecord>& history);

#endif // FS_ORGANIZER_DOMAIN_JOURNAL_LINKS_THE_APP_MADE_H
