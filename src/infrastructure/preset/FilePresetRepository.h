#ifndef FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H
#define FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H

#include <filesystem>

#include "application/ports/PresetRepository.h"

class FilePresetRepository final : public PresetRepository
{
public:
    explicit FilePresetRepository(std::filesystem::path root);

    [[nodiscard]] std::vector<std::string> List(const std::string& profileId) const override;

    [[nodiscard]] std::optional<Preset> Load(const std::string& profileId, const std::string& name) const override;

    bool Save(const std::string& profileId, const Preset& preset) override;

    bool Rename(const std::string& profileId, const std::string& from, const std::string& to) override;

    void Remove(const std::string& profileId, const std::string& name) override;

private:
    [[nodiscard]] std::filesystem::path FolderOf(const std::string& profileId) const;

    [[nodiscard]] std::filesystem::path FileOf(const std::string& profileId, const std::string& name) const;

    std::filesystem::path root_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_PRESET_FILE_PRESET_REPOSITORY_H
