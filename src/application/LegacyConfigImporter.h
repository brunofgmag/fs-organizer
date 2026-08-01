#ifndef FS_ORGANIZER_APPLICATION_LEGACY_CONFIG_IMPORTER_H
#define FS_ORGANIZER_APPLICATION_LEGACY_CONFIG_IMPORTER_H

#include <filesystem>
#include <vector>

#include "application/model/LegacyMigration.h"
#include "application/ports/LegacyConfigSource.h"
#include "domain/legacy/LegacyPreset.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"
#include "domain/ports/FilesystemProbe.h"

class LegacyConfigImporter
{
public:
    LegacyConfigImporter(const LegacyConfigSource& source, const FilesystemProbe& filesystemProbe);

    [[nodiscard]] std::vector<LegacyMigration> Propose(const SimulatorProfile& current,
                                                       const std::vector<TreeNode>& scanned) const;

    [[nodiscard]] std::vector<ImportedPreset> ImportPresets(const std::filesystem::path& presetsPath,
                                                            const SimulatorProfile& profile,
                                                            const std::vector<TreeNode>& scanned) const;

private:
    const LegacyConfigSource& source_;
    const FilesystemProbe& filesystemProbe_;
};

#endif // FS_ORGANIZER_APPLICATION_LEGACY_CONFIG_IMPORTER_H
