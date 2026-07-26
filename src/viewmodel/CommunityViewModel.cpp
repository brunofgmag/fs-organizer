#include "viewmodel/CommunityViewModel.h"

#include <algorithm>

CommunityViewModel::CommunityViewModel(ProfileService& service,
                                       AddonTreeModel& treeModel,
                                       CommunityModel& model,
                                       QObject* parent)
    : QObject(parent), service_(service), treeModel_(treeModel), model_(model)
{
}

void CommunityViewModel::Show()
{
    Refresh();
}

std::vector<RepairCandidate> CommunityViewModel::PlanRepairs() const
{
    const ProfileSnapshot& snapshot = treeModel_.Snapshot();

    return ::PlanRepairs(treeModel_.Profile(), snapshot.entries, snapshot.libraries);
}

void CommunityViewModel::Repair(const std::vector<RepairRequest>& requests)
{
    const SimulatorProfile& profile = treeModel_.Profile();
    const std::vector<LinkOperationResult> results = service_.Repair(profile, requests);

    treeModel_.RefreshEnabled(service_.ResolveEntries(profile));
    Refresh();

    emit RepairFinished(results);
}

std::size_t CommunityViewModel::NeedsAttention() const
{
    return attention_;
}

const ProfileSnapshot& CommunityViewModel::Snapshot() const
{
    return treeModel_.Snapshot();
}

void CommunityViewModel::Refresh()
{
    const ProfileSnapshot& snapshot = treeModel_.Snapshot();
    const std::vector<DestinationEntry>& entries = snapshot.entries;

    model_.ShowEntries(entries, treeModel_.Profile(), snapshot.conflicts);

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
