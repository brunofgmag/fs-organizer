#include "domain/journal/JournalEntries.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

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
}

const OperationRecord& JournalEntry::First() const
{
    return steps.front();
}

const OperationRecord& JournalEntry::Last() const
{
    return steps.back();
}

bool JournalEntry::IsAnImportRun() const
{
    return steps.size() > 1;
}

bool JournalEntry::Succeeded() const
{
    return std::ranges::all_of(steps, StepSucceeded);
}

bool StepSucceeded(const OperationRecord& record)
{
    return Succeeded(record.outcome);
}

std::vector<JournalEntry> GroupImportRuns(const std::vector<OperationRecord>& records)
{
    std::vector<JournalEntry> entries;

    for (std::size_t first = 0; first < records.size();)
    {
        const std::size_t taken =
            records[first].kind == OperationKind::ImportCopyToStaging ? StepsOfTheRunStartingAt(records, first) : 1;

        entries.push_back(JournalEntry{{records.begin() + static_cast<std::ptrdiff_t>(first),
                                        records.begin() + static_cast<std::ptrdiff_t>(first + taken)}});
        first += taken;
    }

    return entries;
}
