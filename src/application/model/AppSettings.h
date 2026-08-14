#ifndef FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
#define FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H

#include <string>
#include <utility>
#include <vector>

#include "application/model/ReadDocument.h"
#include "application/model/ReadingGestures.h"
#include "application/model/UpdateMode.h"
#include "domain/model/LinkType.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/Verification.h"
#include "domain/scenery/AirportCoverage.h"

struct AppSettings
{
    std::vector<SimulatorProfile> profiles;
    std::string activeProfileId;
    LinkType linkType = LinkType::Junction;
    Verification verification = Verification::ByStructure;
    bool manageStartupEntries = true;
    bool managePackageList = false;
    ReadingGestures onCharts = kGesturesAChartIsBornWith;
    ReadingGestures onDocuments = kGesturesADocumentIsBornWith;
    UpdateMode updateMode = UpdateMode::Notify;
    std::string language;
    std::vector<CoexistingPair> coexistingAirports;
    std::vector<ReadDocument> documents;
};

inline void AddProfile(AppSettings& settings, SimulatorProfile profile)
{
    settings.activeProfileId = profile.id;
    settings.profiles.push_back(std::move(profile));
}

#endif // FS_ORGANIZER_APPLICATION_MODEL_APP_SETTINGS_H
