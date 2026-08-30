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
#include "application/ports/BackgroundRunner.h"
#include "viewmodel/GuardedRunner.h"
#include "viewmodel/SessionNotifier.h"

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
                          const SessionNotifier& notifier,
                          const LegacyConfigImporter& importer,
                          const PresetService& presets,
                          BackgroundRunner& runner,
                          QObject* parent = nullptr);

    [[nodiscard]] std::vector<LegacyMigration> Migrations() const;

    [[nodiscard]] bool SomethingIsWaiting() const;

    [[nodiscard]] std::size_t PresetsWaitingIn(const std::filesystem::path& presetsPath) const;

    void Import(const LegacyImportRequest& request, std::vector<std::filesystem::path> presetFolders);

signals:
    void Imported(const LegacyImportReport& report, const LegacyPresetReport& presets);

private:
    [[nodiscard]] LegacyPresetReport ImportPresets(const std::vector<std::filesystem::path>& presetFolders) const;

    void LandWhenTheLibrariesAreReadable(const LegacyImportReport& report,
                                         const std::vector<std::filesystem::path>& presetFolders);

    Session& session_;
    const SessionNotifier& notifier_;
    const LegacyConfigImporter& importer_;
    const PresetService& presets_;
    GuardedRunner importing_;
};

#endif // FS_ORGANIZER_VIEWMODEL_LEGACY_IMPORT_VIEW_MODEL_H
