#ifndef FS_ORGANIZER_INFRASTRUCTURE_SETTINGS_JSON_SETTINGS_REPOSITORY_H
#define FS_ORGANIZER_INFRASTRUCTURE_SETTINGS_JSON_SETTINGS_REPOSITORY_H

#include <filesystem>

#include "application/ports/SettingsRepository.h"

class JsonSettingsRepository final : public SettingsRepository
{
public:
    explicit JsonSettingsRepository(std::filesystem::path file);

    [[nodiscard]] std::optional<AppSettings> Load() const override;

    [[nodiscard]] bool Save(const AppSettings& settings) override;

private:
    std::filesystem::path file_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SETTINGS_JSON_SETTINGS_REPOSITORY_H
