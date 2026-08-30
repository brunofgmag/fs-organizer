#include "viewmodel/LegacyImportViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "domain/support/StringUtils.h"

namespace
{
    bool NameIsTaken(const std::vector<PresetListing>& listings, const std::string& name)
    {
        return std::ranges::any_of(listings,
                                   [&name](const PresetListing& listing)
                                   {
                                       return EqualsIgnoringCase(listing.name, name);
                                   });
    }

    bool HoldsSomethingNew(const MigratableLibrary& library)
    {
        return library.rootExists
            && (library.proposal.state == ProposedState::New
                || std::ranges::any_of(library.proposal.categories,
                                       [](const ProposedCategory& category)
                                       {
                                           return category.state == ProposedState::New;
                                       }));
    }
}

LegacyImportViewModel::LegacyImportViewModel(Session& session,
                                             const SessionNotifier& notifier,
                                             const LegacyConfigImporter& importer,
                                             const PresetService& presets,
                                             BackgroundRunner& runner,
                                             QObject* parent)
    : QObject(parent),
      session_(session),
      notifier_(notifier),
      importer_(importer),
      presets_(presets),
      importing_(runner)
{
}

std::vector<LegacyMigration> LegacyImportViewModel::Migrations() const
{
    return importer_.Propose(session_.Profile(), session_.Snapshot().libraries);
}

bool LegacyImportViewModel::SomethingIsWaiting() const
{
    const std::vector<LegacyMigration> migrations = Migrations();

    return std::ranges::any_of(migrations,
                               [](const LegacyMigration& migration)
                               {
                                   return std::ranges::any_of(migration.libraries, HoldsSomethingNew);
                               });
}

std::size_t LegacyImportViewModel::PresetsWaitingIn(const std::filesystem::path& presetsPath) const
{
    if (presetsPath.empty())
    {
        return 0;
    }

    return importer_.PresetsWaitingIn(presetsPath);
}

void LegacyImportViewModel::Import(const LegacyImportRequest& request, std::vector<std::filesystem::path> presetFolders)
{
    const auto imported = std::make_shared<Session::LegacyImport>();
    imported->profile = session_.Profile();

    importing_.Run(
        [this, imported, request]
        {
            *imported = session_.ImportLegacyOn(std::move(imported->profile), request);
        },
        [this, imported, folders = std::move(presetFolders)]
        {
            const LegacyImportReport report = imported->report;

            if (report.librariesRegistered == 0 && report.categoriesDeclared == 0)
            {
                emit Imported(report, ImportPresets(folders));
                return;
            }

            LandWhenTheLibrariesAreReadable(report, folders);

            session_.AdoptTheLegacyImport(std::move(*imported));
        });
}

void LegacyImportViewModel::LandWhenTheLibrariesAreReadable(const LegacyImportReport& report,
                                                            const std::vector<std::filesystem::path>& presetFolders)
{
    connect(
        &notifier_, &SessionNotifier::ScanFinished, this,
        [this, report, presetFolders]
        {
            emit Imported(report, ImportPresets(presetFolders));
        },
        Qt::SingleShotConnection);
}

LegacyPresetReport LegacyImportViewModel::ImportPresets(const std::vector<std::filesystem::path>& presetFolders) const
{
    LegacyPresetReport report;

    const SimulatorProfile& profile = session_.Profile();

    for (const std::filesystem::path& presetsPath : presetFolders)
    {
        const std::vector<PresetListing> stored = presets_.List(profile.id);

        for (const ImportedPreset& imported :
             importer_.ImportPresets(presetsPath, profile, session_.Snapshot().libraries))
        {
            report.entriesNotFound += imported.unresolvedAddonNames.size() + imported.unresolvedFolders.size();

            if (NameIsTaken(stored, imported.preset.name))
            {
                ++report.nameAlreadyTaken;
                continue;
            }

            if (presets_.Store(profile.id, imported.preset))
            {
                ++report.imported;
            }
        }
    }

    return report;
}
