#include "infrastructure/sim/ExeXmlStartupEntries.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlDocument.h"
#include "infrastructure/sim/RemovedStartupEntriesFile.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "support/FileWriting.h"

namespace
{
    constexpr auto kUserConfigName = "UserCfg.opt";

    enum class Reading : int
    {
        Read = 0,
        NoSuchFile = 1,
        Failed = 2,
    };

    struct StartupFileBytes
    {
        Reading reading = Reading::Failed;
        std::string bytes{};
    };

    [[nodiscard]] StartupFileBytes ReadStartupFile(const std::filesystem::path& file)
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(file, error);

        if (error)
        {
            return {};
        }

        if (!exists)
        {
            return {.reading = Reading::NoSuchFile};
        }

        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return {};
        }

        return {.reading = Reading::Read,
                .bytes = std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>())};
    }

    [[nodiscard]] std::vector<StartupEntry> EntriesOfFile(const std::filesystem::path& file)
    {
        const StartupFileBytes startup = ReadStartupFile(file);

        return startup.reading == Reading::Read ? StartupEntriesIn(startup.bytes) : std::vector<StartupEntry>{};
    }

    [[nodiscard]] bool UserConfigIsBeside(const std::filesystem::path& file)
    {
        if (file.empty())
        {
            return false;
        }

        std::error_code error;

        return std::filesystem::exists(file.parent_path() / kUserConfigName, error);
    }

    struct DocumentToChange
    {
        FileResult refusal = FileResult::Completed;
        std::string bytes{};
        bool existed = false;
    };

    [[nodiscard]] DocumentToChange DocumentToChangeFor(const std::filesystem::path& file, const StartupChange& change)
    {
        const StartupFileBytes startup = ReadStartupFile(file);
        const bool existed = startup.reading == Reading::Read;
        const bool mayCreateIt = std::holds_alternative<StartupAddition>(change) && UserConfigIsBeside(file);

        if (startup.reading == Reading::Failed || (!existed && !mayCreateIt))
        {
            return {.refusal = FileResult::CouldNotReadTheStartupFile};
        }

        if (existed && StartupDocumentIsUtf16(startup.bytes))
        {
            return {.refusal = FileResult::TheStartupFileIsNotUtf8};
        }

        return {.bytes = existed ? startup.bytes : NewStartupDocument(), .existed = existed};
    }

    [[nodiscard]] std::chrono::system_clock::time_point InstantOf(const StartupChange& change)
    {
        if (const auto* removal = std::get_if<StartupRemoval>(&change))
        {
            return removal->at;
        }

        if (const auto* editing = std::get_if<StartupEditing>(&change))
        {
            return editing->at;
        }

        return {};
    }

    [[nodiscard]] std::optional<RemovedStartupEntry> KeptAt(const RemovedStartupEntriesFile& removed,
                                                            const std::filesystem::path& entryPath)
    {
        const std::string wanted = ComparablePath(entryPath);
        const std::vector<RemovedStartupEntry> inTheKeeping = removed.Entries();

        for (const RemovedStartupEntry& kept : inTheKeeping | std::views::reverse)
        {
            if (ComparablePath(kept.path) == wanted)
            {
                return kept;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] bool SameEntry(const StartupEntry& left, const StartupEntry& right)
    {
        return left.label == right.label && ComparablePath(left.path) == ComparablePath(right.path)
            && left.commandLine == right.commandLine && left.enabled == right.enabled;
    }

    [[nodiscard]] std::optional<std::size_t> IndexOfPath(const std::vector<StartupEntry>& entries,
                                                         const std::filesystem::path& entryPath)
    {
        const std::string wanted = ComparablePath(entryPath);

        for (std::size_t at = 0; at < entries.size(); ++at)
        {
            if (!wanted.empty() && ComparablePath(entries[at].path) == wanted)
            {
                return at;
            }
        }

        return std::nullopt;
    }

    struct ExpectedEntries
    {
        std::vector<StartupEntry> entries{};
        bool inAnyOrder = false;
    };

    [[nodiscard]] std::optional<std::filesystem::path> PathTheChangeNames(const StartupChange& change)
    {
        if (const auto* switching = std::get_if<StartupSwitching>(&change))
        {
            return switching->path;
        }

        if (const auto* removal = std::get_if<StartupRemoval>(&change))
        {
            return removal->path;
        }

        if (const auto* editing = std::get_if<StartupEditing>(&change))
        {
            return editing->path;
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<ExpectedEntries> ExpectedAfterAnAppending(std::vector<StartupEntry> entries,
                                                                          const StartupChange& change,
                                                                          const RemovedStartupEntriesFile& removed)
    {
        if (const auto* addition = std::get_if<StartupAddition>(&change))
        {
            entries.push_back(StartupEntry{.label = addition->label,
                                           .path = addition->path,
                                           .commandLine = addition->commandLine,
                                           .enabled = true});

            return ExpectedEntries{.entries = std::move(entries)};
        }

        const std::optional<RemovedStartupEntry> kept = KeptAt(removed, std::get<StartupRestoring>(change).path);

        if (!kept.has_value())
        {
            return std::nullopt;
        }

        entries.push_back(StartupEntry{
            .label = kept->label, .path = kept->path, .commandLine = kept->commandLine, .enabled = kept->enabled});

        return ExpectedEntries{.entries = std::move(entries), .inAnyOrder = true};
    }

    [[nodiscard]] std::optional<ExpectedEntries> ExpectedAfter(std::vector<StartupEntry> entries,
                                                               const StartupChange& change,
                                                               const RemovedStartupEntriesFile& removed)
    {
        const std::optional<std::filesystem::path> named = PathTheChangeNames(change);

        if (!named.has_value())
        {
            return ExpectedAfterAnAppending(std::move(entries), change, removed);
        }

        const std::optional<std::size_t> at = IndexOfPath(entries, *named);

        if (!at.has_value())
        {
            return std::nullopt;
        }

        if (const auto* switching = std::get_if<StartupSwitching>(&change))
        {
            entries[*at].enabled = switching->enabled;
        }
        else if (const auto* editing = std::get_if<StartupEditing>(&change))
        {
            entries[*at].label = editing->label;
            entries[*at].path = editing->newPath;
            entries[*at].commandLine = editing->commandLine;
        }
        else
        {
            entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(*at));
        }

        return ExpectedEntries{.entries = std::move(entries)};
    }

    [[nodiscard]] bool TheEntriesAreWhatTheChangeAsked(const std::string& before,
                                                       const std::string& after,
                                                       const StartupChange& change,
                                                       const RemovedStartupEntriesFile& removed)
    {
        const std::optional<ExpectedEntries> expected = ExpectedAfter(StartupEntriesIn(before), change, removed);

        if (!expected.has_value())
        {
            return false;
        }

        const std::vector<StartupEntry> found = StartupEntriesIn(after);

        return expected->inAnyOrder ? std::ranges::is_permutation(found, expected->entries, SameEntry)
                                    : std::ranges::equal(found, expected->entries, SameEntry);
    }

    [[nodiscard]] bool BackedUp(const std::filesystem::path& file, const std::string& before, StartupBackup& backup)
    {
        if (backup.taken)
        {
            return true;
        }

        backup.taken = WriteFileReplacing(BackupOfStartupFile(file), before);

        return backup.taken;
    }

    [[nodiscard]] StartupDocumentChange
    Restored(const std::string& before, const StartupRestoring& restoring, const RemovedStartupEntriesFile& removed)
    {
        const std::optional<RemovedStartupEntry> kept = KeptAt(removed, restoring.path);

        return kept.has_value() ? WithStartupEntryRestored(before, *kept)
                                : StartupDocumentChange{.result = FileResult::TheDiskDisagreesWithTheScan};
    }

    [[nodiscard]] StartupDocumentChange
    Rewritten(const std::string& before, const StartupChange& change, const RemovedStartupEntriesFile& removed)
    {
        if (const auto* switching = std::get_if<StartupSwitching>(&change))
        {
            std::optional<std::string> after = WithStartupEntrySwitched(before, switching->path, switching->enabled);

            return after.has_value() ? StartupDocumentChange{.document = std::move(*after)}
                                     : StartupDocumentChange{.result = FileResult::TheDiskDisagreesWithTheScan};
        }

        if (const auto* addition = std::get_if<StartupAddition>(&change))
        {
            return WithStartupEntryAdded(before, *addition);
        }

        if (const auto* removal = std::get_if<StartupRemoval>(&change))
        {
            return WithStartupEntryRemoved(before, removal->path);
        }

        if (const auto* editing = std::get_if<StartupEditing>(&change))
        {
            return WithStartupEntryEdited(before, *editing);
        }

        return Restored(before, std::get<StartupRestoring>(change), removed);
    }
}

ExeXmlStartupEntries::ExeXmlStartupEntries(std::filesystem::path filePath) : filePath_(std::move(filePath))
{
}

void ExeXmlStartupEntries::Use(std::filesystem::path filePath)
{
    const std::lock_guard lock(guard_);

    filePath_ = std::move(filePath);
}

void ExeXmlStartupEntries::KeepRemovedEntriesIn(std::filesystem::path removedFilePath)
{
    const std::lock_guard lock(guard_);

    removedFilePath_ = std::move(removedFilePath);
}

ExeXmlStartupEntries::FilesInUse ExeXmlStartupEntries::Files() const
{
    const std::lock_guard lock(guard_);

    return FilesInUse{.startup = filePath_, .removed = removedFilePath_};
}

std::vector<StartupEntry> ExeXmlStartupEntries::Entries() const
{
    return EntriesOfFile(Files().startup);
}

std::vector<StartupRemovedEntry> ExeXmlStartupEntries::Removed() const
{
    const FilesInUse files = Files();

    std::vector<std::string> notListed;
    for (const StartupEntry& entry : EntriesOfFile(files.startup))
    {
        notListed.push_back(ComparablePath(entry.path));
    }

    std::vector<StartupRemovedEntry> removed;

    const std::vector<RemovedStartupEntry> inTheKeeping = RemovedStartupEntriesFile(files.removed).Entries();

    for (const RemovedStartupEntry& kept : inTheKeeping | std::views::reverse)
    {
        const std::string comparable = ComparablePath(kept.path);

        if (std::ranges::find(notListed, comparable) != notListed.end())
        {
            continue;
        }

        notListed.push_back(comparable);
        removed.push_back(StartupRemovedEntry{.entry = StartupEntry{.label = kept.label,
                                                                    .path = kept.path,
                                                                    .commandLine = kept.commandLine,
                                                                    .enabled = kept.enabled},
                                              .removedAt = kept.removedAt});
    }

    std::ranges::stable_sort(removed, std::ranges::greater{}, &StartupRemovedEntry::removedAt);

    return removed;
}

StartupApplied ExeXmlStartupEntries::Apply(const StartupChange& change, StartupBackup& backup)
{
    const std::lock_guard applying(applying_);

    const FilesInUse files = Files();
    const RemovedStartupEntriesFile removed(files.removed);

    if (const auto* forgetting = std::get_if<StartupForgetting>(&change))
    {
        return StartupApplied{.result = removed.Drop(forgetting->path) ? FileResult::Completed
                                                                       : FileResult::CouldNotKeepTheRemovedEntry};
    }

    const DocumentToChange document = DocumentToChangeFor(files.startup, change);

    if (document.refusal != FileResult::Completed)
    {
        return StartupApplied{.result = document.refusal};
    }

    const std::string& before = document.bytes;

    StartupDocumentChange changed = Rewritten(before, change, removed);
    if (changed.result != FileResult::Completed)
    {
        return StartupApplied{.result = changed.result};
    }

    if (changed.document == before)
    {
        return StartupApplied{.result = FileResult::Completed, .was = std::move(changed.was), .changedNothing = true};
    }

    if (!TheEntriesAreWhatTheChangeAsked(before, changed.document, change, removed))
    {
        return StartupApplied{.result = FileResult::CouldNotWriteTheStartupFile};
    }

    if (changed.taken.has_value())
    {
        changed.taken->removedAt = InstantOf(change);

        if (!removed.Keep(*changed.taken))
        {
            return StartupApplied{.result = FileResult::CouldNotKeepTheRemovedEntry};
        }
    }

    if (document.existed && !BackedUp(files.startup, before, backup))
    {
        return StartupApplied{.result = FileResult::CouldNotWriteTheStartupFile};
    }

    if (!WriteFileReplacing(files.startup, changed.document))
    {
        return StartupApplied{.result = FileResult::CouldNotWriteTheStartupFile};
    }

    if (const auto* restoring = std::get_if<StartupRestoring>(&change))
    {
        static_cast<void>(removed.Drop(restoring->path));
    }

    return StartupApplied{.result = FileResult::Completed, .was = std::move(changed.was)};
}
