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

std::size_t CommunityViewModel::NeedsAttention() const
{
    return attention_;
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

    const std::size_t attention = snapshot.conflicts.Count()
        + std::ranges::count_if(entries,
                                [](const DestinationEntry& entry)
                                {
                                    return entry.classification == EntryClassification::Broken
                                        || entry.classification == EntryClassification::Duplicated;
                                });

    if (attention != attention_)
    {
        attention_ = attention;
        emit AttentionChanged(attention_);
    }
}
