#ifndef FS_ORGANIZER_APPLICATION_MODEL_LEGACY_MIGRATION_H
#define FS_ORGANIZER_APPLICATION_MODEL_LEGACY_MIGRATION_H

#include <filesystem>
#include <vector>

#include "domain/legacy/LegacyProposal.h"

struct MigratableLibrary
{
    ProposedLibrary proposal;
    bool rootExists = true;
};

struct LegacyMigration
{
    std::filesystem::path folder;
    std::filesystem::path presetsPath;
    bool configurationWasRead = true;
    std::vector<MigratableLibrary> libraries;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_LEGACY_MIGRATION_H
