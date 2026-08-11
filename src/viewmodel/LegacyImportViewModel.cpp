#include "viewmodel/LegacyImportViewModel.h"

#include <algorithm>

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
                                             const LegacyConfigImporter& importer,
                                             const PresetService& presets,
                                             QObject* parent)
    : QObject(parent), session_(session), importer_(importer), presets_(presets)
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

LegacyImportReport LegacyImportViewModel::Import(const LegacyImportRequest& request) const
{
    return session_.ImportLegacy(request);
}

LegacyPresetReport LegacyImportViewModel::ImportPresets(const std::filesystem::path& presetsPath) const
{
    LegacyPresetReport report;

    const SimulatorProfile& profile = session_.Profile();
    const std::vector<PresetListing> stored = presets_.List(profile.id);

    for (const ImportedPreset& imported : importer_.ImportPresets(presetsPath, profile, session_.Snapshot().libraries))
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

    return report;
}
