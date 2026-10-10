#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_ARCHIVE_READER_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_ARCHIVE_READER_H

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "domain/ports/ArchiveReader.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/InMemoryFileSystem.h"

class FakeArchiveReader final : public ArchiveReader
{
public:
    explicit FakeArchiveReader(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
    {
    }

    void Lists(std::vector<ArchiveEntry> entries)
    {
        entries_ = std::move(entries);
    }

    void ListsFor(const std::filesystem::path& archive, std::vector<ArchiveEntry> entries)
    {
        entriesByOrigin_[archive.generic_string()] = std::move(entries);
    }

    void FailsToList(const ArchiveFailure failure)
    {
        listingFails_ = failure;
    }

    void Holds(const std::string& entry, std::string text)
    {
        contents_[entry] = std::move(text);
    }

    void HoldsFor(const std::filesystem::path& archive, const std::string& entry, std::string text)
    {
        contentsByOrigin_[archive.generic_string()][entry] = std::move(text);
    }

    void FailsToRead(const std::string& entry, const ArchiveFailure failure)
    {
        unreadable_[entry] = failure;
    }

    void FailsToExtract(const ArchiveFailure failure)
    {
        extractionFails_ = failure;
    }

    void FailsAfterWritingEverything(const ArchiveFailure failure)
    {
        failureAfterWriting_ = failure;
    }

    void StopsAfterWriting(const std::size_t files)
    {
        stopsAfter_ = files;
    }

    void WritesTheWrongSizeFor(const std::string& entry)
    {
        wrongSize_.push_back(entry);
    }

    void LeavesOut(const std::string& entry)
    {
        left_.push_back(entry);
    }

    void MergesFoldersThatDifferOnlyInCase()
    {
        mergesFolders_ = true;
    }

    [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& archive) const override
    {
        listed.push_back(archive);

        if (listingFails_ != ArchiveFailure::None)
        {
            return ArchiveListing{.failure = listingFails_};
        }

        const auto own = entriesByOrigin_.find(archive.generic_string());

        return ArchiveListing{.entries = own == entriesByOrigin_.end() ? entries_ : own->second};
    }

    [[nodiscard]] ArchiveText
    ContentsOf(const std::filesystem::path& archive, const std::string& entry, const std::size_t most) const override
    {
        read.push_back(ReadAsked{.entry = entry, .most = most});

        if (const auto refused = unreadable_.find(entry); refused != unreadable_.end())
        {
            return ArchiveText{.failure = refused->second};
        }

        const std::map<std::string, std::string>& contents = HeldBy(archive);
        const auto held = contents.find(entry);

        if (held == contents.end())
        {
            return ArchiveText{.failure = ArchiveFailure::CouldNotExtract};
        }

        return ArchiveText{.text = held->second.substr(0, most)};
    }

    [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path& archive,
                                            const std::vector<ArchiveEntry>& listedEntries,
                                            const std::filesystem::path& into,
                                            const std::function<bool(const CopyProgress&)>& onProgress) const override
    {
        extractions.push_back(Extraction{.archive = archive, .into = into, .entries = listedEntries.size()});

        if (extractionFails_ != ArchiveFailure::None)
        {
            return extractionFails_;
        }

        std::size_t written = 0;
        std::map<std::string, std::string> firstSpelling;

        for (const ArchiveEntry& entry : listedEntries)
        {
            if (stopsAfter_.has_value() && written >= *stopsAfter_)
            {
                return ArchiveFailure::Cancelled;
            }

            if (onProgress && !onProgress(CopyProgress{.copiedBytes = written, .totalBytes = listedEntries.size()}))
            {
                return ArchiveFailure::Cancelled;
            }

            written += Write(entry, into, firstSpelling);
        }

        return failureAfterWriting_;
    }

    struct ReadAsked
    {
        std::string entry{};
        std::size_t most = 0;
    };

    struct Extraction
    {
        std::filesystem::path archive{};
        std::filesystem::path into{};
        std::size_t entries = 0;
    };

    mutable std::vector<std::filesystem::path> listed;
    mutable std::vector<ReadAsked> read;
    mutable std::vector<Extraction> extractions;

private:
    [[nodiscard]] const std::map<std::string, std::string>& HeldBy(const std::filesystem::path& archive) const
    {
        const auto own = contentsByOrigin_.find(archive.generic_string());

        return own == contentsByOrigin_.end() ? contents_ : own->second;
    }

    [[nodiscard]] std::filesystem::path Landing(const ArchiveEntry& entry,
                                                const std::filesystem::path& into,
                                                std::map<std::string, std::string>& firstSpelling) const
    {
        const std::filesystem::path relative = PathFromUtf8(entry.path);

        if (!mergesFolders_)
        {
            return PathUnder(into, relative);
        }

        const auto parts = static_cast<std::size_t>(std::distance(relative.begin(), relative.end()));
        const std::size_t folders = entry.kind == ArchiveEntryKind::Folder ? parts : parts - 1;
        std::filesystem::path landing = into;
        std::string seen;
        std::size_t position = 0;

        for (const std::filesystem::path& part : relative)
        {
            std::string name = AsUtf8(part);

            if (position++ < folders)
            {
                seen += "/" + LoweredForComparison(name);
                name = firstSpelling.try_emplace(seen, name).first->second;
            }

            landing /= PathFromUtf8(name);
        }

        return landing;
    }

    [[nodiscard]] std::size_t Write(const ArchiveEntry& entry,
                                    const std::filesystem::path& into,
                                    std::map<std::string, std::string>& firstSpelling) const
    {
        const std::filesystem::path landing = Landing(entry, into, firstSpelling);

        if (entry.kind == ArchiveEntryKind::Folder)
        {
            fileSystem_.AddDirectory(landing);

            return 0;
        }

        if (std::ranges::find(left_, entry.path) != left_.end())
        {
            return 0;
        }

        const bool wrong = std::ranges::find(wrongSize_, entry.path) != wrongSize_.end();
        fileSystem_.AddFile(landing, wrong ? entry.bytes + 1 : entry.bytes);

        return 1;
    }

    InMemoryFileSystem& fileSystem_;
    std::vector<ArchiveEntry> entries_;
    std::map<std::string, std::string> contents_;
    std::map<std::string, std::vector<ArchiveEntry>> entriesByOrigin_;
    std::map<std::string, std::map<std::string, std::string>> contentsByOrigin_;
    std::map<std::string, ArchiveFailure> unreadable_;
    std::vector<std::string> wrongSize_;
    std::vector<std::string> left_;
    std::optional<std::size_t> stopsAfter_;
    bool mergesFolders_ = false;
    ArchiveFailure listingFails_ = ArchiveFailure::None;
    ArchiveFailure extractionFails_ = ArchiveFailure::None;
    ArchiveFailure failureAfterWriting_ = ArchiveFailure::None;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_ARCHIVE_READER_H
