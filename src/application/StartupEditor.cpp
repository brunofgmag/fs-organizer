#include "application/StartupEditor.h"

#include <algorithm>
#include <utility>

#include "domain/model/AddonId.h"
#include "domain/model/Preset.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"

namespace
{
    bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        return ComparablePath(left) == ComparablePath(right);
    }

    bool NamesThePath(const Preset& preset, const std::filesystem::path& entryPath)
    {
        return std::ranges::any_of(preset.startupEntries,
                                   [&entryPath](const PresetStartupEntry& entry)
                                   {
                                       return SamePath(entry.path, entryPath);
                                   });
    }

    bool Retargeted(Preset& preset, const std::filesystem::path& from, const std::filesystem::path& to)
    {
        if (SamePath(from, to) || !NamesThePath(preset, from))
        {
            return false;
        }

        const bool alreadyNamesTheTarget = NamesThePath(preset, to);

        if (alreadyNamesTheTarget)
        {
            std::erase_if(preset.startupEntries,
                          [&from](const PresetStartupEntry& entry)
                          {
                              return SamePath(entry.path, from);
                          });

            return true;
        }

        for (PresetStartupEntry& entry : preset.startupEntries)
        {
            if (SamePath(entry.path, from))
            {
                entry.path = to;
            }
        }

        return true;
    }

    std::filesystem::path InWindowsForm(std::filesystem::path path)
    {
        return path.make_preferred();
    }

    bool NeverChangesItsMind(const FileResult result)
    {
        return result == FileResult::TheStartupEntryIsAlreadyThere || result == FileResult::TheDiskDisagreesWithTheScan;
    }

    const TreeNode* AddonHolding(const std::vector<TreeNode>& libraries, const std::filesystem::path& file)
    {
        for (const TreeNode& library : libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                if (PathIsInside(file, addon->path))
                {
                    return addon;
                }
            }
        }

        return nullptr;
    }

    std::filesystem::path
    PathOfTheLinkTo(const SimulatorProfile& profile, const TreeNode& addon, const std::filesystem::path& file)
    {
        const std::filesystem::path link = PlannedLinkPath(profile, addon.path);
        const std::filesystem::path inside = TailBelow(file, PartsIn(addon.path));

        return inside.empty() ? link : PathUnder(link, inside);
    }
}

StartupEditor::StartupEditor(StartupService& startup,
                             PresetRepository& presets,
                             const FilesystemProbe& filesystemProbe,
                             const OperationLog& log)
    : startup_(startup), presets_(presets), filesystemProbe_(filesystemProbe), log_(log)
{
}

StartupDraftCheck StartupEditor::Check(const SimulatorProfile& profile,
                                       const ProfileSnapshot& snapshot,
                                       const std::filesystem::path& chosenFile,
                                       const std::optional<std::filesystem::path>& editedEntry) const
{
    StartupDraftCheck check;
    const std::filesystem::path file = InWindowsForm(chosenFile);
    const bool keepsTheEntryPath = editedEntry.has_value() && SamePath(*editedEntry, file);

    check.pathToWrite = keepsTheEntryPath ? *editedEntry : file;

    if (const TreeNode* addon = keepsTheEntryPath ? nullptr : AddonHolding(snapshot.libraries, file); addon != nullptr)
    {
        check.pathToWrite = InWindowsForm(PathOfTheLinkTo(profile, *addon, file));
        check.insideAnAddon = true;
        check.addonFolderName = AsUtf8(addon->path.filename());
        check.addonIsOff = !snapshot.enabled.Contains(addon->path);
    }

    check.changesThePath = editedEntry.has_value() && !SamePath(*editedEntry, check.pathToWrite);

    if (editedEntry.has_value())
    {
        check.presetsNamingTheEntry = PresetsNaming(profile.id, *editedEntry);
    }

    const bool isTheEntryItself = editedEntry.has_value() && !check.changesThePath;

    if (!isTheEntryItself)
    {
        for (const StartupEntry& entry : startup_.Entries())
        {
            if (SamePath(entry.path, check.pathToWrite))
            {
                check.refusal = FileResult::TheStartupEntryIsAlreadyThere;
                check.occupiedBy = entry.label;

                return check;
            }
        }
    }

    const bool needsTheProgram = !editedEntry.has_value() || check.changesThePath;

    if (needsTheProgram && !filesystemProbe_.EntryExistsWithoutFollowingLinks(file))
    {
        check.refusal = FileResult::TheProgramDoesNotExist;
    }

    return check;
}

