#include "domain/importing/WhatTheImporterBrought.h"

#include <ranges>

#include "domain/support/PathUtils.h"

void FoldersTheImporterBroughtSoFar::Fold(const OperationRecord& record)
{
    if (!Succeeded(record.outcome))
    {
        return;
    }

    const bool arrivedNow = record.kind == OperationKind::ImportMoveIntoPlace;
    const bool movedOn = record.kind == OperationKind::MoveAddon && brought_.erase(ComparablePath(record.source)) == 1;

    if (arrivedNow || movedOn)
    {
        brought_.insert_or_assign(ComparablePath(record.target), record.target);
    }
}

std::vector<std::filesystem::path> FoldersTheImporterBroughtSoFar::Folders() const
{
    std::vector<std::filesystem::path> folders;
    folders.reserve(brought_.size());

    for (const std::filesystem::path& folder : brought_ | std::views::values)
    {
        folders.push_back(folder);
    }

    return folders;
}

std::vector<std::filesystem::path> FoldersTheImporterBrought(const std::vector<OperationRecord>& history)
{
    FoldersTheImporterBroughtSoFar brought;

    for (const OperationRecord& record : history)
    {
        brought.Fold(record);
    }

    return brought.Folders();
}
