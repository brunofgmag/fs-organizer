#include "domain/importing/WhatTheImporterBrought.h"

#include <map>
#include <ranges>
#include <string>

#include "domain/support/PathUtils.h"

std::vector<std::filesystem::path> FoldersTheImporterBrought(const std::vector<OperationRecord>& history)
{
    std::map<std::string, std::filesystem::path> brought;

    for (const OperationRecord& record : history)
    {
        if (!Succeeded(record.outcome))
        {
            continue;
        }

        const bool arrivedNow = record.kind == OperationKind::ImportMoveIntoPlace;
        const bool movedOn =
            record.kind == OperationKind::MoveAddon && brought.erase(ComparablePath(record.source)) == 1;

        if (arrivedNow || movedOn)
        {
            brought.insert_or_assign(ComparablePath(record.target), record.target);
        }
    }

    std::vector<std::filesystem::path> folders;
    for (const std::filesystem::path& folder : brought | std::views::values)
    {
        folders.push_back(folder);
    }

    return folders;
}