StartupGestureOutcome
StartupEditor::Add(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const StartupDraft& draft)
{
    ForgetWhatBelongsToOtherProfilesThan(profile.id);

    const StartupDraftCheck check = Check(profile, snapshot, draft.file, std::nullopt);

    if (check.refusal != FileResult::Completed)
    {
        Record(OperationKind::AddTheStartupEntry, draft.label, {}, check.pathToWrite, check.refusal);

        return StartupGestureOutcome{.result = check.refusal, .entryPath = check.pathToWrite};
    }

    StartupBackup backup;
    const StartupApplied applied = startup_.Apply(
        StartupAddition{.label = draft.label, .path = check.pathToWrite, .commandLine = draft.commandLine}, backup);

    Record(OperationKind::AddTheStartupEntry, draft.label, {}, check.pathToWrite, applied.result);

    if (applied.result == FileResult::Completed)
    {
        Remember(Undoable{.profileId = profile.id,
                          .effect = StartupUndoEffect::RemovesTheAddedEntry,
                          .path = check.pathToWrite,
                          .label = draft.label,
                          .before = {}});
    }

    return StartupGestureOutcome{.result = applied.result, .entryPath = check.pathToWrite};
}

StartupGestureOutcome StartupEditor::Edit(const SimulatorProfile& profile,
                                          const ProfileSnapshot& snapshot,
                                          const std::filesystem::path& entryPath,
                                          const StartupDraft& draft)
{
    ForgetWhatBelongsToOtherProfilesThan(profile.id);

    const StartupDraftCheck check = Check(profile, snapshot, draft.file, entryPath);

    if (check.refusal != FileResult::Completed)
    {
        Record(OperationKind::EditTheStartupEntry, draft.label, entryPath, check.pathToWrite, check.refusal);

        return StartupGestureOutcome{.result = check.refusal, .entryPath = entryPath};
    }

    return EditEntry(
        profile.id, entryPath,
        EditedValues{.label = draft.label, .newPath = check.pathToWrite, .commandLine = draft.commandLine});
}

StartupGestureOutcome StartupEditor::Remove(const std::string& profileId, const std::filesystem::path& entryPath)
{
    ForgetWhatBelongsToOtherProfilesThan(profileId);

    return RemoveEntry(profileId, entryPath);
}

StartupGestureOutcome StartupEditor::Restore(const std::string& profileId, const std::filesystem::path& entryPath)
{
    ForgetWhatBelongsToOtherProfilesThan(profileId);

    const StartupGestureOutcome outcome = RestoreEntry(entryPath);

    if (outcome.result == FileResult::Completed)
    {
        ForgetTheUndoThatRestores(entryPath);
    }

    return outcome;
}

StartupGestureOutcome StartupEditor::Forget(const std::string& profileId, const std::filesystem::path& entryPath)
{
    ForgetWhatBelongsToOtherProfilesThan(profileId);

    const std::string label = LabelOfRemoved(entryPath);
    StartupBackup backup;
    const StartupApplied applied = startup_.Apply(StartupForgetting{.path = entryPath}, backup);

    Record(OperationKind::ForgetTheStartupEntry, label, {}, entryPath, applied.result);

    if (applied.result == FileResult::Completed)
    {
        ForgetTheUndoThatRestores(entryPath);
    }

    return StartupGestureOutcome{.result = applied.result, .entryPath = entryPath};
}

StartupGestureOutcome StartupEditor::Switch(const std::filesystem::path& entryPath, const bool enabled)
{
    const std::string label = LabelOfEntry(entryPath);
    const FileResult result = startup_.Switch(entryPath, enabled);

    Record(enabled ? OperationKind::TurnOnTheStartupEntry : OperationKind::TurnOffTheStartupEntry, label, {}, entryPath,
           result);

    return StartupGestureOutcome{.result = result, .entryPath = entryPath};
}

std::optional<StartupUndoPlan> StartupEditor::WhatUndoWouldDo(const std::string& profileId) const
{
    const std::optional<Undoable> pending = PendingFor(profileId);

    if (!pending.has_value())
    {
        return std::nullopt;
    }

    return StartupUndoPlan{.effect = pending->effect, .label = pending->label};
}

