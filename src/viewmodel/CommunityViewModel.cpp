#include "viewmodel/CommunityViewModel.h"

#include <algorithm>
#include <map>
#include <string>

#include "domain/support/PathUtils.h"

namespace
{
    [[nodiscard]] std::filesystem::path WhereTheBytesAre(const DestinationEntry& entry)
    {
        return entry.target.empty() ? entry.path : entry.target;
    }

    [[nodiscard]] bool WorthMeasuring(const DestinationEntry& entry)
    {
        return entry.classification != EntryClassification::Unavailable;
    }

    void NoteAsUnmeasured(std::vector<UnmeasuredEntries>& unmeasured, EntryClassification classification);

    [[nodiscard]] SelectionSize AttributedToTheEntries(const std::vector<DestinationEntry>& entries,
                                                       const FolderSizeReport& report,
                                                       SelectionSize answer)
    {
        std::map<std::string, bool> answered;
        for (const MeasuredFolder& folder : report.folders)
        {
            answered[ComparablePath(folder.folder)] = folder.measured;
        }

        answer.bytes = report.bytes;

        for (const DestinationEntry& entry : entries)
        {
            if (!WorthMeasuring(entry))
            {
                continue;
            }

            if (answered[ComparablePath(WhereTheBytesAre(entry))])
            {
                ++answer.measured;
                continue;
            }

            NoteAsUnmeasured(answer.unmeasured, entry.classification);
        }

        return answer;
    }

    void NoteAsUnmeasured(std::vector<UnmeasuredEntries>& unmeasured, const EntryClassification classification)
    {
        const auto already = std::ranges::find_if(unmeasured,
                                                  [classification](const UnmeasuredEntries& group)
                                                  {
                                                      return group.classification == classification;
                                                  });

        if (already != unmeasured.end())
        {
            ++already->count;
            return;
        }

        unmeasured.push_back(UnmeasuredEntries{.classification = classification, .count = 1});
    }
}

CommunityViewModel::CommunityViewModel(ProfileService& service,
                                       Session& session,
                                       const SessionNotifier& notifier,
                                       CommunityModel& model,
                                       SizeService& sizes,
                                       QObject* parent)
    : QObject(parent), service_(service), session_(session), model_(model), sizes_(sizes), caller_(sizes.NewCaller())
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &CommunityViewModel::Show);
}

void CommunityViewModel::Show()
{
    Refresh();
}

void CommunityViewModel::MeasureTheSelection(const std::vector<DestinationEntry>& entries)
{
    std::vector<std::filesystem::path> folders;
    SelectionSize skipped{.selected = entries.size()};

    for (const DestinationEntry& entry : entries)
    {
        if (!WorthMeasuring(entry))
        {
            NoteAsUnmeasured(skipped.unmeasured, entry.classification);
            continue;
        }

        folders.push_back(WhereTheBytesAre(entry));
    }

    if (folders.empty())
    {
        emit SizeMeasured(skipped);
        return;
    }

    emit SizeMeasuring();

    sizes_.MeasureFolders(folders, caller_, Freshness::ReuseWhatIsKnown, {},
                          [this, entries, skipped](const FolderSizeReport& report)
                          {
                              emit SizeMeasured(AttributedToTheEntries(entries, report, skipped));
                          });
}

std::vector<RepairCandidate> CommunityViewModel::PlanRepairs() const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();

    return ::PlanRepairs(session_.Profile(), snapshot.entries, snapshot.libraries);
}

void CommunityViewModel::Repair(const std::vector<RepairRequest>& requests)
{
    const std::vector<LinkOperationResult> results = service_.Repair(session_.Profile(), requests);

    session_.RefreshEntries();
    Refresh();

    emit RepairFinished(results);
}

AttentionBreakdown CommunityViewModel::Breakdown() const
{
    return breakdown_;
}

const ProfileSnapshot& CommunityViewModel::Snapshot() const
{
    return session_.Snapshot();
}

void CommunityViewModel::Refresh()
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const std::vector<DestinationEntry>& entries = snapshot.entries;

    model_.ShowEntries(entries, session_.Profile(), snapshot.conflicts);

    const auto classified = [&entries](const EntryClassification wanted)
    {
        return static_cast<std::size_t>(std::ranges::count_if(entries,
                                                              [wanted](const DestinationEntry& entry)
                                                              {
                                                                  return entry.classification == wanted;
                                                              }));
    };

    const AttentionBreakdown breakdown{
        .broken = classified(EntryClassification::Broken),
        .conflicts = snapshot.conflicts.Count(),
        .duplicated = classified(EntryClassification::Duplicated),
        .unmanaged = classified(EntryClassification::Unmanaged),
    };

    if (breakdown != breakdown_)
    {
        breakdown_ = breakdown;
        emit BreakdownChanged(breakdown_);
    }
}
