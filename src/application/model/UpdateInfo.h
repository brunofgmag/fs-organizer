#ifndef FS_ORGANIZER_APPLICATION_MODEL_UPDATE_INFO_H
#define FS_ORGANIZER_APPLICATION_MODEL_UPDATE_INFO_H

#include <string>

struct UpdateInfo
{
    std::string version;
    std::string releasePageUrl;
    std::string zipUrl;
    std::string shaUrl;
    std::string zipName;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_UPDATE_INFO_H
