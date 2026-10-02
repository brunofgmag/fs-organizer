#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/support/PathUtils.h"

class FakeStartupEntries final : public StartupEntries
{
public:
    std::size_t writes = 0;
    mutable std::size_t reads = 0;
    std::size_t switchesThatFoundTheBackupTaken = 0;
    std::size_t switchesThatFoundNoBackup = 0;

    void Carry(StartupEntry entry)
    {
        entries_.push_back(std::move(entry));
    }

    void CarryRemoved(StartupEntry entry,
                      std::filesystem::path followedBy = {},
                      const std::chrono::system_clock::time_point removedAt = {})
    {
        kept_.push_back(Kept{.entry = std::move(entry), .followedBy = std::move(followedBy), .removedAt = removedAt});
    }

    [[nodiscard]] std::vector<StartupEntry> Entries() const override
    {
        ++reads;

        return entries_;
    }

    [[nodiscard]] std::vector<StartupRemovedEntry> Removed() const override
    {
        std::vector<StartupRemovedEntry> removed;

        std::vector<StartupEntry> seen = entries_;

        for (const Kept& kept : kept_ | std::views::reverse)
        {
            if (!IndexOf(seen, kept.entry.path).has_value())
            {
                removed.push_back(StartupRemovedEntry{.entry = kept.entry, .removedAt = kept.removedAt});
                seen.push_back(kept.entry);
            }
        }

        std::ranges::stable_sort(removed, std::ranges::greater{}, &StartupRemovedEntry::removedAt);

        return removed;
    }

    void MakeSwitchingFailWith(const FileResult result)
    {
        refusal_ = result;
    }

    void MakeSwitchingThrow()
    {
        throwing_ = true;
    }

    [[nodiscard]] StartupApplied Apply(const StartupChange& change, StartupBackup& backup) override
    {
        if (std::holds_alternative<StartupSwitching>(change))
        {
            ++(backup.taken ? switchesThatFoundTheBackupTaken : switchesThatFoundNoBackup);
        }

        if (throwing_)
        {
            throw std::runtime_error("the startup file went away");
        }

        if (refusal_ != FileResult::Completed)
        {
            return StartupApplied{.result = refusal_};
        }

        StartupApplied applied = std::visit(
            [this](const auto& what)
            {
                return Perform(what);
            },
            change);

        if (applied.result == FileResult::Completed && wroteTheFile_)
        {
            backup.taken = true;
            ++writes;
        }

        applied.changedNothing = applied.result == FileResult::Completed && !wroteTheFile_
            && !std::holds_alternative<StartupForgetting>(change);

        wroteTheFile_ = false;

        return applied;
    }

private:
    struct Kept
    {
        StartupEntry entry{};
        std::filesystem::path followedBy{};
        std::chrono::system_clock::time_point removedAt{};
    };

    [[nodiscard]] static std::optional<std::size_t> IndexOf(const std::vector<StartupEntry>& entries,
                                                            const std::filesystem::path& path)
    {
        const auto found = std::ranges::find_if(entries,
                                                [&path](const StartupEntry& entry)
                                                {
                                                    return ComparablePath(entry.path) == ComparablePath(path);
                                                });

        return found == entries.end() ? std::nullopt : std::optional<std::size_t>(found - entries.begin());
    }

    [[nodiscard]] std::filesystem::path PathAfter(const std::size_t index) const
    {
        return index + 1 < entries_.size() ? entries_[index + 1].path : std::filesystem::path{};
    }

    [[nodiscard]] StartupApplied Perform(const StartupSwitching& switching)
    {
        const std::optional<std::size_t> at = IndexOf(entries_, switching.path);
        if (!at.has_value())
        {
            return StartupApplied{.result = FileResult::TheDiskDisagreesWithTheScan};
        }

        StartupEntry& entry = entries_[*at];
        if (entry.enabled != switching.enabled)
        {
            entry.enabled = switching.enabled;
            wroteTheFile_ = true;
        }

        return StartupApplied{};
    }

