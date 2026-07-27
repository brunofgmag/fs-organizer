#include "viewmodel/AddonTreeViewModel.h"

#include <algorithm>

#include "domain/tree/ToggleDirection.h"

AddonTreeViewModel::AddonTreeViewModel(Session& session,
                                       ProfileService& service,
                                       const ProcessProbe& probe,
                                       AddonTreeModel& model,
                                       const SessionNotifier& notifier,
                                       QObject* parent)
    : QObject(parent), session_(session), service_(service), probe_(probe), model_(model)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &AddonTreeViewModel::AdoptScan);

    connect(&notifier, &SessionNotifier::Refreshed, this,
            [this]
            {
                model_.Refresh(session_.Snapshot(), session_.Profile());
            });
}

void AddonTreeViewModel::ShowActiveProfile() const
{
    session_.ShowActiveProfile();
}

void AddonTreeViewModel::ChooseProfile(const std::string& profileId) const
{
    session_.ChooseProfile(profileId);
}

void AddonTreeViewModel::AdoptScan()
{
    model_.Show(session_.Snapshot(), session_.Profile());

    if (!probe_.SimulatorIsRunning() && restartPending_)
    {
        restartPending_ = false;
        emit RestartPendingChanged(false);
    }

    emit Shown();
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes)
{
    const ProfileSnapshot& snapshot = session_.Snapshot();

    Toggle(nodes, ShouldEnable(session_.Profile(), snapshot.entries, snapshot.enabled, nodes));
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    const std::vector<LinkOperationResult> results =
        service_.SetEnabled(session_.Profile(), session_.Snapshot(), nodes, enable);

    ApplyResults(results);
}

void AddonTreeViewModel::UndoLastBatch()
{
    const std::vector<LinkOperationResult> results = service_.UndoLastBatch();

    ApplyResults(results);
}

void AddonTreeViewModel::ApplyResults(const std::vector<LinkOperationResult>& results)
{
    session_.RefreshEntries();

    NoteSimulatorState(results);

    emit BatchFinished(results);
}

void AddonTreeViewModel::NoteSimulatorState(const std::vector<LinkOperationResult>& results)
{
    const bool changed = std::ranges::any_of(results,
                                             [](const LinkOperationResult& result)
                                             {
                                                 return result.outcome.Succeeded();
                                             });

    if (!changed || !probe_.SimulatorIsRunning())
    {
        return;
    }

    if (!warnedAboutSimulator_)
    {
        warnedAboutSimulator_ = true;
        emit SimulatorIsRunning();
    }

    if (!restartPending_)
    {
        restartPending_ = true;
        emit RestartPendingChanged(true);
    }
}

void AddonTreeViewModel::OverrideDestination(const TreeNode* node, const std::filesystem::path& destination) const
{
    session_.OverrideDestination(*node, destination);
}

LibraryReport AddonTreeViewModel::AddLibrary(const std::filesystem::path& path) const
{
    return session_.RegisterLibrary(path);
}

bool AddonTreeViewModel::CanUndo() const
{
    return service_.CanUndo();
}

const SimulatorProfile& AddonTreeViewModel::Profile() const
{
    return session_.Profile();
}
