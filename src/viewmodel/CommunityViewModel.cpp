#include "viewmodel/CommunityViewModel.h"

#include <algorithm>

CommunityViewModel::CommunityViewModel(ProfileService& service,
                                       Session& session,
                                       const SessionNotifier& notifier,
                                       CommunityModel& model,
                                       QObject* parent)
    : QObject(parent), service_(service), session_(session), model_(model)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &CommunityViewModel::Show);
}

void CommunityViewModel::Show()
{
    Refresh();
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

    if (breakdown.broken != breakdown_.broken || breakdown.conflicts != breakdown_.conflicts
        || breakdown.duplicated != breakdown_.duplicated || breakdown.unmanaged != breakdown_.unmanaged)
    {
        breakdown_ = breakdown;
        emit BreakdownChanged(breakdown_);
    }
}
