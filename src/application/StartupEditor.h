#ifndef FS_ORGANIZER_APPLICATION_STARTUP_EDITOR_H
#define FS_ORGANIZER_APPLICATION_STARTUP_EDITOR_H

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "application/StartupService.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/PresetRepository.h"
#include "domain/journal/OperationLog.h"
#include "domain/model/FileResult.h"
#include "domain/model/OperationKind.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/FilesystemProbe.h"

struct StartupDraft
{
    std::string label{};
    std::filesystem::path file{};
    std::string commandLine{};
};

struct StartupDraftCheck
{
    std::filesystem::path pathToWrite{};
    bool insideAnAddon = false;
    std::string addonFolderName{};
    bool addonIsOff = false;
    FileResult refusal = FileResult::Completed;
    std::string occupiedBy{};
    bool changesThePath = false;
    std::size_t presetsNamingTheEntry = 0;
};

struct StartupGestureOutcome
{
    FileResult result = FileResult::Completed;
    std::filesystem::path entryPath{};
    std::vector<std::string> presetsThatFollowed{};
    std::vector<std::string> presetsThatCouldNotBeWritten{};
    bool returnPresetFollowed = false;
    bool returnPresetCouldNotBeWritten = false;
    bool changedNothing = false;
};

enum class StartupUndoEffect : int
{
    RemovesTheAddedEntry = 0,
    RestoresTheRemovedEntry = 1,
    EditsTheEntryBack = 2,
};

struct StartupUndoPlan
{
    StartupUndoEffect effect = StartupUndoEffect::RemovesTheAddedEntry;
    std::string label{};
};

class StartupEditor
{
public:
    StartupEditor(StartupService& startup,
                  PresetRepository& presets,
                  const FilesystemProbe& filesystemProbe,
                  const OperationLog& log);

    [[nodiscard]] StartupDraftCheck Check(const SimulatorProfile& profile,
                                          const ProfileSnapshot& snapshot,
                                          const std::filesystem::path& chosenFile,
                                          const std::optional<std::filesystem::path>& editedEntry) const;

    [[nodiscard]] StartupGestureOutcome
    Add(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const StartupDraft& draft);

    [[nodiscard]] StartupGestureOutcome Edit(const SimulatorProfile& profile,
                                             const ProfileSnapshot& snapshot,
                                             const std::filesystem::path& entryPath,
                                             const StartupDraft& draft);

    [[nodiscard]] StartupGestureOutcome Remove(const std::string& profileId, const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome Restore(const std::string& profileId, const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome Forget(const std::string& profileId, const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome Switch(const std::filesystem::path& entryPath, bool enabled);

    [[nodiscard]] std::optional<StartupUndoPlan> WhatUndoWouldDo(const std::string& profileId) const;

    [[nodiscard]] std::optional<StartupGestureOutcome> Undo(const std::string& profileId);

    [[nodiscard]] std::vector<StartupRemovedEntry> Removed() const;

private:
    struct Undoable
    {
        std::string profileId{};
        StartupUndoEffect effect = StartupUndoEffect::RemovesTheAddedEntry;
        std::filesystem::path path{};
        std::string label{};
        StartupEntry before{};
    };

    struct EditedValues
    {
        std::string label{};
        std::filesystem::path newPath{};
        std::string commandLine{};
    };

    [[nodiscard]] StartupGestureOutcome RemoveEntry(const std::string& profileId,
                                                    const std::filesystem::path& entryPath);

    void
    RememberTheRemoval(const std::string& profileId, const std::filesystem::path& entryPath, const std::string& label);

    [[nodiscard]] StartupGestureOutcome RestoreEntry(const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome
    EditEntry(const std::string& profileId, const std::filesystem::path& entryPath, const EditedValues& values);

    void FollowInThePresets(const std::string& profileId,
                            const std::filesystem::path& from,
                            const std::filesystem::path& to,
                            StartupGestureOutcome& outcome) const;

    [[nodiscard]] std::size_t PresetsNaming(const std::string& profileId, const std::filesystem::path& entryPath) const;

    [[nodiscard]] bool TheFileStillHas(const std::filesystem::path& entryPath) const;

    [[nodiscard]] std::string LabelOfEntry(const std::filesystem::path& entryPath) const;

    [[nodiscard]] std::string LabelOfRemoved(const std::filesystem::path& entryPath) const;

    void Record(OperationKind kind,
                const std::string& label,
                const std::filesystem::path& source,
                const std::filesystem::path& target,
                FileResult result) const;

    void Remember(Undoable undoable);

    void ForgetWhatBelongsToOtherProfilesThan(const std::string& profileId);

    void ForgetTheVersionTheUndoReplaced(const Undoable& undone);

    void ForgetWhatTheEditKeptAtTheOldPath(const Undoable& undone);

    void ForgetTheUndoThatRestores(const std::filesystem::path& entryPath);

    void ForgetTheUndo();

    [[nodiscard]] std::optional<Undoable> PendingFor(const std::string& profileId) const;

    StartupService& startup_;
    PresetRepository& presets_;
    const FilesystemProbe& filesystemProbe_;
    const OperationLog& log_;
    mutable std::mutex guard_;
    std::optional<Undoable> undoable_{};
};

#endif // FS_ORGANIZER_APPLICATION_STARTUP_EDITOR_H
