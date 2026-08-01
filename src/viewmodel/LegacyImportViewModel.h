#ifndef FS_ORGANIZER_VIEWMODEL_LEGACY_IMPORT_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_LEGACY_IMPORT_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <vector>

#include <QtCore/QObject>

#include "application/LegacyConfigImporter.h"
#include "application/PresetService.h"
#include "application/Session.h"
#include "application/model/LegacyImport.h"
#include "application/model/LegacyMigration.h"

struct LegacyPresetReport
{
    std::size_t imported = 0;
    std::size_t nameAlreadyTaken = 0;
    std::size_t entriesNotFound = 0;
};

class LegacyImportViewModel final : public QObject
{
    Q_OBJECT

public:
    LegacyImportViewModel(Session& session,
                          const LegacyConfigImporter& importer,
                          const PresetService& presets,
                          QObject* parent = nullptr);

    [[nodiscard]] std::vector<LegacyMigration> Migrations() const;

    [[nodiscard]] std::size_t PresetsWaitingIn(const std::filesystem::path& presetsPath) const;

    [[nodiscard]] LegacyImportReport Import(const LegacyImportRequest& request) const;

    [[nodiscard]] LegacyPresetReport ImportPresets(const std::filesystem::path& presetsPath) const;

private:
    Session& session_;
    const LegacyConfigImporter& importer_;
    const PresetService& presets_;
};

#endif // FS_ORGANIZER_VIEWMODEL_LEGACY_IMPORT_VIEW_MODEL_H
