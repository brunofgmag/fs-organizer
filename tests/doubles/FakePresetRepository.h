#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H

#include <chrono>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "application/ports/PresetRepository.h"

class FakePresetRepository final : public PresetRepository
{
public:
    [[nodiscard]] std::vector<PresetListing> List(const std::string& profileId) const override
    {
        std::vector<PresetListing> listings;

        const auto profile = byProfile_.find(profileId);
        if (profile == byProfile_.end())
        {
            return listings;
        }

        for (const auto& name : profile->second | std::views::keys)
        {
            const auto written = writtenAt_.find(name);

            listings.push_back(
                {.name = name,
                 .writtenAt = written == writtenAt_.end() ? std::nullopt : std::optional(written->second)});
        }

        return listings;
    }

    void SayItWasWrittenAt(const std::string& name, const std::chrono::system_clock::time_point when)
    {
        writtenAt_[name] = when;
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

    [[nodiscard]] std::optional<Preset> LoadReturnPreset(const std::string& profileId) const override
    {
        const auto preset = returns_.find(profileId);

        return preset == returns_.end() ? std::nullopt : std::optional(preset->second);
    }

    [[nodiscard]] bool SaveReturnPreset(const std::string& profileId, const Preset& preset) override
    {
        if (refusing_)
        {
            return false;
        }

        returns_[profileId] = preset;

        return true;
    }

    void RefuseEveryWrite()
    {
        refusing_ = true;
    }

private:
    std::map<std::string, std::map<std::string, Preset>> byProfile_;
    std::map<std::string, Preset> returns_;
    std::map<std::string, std::chrono::system_clock::time_point> writtenAt_;
    bool refusing_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PRESET_REPOSITORY_H
