#ifndef FS_ORGANIZER_APPLICATION_PORTS_SETTINGS_REPOSITORY_H
#define FS_ORGANIZER_APPLICATION_PORTS_SETTINGS_REPOSITORY_H

#include "application/model/AppSettings.h"

class SettingsRepository
{
public:
    virtual ~SettingsRepository() = default;

    [[nodiscard]] virtual AppSettings Load() const = 0;

    virtual void Save(const AppSettings& settings) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_SETTINGS_REPOSITORY_H
