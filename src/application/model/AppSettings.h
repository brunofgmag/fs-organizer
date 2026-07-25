#ifndef FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
#define FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H

#include <string>
#include <vector>

#include "domain/model/SimulatorProfile.h"

struct AppSettings
{
    std::vector<SimulatorProfile> profiles;
    std::string activeProfileId;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
