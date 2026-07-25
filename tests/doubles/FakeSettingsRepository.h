#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H

#include "application/ports/SettingsRepository.h"

class FakeSettingsRepository final : public SettingsRepository
{
public:
    [[nodiscard]] AppSettings Load() const override
    {
        return stored;
    }

    void Save(const AppSettings& settings) override
    {
        stored = settings;
        ++saves;
    }

    AppSettings stored;
    int saves = 0;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
