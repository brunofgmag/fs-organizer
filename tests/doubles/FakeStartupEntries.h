#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H

#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/support/PathUtils.h"

class FakeStartupEntries final : public StartupEntries
{
public:
    std::size_t writes = 0;

    void Carry(StartupEntry entry)
    {
        entries_.push_back(std::move(entry));
    }

    [[nodiscard]] std::vector<StartupEntry> Entries() const override
    {
        return entries_;
    }

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, const bool enabled) override
    {
        ++writes;

        for (StartupEntry& entry : entries_)
        {
            if (ComparablePath(entry.path) == ComparablePath(entryPath))
            {
                entry.enabled = enabled;

                return FileResult::Completed;
            }
        }

        return FileResult::TheDiskDisagreesWithTheScan;
    }

private:
    std::vector<StartupEntry> entries_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
