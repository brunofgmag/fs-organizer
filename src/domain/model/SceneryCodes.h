#ifndef FS_ORGANIZER_DOMAIN_MODEL_SCENERY_CODES_H
#define FS_ORGANIZER_DOMAIN_MODEL_SCENERY_CODES_H

#include <string>
#include <vector>

enum class SceneryReading : int
{
    Read = 0,
    ItCarriesNoSignature = 1,
    ItEndsBeforeItSaysItDoes = 2,
};

struct SceneryCodes
{
    SceneryReading reading = SceneryReading::Read;
    std::vector<std::string> codes{};
    bool anIdentifierDidNotDecode = false;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_SCENERY_CODES_H
