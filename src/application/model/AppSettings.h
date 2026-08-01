#ifndef FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
#define FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H

#include <string>
#include <vector>

#include "application/model/UpdateMode.h"
#include "domain/model/LinkType.h"
#include "domain/model/SimulatorProfile.h"

struct AppSettings
{
    std::vector<SimulatorProfile> profiles;
    std::string activeProfileId;
    LinkType linkType = LinkType::Junction;
    bool verifyWithHash = false;
    UpdateMode updateMode = UpdateMode::Notify;
    std::string language;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