std::optional<StartupGestureOutcome> StartupEditor::Undo(const std::string& profileId)
{
    const std::optional<Undoable> pending = PendingFor(profileId);

    if (!pending.has_value())
    {
        return std::nullopt;
    }

    StartupGestureOutcome outcome;

    switch (pending->effect)
    {
    case StartupUndoEffect::RemovesTheAddedEntry: outcome = RemoveEntry(profileId, pending->path); break;
    case StartupUndoEffect::RestoresTheRemovedEntry: outcome = RestoreEntry(pending->path); break;
    case StartupUndoEffect::EditsTheEntryBack:
        outcome = EditEntry(profileId, pending->path,
                            EditedValues{.label = pending->before.label,
                                         .newPath = pending->before.path,
                                         .commandLine = pending->before.commandLine});
        break;
    }

    if (outcome.result == FileResult::Completed)
    {
        ForgetTheUndo();

        if (pending->effect == StartupUndoEffect::EditsTheEntryBack && !SamePath(pending->before.path, pending->path))
        {
            ForgetTheVersionTheUndoReplaced(*pending);
            ForgetWhatTheEditKeptAtTheOldPath(*pending);
        }
    }
    else if (NeverChangesItsMind(outcome.result))
    {
        ForgetTheUndo();
    }

    return outcome;
}

std::vector<StartupRemovedEntry> StartupEditor::Removed() const
{
    return startup_.Removed();
}

StartupGestureOutcome StartupEditor::RemoveEntry(const std::string& profileId, const std::filesystem::path& entryPath)
{
    const std::string known = LabelOfEntry(entryPath);
    StartupBackup backup;
    const StartupApplied applied = startup_.Apply(StartupRemoval{.path = entryPath, .at = log_.Now()}, backup);
    const std::string label = applied.was.has_value() ? applied.was->label : known;

    Record(OperationKind::RemoveTheStartupEntry, label, {}, entryPath, applied.result);

    if (applied.result == FileResult::Completed)
    {
        RememberTheRemoval(profileId, entryPath, label);
    }

    return StartupGestureOutcome{.result = applied.result, .entryPath = entryPath};
}

void StartupEditor::RememberTheRemoval(const std::string& profileId,
                                       const std::filesystem::path& entryPath,
                                       const std::string& label)
{
    if (!TheFileStillHas(entryPath))
    {
        Remember(Undoable{.profileId = profileId,
                          .effect = StartupUndoEffect::RestoresTheRemovedEntry,
                          .path = entryPath,
                          .label = label,
                          .before = {}});

        return;
    }

    ForgetTheUndo();
}

StartupGestureOutcome StartupEditor::RestoreEntry(const std::filesystem::path& entryPath)
{
    const std::string label = LabelOfRemoved(entryPath);
    StartupBackup backup;
    const StartupApplied applied = startup_.Apply(StartupRestoring{.path = entryPath}, backup);

    Record(OperationKind::RestoreTheStartupEntry, label, {}, entryPath, applied.result);

    return StartupGestureOutcome{.result = applied.result, .entryPath = entryPath};
}

StartupGestureOutcome StartupEditor::EditEntry(const std::string& profileId,
                                               const std::filesystem::path& entryPath,
                                               const EditedValues& values)
{
    StartupBackup backup;
    const StartupApplied applied = startup_.Apply(StartupEditing{.path = entryPath,
                                                                 .label = values.label,
                                                                 .newPath = values.newPath,
                                                                 .commandLine = values.commandLine,
                                                                 .at = log_.Now()},
                                                  backup);

    if (applied.result == FileResult::Completed && applied.changedNothing)
    {
        return StartupGestureOutcome{.result = applied.result, .entryPath = values.newPath, .changedNothing = true};
    }

    const bool movesTheEntry = !SamePath(entryPath, values.newPath);

    Record(OperationKind::EditTheStartupEntry, values.label, movesTheEntry ? entryPath : std::filesystem::path{},
           values.newPath, applied.result);

    StartupGestureOutcome outcome{.result = applied.result, .entryPath = values.newPath};

    if (applied.result != FileResult::Completed)
    {
        return outcome;
    }

    if (movesTheEntry)
    {
        FollowInThePresets(profileId, entryPath, values.newPath, outcome);
    }

    if (applied.was.has_value())
    {
        Remember(Undoable{.profileId = profileId,
                          .effect = StartupUndoEffect::EditsTheEntryBack,
                          .path = values.newPath,
                          .label = values.label,
                          .before = *applied.was});
    }

    return outcome;
}