    [[nodiscard]] StartupApplied Perform(const StartupAddition& addition)
    {
        if (IndexOf(entries_, addition.path).has_value())
        {
            return StartupApplied{.result = FileResult::TheStartupEntryIsAlreadyThere};
        }

        entries_.push_back(StartupEntry{
            .label = addition.label, .path = addition.path, .commandLine = addition.commandLine, .enabled = true});
        wroteTheFile_ = true;

        return StartupApplied{};
    }

    [[nodiscard]] StartupApplied Perform(const StartupRemoval& removal)
    {
        const std::optional<std::size_t> at = IndexOf(entries_, removal.path);
        if (!at.has_value())
        {
            return StartupApplied{.result = FileResult::TheDiskDisagreesWithTheScan};
        }

        StartupEntry was = entries_[*at];
        Keep(was, PathAfter(*at), removal.at);
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(*at));
        wroteTheFile_ = true;

        return StartupApplied{.was = std::move(was)};
    }

    [[nodiscard]] StartupApplied Perform(const StartupEditing& editing)
    {
        const std::optional<std::size_t> at = IndexOf(entries_, editing.path);
        if (!at.has_value())
        {
            return StartupApplied{.result = FileResult::TheDiskDisagreesWithTheScan};
        }

        const bool movesTheEntry = ComparablePath(editing.newPath) != ComparablePath(editing.path);
        if (movesTheEntry && IndexOf(entries_, editing.newPath).has_value())
        {
            return StartupApplied{.result = FileResult::TheStartupEntryIsAlreadyThere};
        }

        StartupEntry& entry = entries_[*at];
        StartupEntry was = entry;

        if (movesTheEntry)
        {
            Keep(was, PathAfter(*at), editing.at);
        }

        entry.label = editing.label;
        entry.path = editing.newPath;
        entry.commandLine = editing.commandLine;
        wroteTheFile_ = entry.label != was.label || entry.path != was.path || entry.commandLine != was.commandLine;

        return StartupApplied{.was = std::move(was)};
    }

    [[nodiscard]] StartupApplied Perform(const StartupRestoring& restoring)
    {
        const std::optional<std::size_t> kept = KeptAt(restoring.path);
        if (!kept.has_value())
        {
            return StartupApplied{.result = FileResult::TheDiskDisagreesWithTheScan};
        }

        if (IndexOf(entries_, restoring.path).has_value())
        {
            return StartupApplied{.result = FileResult::TheStartupEntryIsAlreadyThere};
        }

        const Kept restored = kept_[*kept];
        const std::optional<std::size_t> before = IndexOf(entries_, restored.followedBy);

        entries_.insert(before.has_value() ? entries_.begin() + static_cast<std::ptrdiff_t>(*before) : entries_.end(),
                        restored.entry);
        DropKept(restoring.path);
        wroteTheFile_ = true;

        return StartupApplied{};
    }

    [[nodiscard]] StartupApplied Perform(const StartupForgetting& forgetting)
    {
        DropKept(forgetting.path);

        return StartupApplied{};
    }

    void DropKept(const std::filesystem::path& path)
    {
        if (const std::optional<std::size_t> kept = KeptAt(path); kept.has_value())
        {
            kept_.erase(kept_.begin() + static_cast<std::ptrdiff_t>(*kept));
        }
    }

    [[nodiscard]] std::optional<std::size_t> KeptAt(const std::filesystem::path& path) const
    {
        for (std::size_t at = kept_.size(); at > 0; --at)
        {
            if (ComparablePath(kept_[at - 1].entry.path) == ComparablePath(path))
            {
                return at - 1;
            }
        }

        return std::nullopt;
    }

    void Keep(const StartupEntry& entry,
              const std::filesystem::path& followedBy,
              const std::chrono::system_clock::time_point removedAt)
    {
        kept_.push_back(Kept{.entry = entry, .followedBy = followedBy, .removedAt = removedAt});
    }

    std::vector<StartupEntry> entries_;
    std::vector<Kept> kept_;
    FileResult refusal_ = FileResult::Completed;
    bool throwing_ = false;
    bool wroteTheFile_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
