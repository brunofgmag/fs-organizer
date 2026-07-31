#ifndef FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
#define FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H

#include <string>
#include <vector>

#include "domain/model/LinkType.h"
#include "domain/model/SimulatorProfile.h"

struct AppSettings
{
    std::vector<SimulatorProfile> profiles;
    std::string activeProfileId;
    LinkType linkType = LinkType::Junction;
    bool verifyWithHash = false;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
