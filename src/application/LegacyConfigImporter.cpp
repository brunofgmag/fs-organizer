#include "application/LegacyConfigImporter.h"

#include <utility>

LegacyConfigImporter::LegacyConfigImporter(const LegacyConfigSource& source, const FilesystemProbe& filesystemProbe)
    : source_(source), filesystemProbe_(filesystemProbe)
{
}

std::vector<LegacyMigration> LegacyConfigImporter::Propose(const SimulatorProfile& current,
                                                           const std::vector<TreeNode>& scanned) const
{
    std::vector<LegacyMigration> migrations;

    for (const FoundLegacyInstallation& found : source_.Installations())
    {
        LegacyMigration migration;
        migration.folder = found.folder;
        migration.configurationWasRead = found.configuration.has_value();

        if (migration.configurationWasRead)
        {
            migration.presetsPath = found.configuration->presetsPath;

            for (ProposedLibrary& proposal : ProposeLibraries(*found.configuration, current, scanned))
            {
                const bool rootExists = filesystemProbe_.TargetDirectoryExists(proposal.root);

                migration.libraries.push_back(
                    MigratableLibrary{.proposal = std::move(proposal), .rootExists = rootExists});
            }
        }

        migrations.push_back(std::move(migration));
    }

    return migrations;
}

std::vector<ImportedPreset> LegacyConfigImporter::ImportPresets(const std::filesystem::path& presetsPath,
                                                                const SimulatorProfile& profile,
                                                                const std::vector<TreeNode>& scanned) const
{
    std::vector<ImportedPreset> imported;

    for (const LegacyPresetSelection& selection : source_.PresetsIn(presetsPath))
    {
        imported.push_back(ImportLegacyPreset(selection, profile, scanned));
    }

    return imported;
}
