#include "viewmodel/AddonTreeViewModel.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include <QtCore/QThread>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/ToggleDirection.h"

namespace
{
    const SimulatorProfile* ProfileById(const AppSettings& settings, const std::string& id)
    {
        const auto match = std::ranges::find_if(settings.profiles,
                                                [&id](const SimulatorProfile& profile)
                                                {
                                                    return profile.id == id;
                                                });

        return match == settings.profiles.end() ? nullptr : &*match;
    }
}

AddonTreeViewModel::AddonTreeViewModel(ProfileService& service,
                                       SettingsRepository& settings,
                                       const ProcessProbe& probe,
                                       AddonTreeModel& model,
                                       QObject* parent)
    : QObject(parent), service_(service), settings_(settings), probe_(probe), model_(model)
{
}

void AddonTreeViewModel::ShowActiveProfile()
{
    const AppSettings settings = settings_.Load();
    const SimulatorProfile* active = ProfileById(settings, settings.activeProfileId);

    profile_ =
        active != nullptr ? *active : (settings.profiles.empty() ? SimulatorProfile{} : settings.profiles.front());

    StartScan();
}

void AddonTreeViewModel::ChooseProfile(const std::string& profileId)
{
    AppSettings settings = settings_.Load();
    settings.activeProfileId = profileId;
    settings_.Save(settings);

    ShowActiveProfile();
}

void AddonTreeViewModel::StartScan()
{
    if (scan_ != nullptr)
    {
        rescanWhenIdle_ = true;
        return;
    }

    rescanWhenIdle_ = false;
    emit ScanStarted();

    const SimulatorProfile profile = profile_;
    scan_ = QThread::create(
        [this, profile]
        {
            scanned_ = service_.Scan(profile);
        });

    connect(scan_, &QThread::finished, this, &AddonTreeViewModel::AdoptScan);
    scan_->start();
}

void AddonTreeViewModel::AdoptScan()
{
    scan_->deleteLater();
    scan_ = nullptr;

    if (rescanWhenIdle_)
    {
        StartScan();
        return;
    }

    model_.ShowSnapshot(std::move(scanned_), profile_);
    scanned_ = {};

    if (!probe_.SimulatorIsRunning() && restartPending_)
    {
        restartPending_ = false;
        emit RestartPendingChanged(false);
    }

    emit ScanFinished();
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes)
{
    const ProfileSnapshot& snapshot = model_.Snapshot();

    Toggle(nodes, ShouldEnable(profile_, snapshot.entries, snapshot.enabled, nodes));
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    const std::vector<LinkOperationResult> results = service_.SetEnabled(profile_, model_.Snapshot(), nodes, enable);

    ApplyResults(results);
}

void AddonTreeViewModel::UndoLastBatch()
{
    const std::vector<LinkOperationResult> results = service_.UndoLastBatch();

    ApplyResults(results);
}

void AddonTreeViewModel::ApplyResults(const std::vector<LinkOperationResult>& results)
{
    model_.RefreshEnabled(service_.ResolveEntries(profile_));

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

void AddonTreeViewModel::OverrideDestination(const TreeNode* node, const std::filesystem::path& destination)
{
    const Library* library = LibraryContaining(profile_, node->path);
    if (library == nullptr)
    {
        return;
    }

    const std::filesystem::path relative = RelativeToLibrary(*library, node->path);
    const LibraryId libraryId = library->id;

    std::erase_if(profile_.destinationOverrides,
                  [&libraryId, &relative](const DestinationOverride& known)
                  {
                      return known.libraryId == libraryId
                          && ComparablePath(known.relativePath) == ComparablePath(relative);
                  });

    if (!destination.empty())
    {
        profile_.destinationOverrides.push_back({libraryId, relative, destination});
    }

    SaveProfile();
    model_.ShowProfile(profile_);
}

LibraryReport AddonTreeViewModel::AddLibrary(const std::filesystem::path& path)
{
    const LibraryReport report = service_.RegisterLibrary(profile_, path);

    if (report.Accepted())
    {
        SaveProfile();
        StartScan();
    }

    return report;
}

void AddonTreeViewModel::SaveProfile() const
{
    AppSettings settings = settings_.Load();

    for (SimulatorProfile& stored : settings.profiles)
    {
        if (stored.id == profile_.id)
        {
            stored = profile_;
        }
    }

    settings_.Save(settings);
}

bool AddonTreeViewModel::CanUndo() const
{
    return service_.CanUndo();
}

const SimulatorProfile& AddonTreeViewModel::Profile() const
{
    return profile_;
}
