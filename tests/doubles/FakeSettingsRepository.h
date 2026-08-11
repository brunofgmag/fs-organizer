#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H

#include <functional>
#include <utility>

#include "application/ports/SettingsRepository.h"

inline AppSettings SettingsWith(SimulatorProfile profile)
{
    AppSettings settings;
    AddProfile(settings, std::move(profile));

    return settings;
}

class FakeSettingsRepository final : public SettingsRepository
{
public:
    FakeSettingsRepository() = default;

    explicit FakeSettingsRepository(AppSettings settings) : stored(std::move(settings))
    {
    }

    [[nodiscard]] std::optional<AppSettings> Load() const override
    {
        ++loads;

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
    mutable int loads = 0;
    int saves = 0;
    bool refusing = false;
};

inline std::function<bool(const SimulatorProfile&)> KeepIn(FakeSettingsRepository& repository)
{
    return [&repository](const SimulatorProfile& profile)
    {
        AppSettings next = repository.stored;
        AddProfile(next, profile);

        return repository.Save(next);
    };
}

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SETTINGS_REPOSITORY_H
