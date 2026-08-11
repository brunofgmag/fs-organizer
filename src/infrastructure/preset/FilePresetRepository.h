#ifndef FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H
#define FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H

#include <filesystem>

#include "application/ports/PresetRepository.h"

class FilePresetRepository final : public PresetRepository
{
public:
    explicit FilePresetRepository(std::filesystem::path root);

    [[nodiscard]] std::vector<PresetListing> List(const std::string& profileId) const override;

    [[nodiscard]] std::optional<Preset> Load(const std::string& profileId, const std::string& name) const override;

    [[nodiscard]] bool Save(const std::string& profileId, const Preset& preset) override;

    [[nodiscard]] bool Rename(const std::string& profileId, const std::string& from, const std::string& to) override;

    void Remove(const std::string& profileId, const std::string& name) override;

    [[nodiscard]] std::optional<Preset> LoadReturnPreset(const std::string& profileId) const override;

    [[nodiscard]] bool SaveReturnPreset(const std::string& profileId, const Preset& preset) override;

private:
    [[nodiscard]] std::optional<std::filesystem::path> FolderOf(const std::string& profileId) const;

    [[nodiscard]] std::optional<std::filesystem::path> FileOf(const std::string& profileId,
                                                              const std::string& name) const;

    [[nodiscard]] std::optional<std::filesystem::path> ReturnFileOf(const std::string& profileId) const;

    std::filesystem::path root_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H
