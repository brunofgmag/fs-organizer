#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H

#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "application/ports/PresetRepository.h"

class FakePresetRepository final : public PresetRepository
{
public:
    [[nodiscard]] std::vector<std::string> List(const std::string& profileId) const override
    {
        std::vector<std::string> names;

        const auto profile = byProfile_.find(profileId);
        if (profile == byProfile_.end())
        {
            return names;
        }

        for (const auto& name : profile->second | std::views::keys)
        {
            names.push_back(name);
        }

        return names;
    }

    [[nodiscard]] std::optional<Preset> Load(const std::string& profileId, const std::string& name) const override
    {
        const auto profile = byProfile_.find(profileId);
        if (profile == byProfile_.end())
        {
            return std::nullopt;
        }

        const auto preset = profile->second.find(name);
        if (preset == profile->second.end())
        {
            return std::nullopt;
        }

        return preset->second;
    }

    [[nodiscard]] bool Save(const std::string& profileId, const Preset& preset) override
    {
        if (refusing_)
        {
            return false;
        }

        byProfile_[profileId][preset.name] = preset;

        return true;
    }

    [[nodiscard]] bool Rename(const std::string& profileId, const std::string& from, const std::string& to) override
    {
        const std::optional<Preset> preset = Load(profileId, from);
        if (!preset.has_value())
        {
            return false;
        }

        Preset renamed = *preset;
        renamed.name = to;

        if (!Save(profileId, renamed))
        {
            return false;
        }

        if (from != to)
        {
            Remove(profileId, from);
        }

        return true;
    }

    void Remove(const std::string& profileId, const std::string& name) override
    {
        byProfile_[profileId].erase(name);
    }

    void RefuseEveryWrite()
    {
        refusing_ = true;
    }

private:
    std::map<std::string, std::map<std::string, Preset>> byProfile_;
    bool refusing_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H
