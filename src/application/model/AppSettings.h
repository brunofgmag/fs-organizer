#ifndef FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
#define FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H

#include <string>
#include <utility>
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
    bool manageStartupEntries = true;
    UpdateMode updateMode = UpdateMode::Notify;
    std::string language;
};

inline void AddProfile(AppSettings& settings, SimulatorProfile profile)
{
    settings.activeProfileId = profile.id;
    settings.profiles.push_back(std::move(profile));
}

#endif // FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