void StartupEditor::FollowInThePresets(const std::string& profileId,
                                       const std::filesystem::path& from,
                                       const std::filesystem::path& to,
                                       StartupGestureOutcome& outcome) const
{
    for (const PresetListing& listing : presets_.List(profileId))
    {
        std::optional<Preset> preset = presets_.Load(profileId, listing.name);

        if (!preset.has_value() || !Retargeted(*preset, from, to))
        {
            continue;
        }

        (presets_.Save(profileId, *preset) ? outcome.presetsThatFollowed : outcome.presetsThatCouldNotBeWritten)
            .push_back(listing.name);
    }

    std::optional<Preset> theReturn = presets_.LoadReturnPreset(profileId);

    if (!theReturn.has_value() || !Retargeted(*theReturn, from, to))
    {
        return;
    }

    const bool saved = presets_.SaveReturnPreset(profileId, *theReturn);

    outcome.returnPresetFollowed = saved;
    outcome.returnPresetCouldNotBeWritten = !saved;
}

std::size_t StartupEditor::PresetsNaming(const std::string& profileId, const std::filesystem::path& entryPath) const
{
    std::size_t naming = 0;

    for (const PresetListing& listing : presets_.List(profileId))
    {
        const std::optional<Preset> preset = presets_.Load(profileId, listing.name);

        if (preset.has_value() && NamesThePath(*preset, entryPath))
        {
            ++naming;
        }
    }

    return naming;
}

bool StartupEditor::TheFileStillHas(const std::filesystem::path& entryPath) const
{
    return std::ranges::any_of(startup_.Entries(),
                               [&entryPath](const StartupEntry& entry)
                               {
                                   return SamePath(entry.path, entryPath);
                               });
}

std::string StartupEditor::LabelOfEntry(const std::filesystem::path& entryPath) const
{
    for (const StartupEntry& entry : startup_.Entries())
    {
        if (SamePath(entry.path, entryPath))
        {
            return entry.label;
        }
    }

    return {};
}

std::string StartupEditor::LabelOfRemoved(const std::filesystem::path& entryPath) const
{
    for (const StartupRemovedEntry& removed : startup_.Removed())
    {
        if (SamePath(removed.entry.path, entryPath))
        {
            return removed.entry.label;
        }
    }

    return {};
}

void StartupEditor::Record(const OperationKind kind,
                           const std::string& label,
                           const std::filesystem::path& source,
                           const std::filesystem::path& target,
                           const FileResult result) const
{
    log_.RecordImport(kind, AddonId{}, source, target, result, OriginSource::Unknown, label);
}

void StartupEditor::Remember(Undoable undoable)
{
    const std::lock_guard lock(guard_);

    undoable_ = std::move(undoable);
}

void StartupEditor::ForgetWhatBelongsToOtherProfilesThan(const std::string& profileId)
{
    const std::lock_guard lock(guard_);

    if (undoable_.has_value() && undoable_->profileId != profileId)
    {
        undoable_.reset();
    }
}

void StartupEditor::ForgetTheVersionTheUndoReplaced(const Undoable& undone)
{
    StartupBackup backup;

    static_cast<void>(startup_.Apply(StartupForgetting{.path = undone.path}, backup));
}

void StartupEditor::ForgetWhatTheEditKeptAtTheOldPath(const Undoable& undone)
{
    StartupBackup backup;

    static_cast<void>(startup_.Apply(StartupForgetting{.path = undone.before.path}, backup));
}

void StartupEditor::ForgetTheUndoThatRestores(const std::filesystem::path& entryPath)
{
    const std::lock_guard lock(guard_);

    if (undoable_.has_value() && undoable_->effect == StartupUndoEffect::RestoresTheRemovedEntry
        && SamePath(undoable_->path, entryPath))
    {
        undoable_.reset();
    }
}

void StartupEditor::ForgetTheUndo()
{
    const std::lock_guard lock(guard_);

    undoable_.reset();
}

std::optional<StartupEditor::Undoable> StartupEditor::PendingFor(const std::string& profileId) const
{
    const std::lock_guard lock(guard_);

    if (undoable_.has_value() && undoable_->profileId == profileId)
    {
        return undoable_;
    }

    return std::nullopt;
}
