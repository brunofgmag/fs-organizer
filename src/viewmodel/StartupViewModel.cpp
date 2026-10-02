#include "viewmodel/StartupViewModel.h"

#include <utility>

StartupViewModel::StartupViewModel(StartupService& service,
                                   StartupEditor& editor,
                                   Session& session,
                                   const Clock& clock,
                                   QObject* parent)
    : QObject(parent), service_(service), editor_(editor), session_(session), clock_(clock)
{
}

void StartupViewModel::Show()
{
    Read();

    emit Changed();
}

bool StartupViewModel::Managing() const
{
    return service_.Managing();
}

void StartupViewModel::Manage(const bool managing)
{
    if (service_.Managing() == managing)
    {
        return;
    }

    const bool written = session_.Rewrite(
        [managing](AppSettings& settings)
        {
            settings.manageStartupEntries = managing;

            return true;
        });

    if (!written)
    {
        emit SettingsCouldNotBeSaved();
        return;
    }

    service_.Manage(managing);
    Read();

    emit Changed();
}

const std::vector<StartupLine>& StartupViewModel::Lines() const
{
    return report_.lines;
}

const std::vector<StartupRemovedEntry>& StartupViewModel::Removed() const
{
    return removed_;
}

std::optional<StartupUndoPlan> StartupViewModel::UndoPlan() const
{
    if (!service_.Managing())
    {
        return std::nullopt;
    }

    return editor_.WhatUndoWouldDo(session_.Profile().id);
}

std::optional<std::chrono::system_clock::time_point> StartupViewModel::ReadAt() const
{
    return readAt_;
}

std::optional<std::string> StartupViewModel::RunningSimulator() const
{
    return service_.RunningSimulator();
}

StartupDraftCheck StartupViewModel::Check(const std::filesystem::path& chosenFile,
                                          const std::optional<std::filesystem::path>& editedEntry) const
{
    return editor_.Check(session_.Profile(), session_.Snapshot(), chosenFile, editedEntry);
}

FileResult StartupViewModel::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    return Settled(editor_.Switch(entryPath, enabled)).result;
}

StartupGestureOutcome StartupViewModel::Add(const StartupDraft& draft)
{
    return Settled(editor_.Add(session_.Profile(), session_.Snapshot(), draft));
}

StartupGestureOutcome StartupViewModel::Edit(const std::filesystem::path& entryPath, const StartupDraft& draft)
{
    return Settled(editor_.Edit(session_.Profile(), session_.Snapshot(), entryPath, draft));
}

StartupGestureOutcome StartupViewModel::Remove(const std::filesystem::path& entryPath)
{
    return Settled(editor_.Remove(session_.Profile().id, entryPath));
}

StartupGestureOutcome StartupViewModel::Restore(const std::filesystem::path& entryPath)
{
    return Settled(editor_.Restore(session_.Profile().id, entryPath));
}

StartupGestureOutcome StartupViewModel::Discard(const std::filesystem::path& entryPath)
{
    return Settled(editor_.Forget(session_.Profile().id, entryPath));
}

std::optional<StartupGestureOutcome> StartupViewModel::Undo()
{
    std::optional<StartupGestureOutcome> outcome = editor_.Undo(session_.Profile().id);

    if (outcome.has_value())
    {
        outcome = Settled(std::move(*outcome));
    }

    return outcome;
}

StartupGestureOutcome StartupViewModel::Settled(StartupGestureOutcome outcome)
{
    Read();
    session_.RefreshStartupEntries();

    if (Succeeded(outcome.result))
    {
        emit Changed();
    }

    return outcome;
}

void StartupViewModel::Read()
{
    if (!service_.Managing())
    {
        report_ = {};
        removed_.clear();
        readAt_.reset();

        return;
    }

    report_ = service_.Report(session_.Profile(), session_.Snapshot());
    removed_ = editor_.Removed();
    readAt_ = clock_.Now();
}
