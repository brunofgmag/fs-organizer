#ifndef FS_ORGANIZER_APPLICATION_PORTS_PRESET_REPOSITORY_H
#define FS_ORGANIZER_APPLICATION_PORTS_PRESET_REPOSITORY_H

#include <optional>
#include <string>
#include <vector>

#include "domain/model/Preset.h"

class PresetRepository
{
public:
    virtual ~PresetRepository() = default;

    [[nodiscard]] virtual std::vector<std::string> List(const std::string& profileId) const = 0;

    [[nodiscard]] virtual std::optional<Preset> Load(const std::string& profileId, const std::string& name) const = 0;

    [[nodiscard]] virtual bool Save(const std::string& profileId, const Preset& preset) = 0;

    [[nodiscard]] virtual bool Rename(const std::string& profileId, const std::string& from, const std::string& to) = 0;

    virtual void Remove(const std::string& profileId, const std::string& name) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_PRESET_REPOSITORY_H
