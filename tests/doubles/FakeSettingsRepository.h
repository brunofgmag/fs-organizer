#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H

#include "application/ports/SettingsRepository.h"

class FakeSettingsRepository final : public SettingsRepository
{
public:
    [[nodiscard]] std::optional<AppSettings> Load() const override
    {
        if (unreadable)
        {
            return std::nullopt;
        }

        return stored;
    }

    [[nodiscard]] bool Save(const AppSettings& settings) override
    {
        if (refusing)
        {
            return false;
        }

        stored = settings;
        ++saves;

        return true;
    }

    AppSettings stored;
    int saves = 0;
    bool refusing = false;
    bool unreadable = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
