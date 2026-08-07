#include "domain/journal/JournalEntries.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr std::array kImportRunSteps{
        OperationKind::ImportCopyToStaging, OperationKind::ImportVerifyStaging, OperationKind::ImportMoveIntoPlace,
        OperationKind::ImportRemoveSource,  OperationKind::EnableAddon,
    };

    std::optional<std::size_t> PositionInTheRun(const OperationKind kind)
    {
        const auto step = std::ranges::find(kImportRunSteps, kind);

        return step == kImportRunSteps.end() ? std::nullopt
                                             : std::optional(static_cast<std::size_t>(step - kImportRunSteps.begin()));
    }

    std::size_t StepsOfTheRunStartingAt(const std::vector<OperationRecord>& records, const std::size_t first)
    {
        std::size_t last = first;
        std::size_t reached = 0;

        for (std::size_t next = first + 1; next < records.size(); ++next)
        {
            const std::optional<std::size_t> position = PositionInTheRun(records[next].kind);

            if (!position.has_value() || *position <= reached || !(records[next].addonId == records[first].addonId))
            {
                break;
            }

            reached = *position;
            last = next;
        }

        return last - first + 1;
    }

    bool ASwapStartsAt(const std::vector<OperationRecord>& records, const std::size_t first)
    {
        if (records[first].kind != OperationKind::DisableAddon || first + 1 >= records.size())
        {
            return false;
        }

        const OperationRecord& next = records[first + 1];

        return next.kind == OperationKind::EnableAddon
            && ComparablePath(next.target) == ComparablePath(records[first].target)
            && !(next.addonId == records[first].addonId);
    }

    JournalEntry EntryOf(const std::vector<OperationRecord>& records,
                         const std::size_t first,
                         const std::size_t taken,
                         const JournalEntryKind kind)
    {
        return JournalEntry{{records.begin() + static_cast<std::ptrdiff_t>(first),
                             records.begin() + static_cast<std::ptrdiff_t>(first + taken)},
                            kind};
    }
}

const OperationRecord& JournalEntry::First() const
{
    return steps.front();
}

const OperationRecord& JournalEntry::Last() const
{
    return steps.back();
}

const OperationRecord& JournalEntry::WhereItStopped() const
{
    const auto stopped = std::ranges::find_if_not(steps, StepSucceeded);

    return stopped == steps.end() ? Last() : *stopped;
}

bool JournalEntry::IsAnImportRun() const
{
    return kind == JournalEntryKind::ImportRun;
}

bool JournalEntry::IsASwap() const
{
    return kind == JournalEntryKind::Swap;
}

bool JournalEntry::HasSteps() const
{
    return kind != JournalEntryKind::OneOperation;
}

bool JournalEntry::Succeeded() const
{
    return std::ranges::all_of(steps, StepSucceeded);
}

bool StepSucceeded(const OperationRecord& record)
{
    return Succeeded(record.outcome);
}

std::vector<JournalEntry> GroupOperations(const std::vector<OperationRecord>& records)
{
    std::vector<JournalEntry> entries;

    for (std::size_t first = 0; first < records.size();)
    {
        if (records[first].kind == OperationKind::ImportCopyToStaging)
        {
            const std::size_t taken = StepsOfTheRunStartingAt(records, first);

            entries.push_back(EntryOf(records, first, taken,
                                      taken > 1 ? JournalEntryKind::ImportRun : JournalEntryKind::OneOperation));
            first += taken;
            continue;
        }

        if (ASwapStartsAt(records, first))
        {
            entries.push_back(EntryOf(records, first, 2, JournalEntryKind::Swap));
            first += 2;
            continue;
        }

        entries.push_back(EntryOf(records, first, 1, JournalEntryKind::OneOperation));
        ++first;
    }

    return entries;
}
