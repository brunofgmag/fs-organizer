#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/support/PathUtils.h"

class FakeStartupEntries final : public StartupEntries
{
public:
    std::size_t writes = 0;
    mutable std::size_t reads = 0;
    std::size_t batchesOpened = 0;
    std::size_t batchesClosed = 0;
    std::size_t switchesInsideABatch = 0;
    std::size_t switchesOutsideABatch = 0;

    void Carry(StartupEntry entry)
    {
        entries_.push_back(std::move(entry));
    }

    [[nodiscard]] std::vector<StartupEntry> Entries() const override
    {
        ++reads;

        return entries_;
    }

    void MakeSwitchingFailWith(const FileResult result)
    {
        refusal_ = result;
    }

    void MakeSwitchingThrow()
    {
        throwing_ = true;
    }

    void OpenBatch() override
    {
        ++batchesOpened;
    }

    void CloseBatch() override
    {
        ++batchesClosed;
    }

    [[nodiscard]] bool BatchIsOpen() const
    {
        return batchesOpened > batchesClosed;
    }

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, const bool enabled) override
    {
        ++(BatchIsOpen() ? switchesInsideABatch : switchesOutsideABatch);

        if (throwing_)
        {
            throw std::runtime_error("the startup file went away");
        }

        if (refusal_ != FileResult::Completed)
        {
            return refusal_;
        }

        for (StartupEntry& entry : entries_)
        {
            if (ComparablePath(entry.path) != ComparablePath(entryPath))
            {
                continue;
            }

            if (entry.enabled != enabled)
            {
                entry.enabled = enabled;
                ++writes;
            }

            return FileResult::Completed;
        }

        return FileResult::TheDiskDisagreesWithTheScan;
    }

private:
    std::vector<StartupEntry> entries_;
    FileResult refusal_ = FileResult::Completed;
    bool throwing_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_STARTUP_ENTRIES_H
