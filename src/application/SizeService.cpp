#include "application/SizeService.h"

#include <memory>
#include <numeric>
#include <ranges>
#include <set>
#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    using MeasuredTrees = std::map<std::string, MeasuredTree>;

    struct Walk
    {
        const FilesystemProbe& filesystemProbe;
        const MeasuredTrees& known;
        MeasuredTrees& fresh;
        const std::function<bool(const SizeProgress&)>& onProgress;
        std::size_t total = 0;
        std::size_t done = 0;
        bool cancelled = false;
    };

    [[nodiscard]] std::optional<MeasuredTree> TreeUnder(const Walk& walk, const std::filesystem::path& folder)
    {
        const std::string key = ComparablePath(folder);

        if (const auto measured = walk.known.find(key); measured != walk.known.end())
        {
            return measured->second;
        }

        const std::optional<TreeFingerprint> walked = walk.filesystemProbe.FingerprintTree(folder);
        if (!walked.has_value())
        {
            return std::nullopt;
        }

        const auto sizes = walked->files | std::views::transform(&FileFingerprint::size);
        const MeasuredTree measured{.bytes = std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0}),
                                    .longestEntry = walked->longestEntry};

        walk.fresh[key] = measured;

        return measured;
    }

    [[nodiscard]] MeasuredFolder WeighOne(Walk& walk, const std::filesystem::path& folder)
    {
        MeasuredFolder weighed{.folder = folder};

        if (walk.cancelled)
        {
            return weighed;
        }

        if (walk.onProgress
            && !walk.onProgress(SizeProgress{.folder = folder, .measured = walk.done, .total = walk.total}))
        {
            walk.cancelled = true;

            return weighed;
        }

        const std::optional<MeasuredTree> measured = TreeUnder(walk, folder);
        ++walk.done;

        weighed.measured = measured.has_value();
        weighed.bytes = measured.value_or(MeasuredTree{}).bytes;
        weighed.longestEntry = measured.value_or(MeasuredTree{}).longestEntry;

        return weighed;
    }

    [[nodiscard]] MeasuredNode MeasureAddon(Walk& walk, const TreeNode& node)
    {
        const MeasuredFolder weighed = WeighOne(walk, node.path);

        return MeasuredNode{.kind = node.kind,
                            .path = node.path,
                            .bytes = weighed.bytes,
                            .measured = weighed.measured,
                            .longestEntry = weighed.longestEntry};
    }

    [[nodiscard]] MeasuredNode MeasureTree(Walk& walk, const TreeNode& node)
    {
        if (node.kind == TreeNodeKind::Addon)
        {
            return MeasureAddon(walk, node);
        }

        MeasuredNode measured{.kind = node.kind, .path = node.path};

        for (const TreeNode& child : node.children)
        {
            measured.children.push_back(MeasureTree(walk, child));
            measured.bytes += measured.children.back().bytes;
            measured.measured = measured.measured && measured.children.back().measured;
            measured.longestEntry = std::max(measured.longestEntry, measured.children.back().longestEntry);
        }

        return measured;
    }
}

SizeService::SizeService(const CatalogScanner& catalog,
                         const FilesystemProbe& filesystemProbe,
                         const Clock& clock,
                         BackgroundRunner& runner)
    : catalog_(catalog), filesystemProbe_(filesystemProbe), clock_(clock), runner_(runner)
{
}

MeasurementCaller SizeService::NewCaller()
{
    return MeasurementCaller{.id = ++callers_};
}

std::shared_ptr<MeasuredTrees> SizeService::WhatIsKnown(const Freshness freshness) const
{
    return std::make_shared<MeasuredTrees>(freshness == Freshness::ReuseWhatIsKnown ? measured_ : MeasuredTrees{});
}

void SizeService::Measure(const std::vector<std::filesystem::path>& libraryRoots,
                          const MeasurementCaller caller,
                          const Freshness freshness,
                          std::function<bool(const SizeProgress&)> onProgress,
                          std::function<void(const SizeReport&)> onMeasured)
{
    const int mine = ++asked_[caller.id];

    const auto known = WhatIsKnown(freshness);
    const auto fresh = std::make_shared<MeasuredTrees>();
    const auto report = std::make_shared<SizeReport>();

    runner_.Run(
        [this, libraryRoots, known, fresh, report, onProgress = std::move(onProgress)]
        {
            *report = MeasureLibraries(libraryRoots, *known, *fresh, onProgress);
        },
        [this, caller, mine, fresh, report, onMeasured = std::move(onMeasured)]
        {
            if (!Adopt(caller, mine, *fresh))
            {
                return;
            }

            report->measuredAt = clock_.Now();

            if (onMeasured)
            {
                onMeasured(*report);
            }
        });
}

void SizeService::MeasureFolders(const std::vector<std::filesystem::path>& folders,
                                 const MeasurementCaller caller,
                                 const Freshness freshness,
                                 std::function<bool(const SizeProgress&)> onProgress,
                                 std::function<void(const FolderSizeReport&)> onMeasured)
{
    const int mine = ++asked_[caller.id];

    const auto known = WhatIsKnown(freshness);
    const auto fresh = std::make_shared<MeasuredTrees>();
    const auto report = std::make_shared<FolderSizeReport>();

    runner_.Run(
        [this, folders, known, fresh, report, onProgress = std::move(onProgress)]
        {
            *report = WalkFolders(folders, *known, *fresh, onProgress);
        },
        [this, caller, mine, fresh, report, onMeasured = std::move(onMeasured)]
        {
            if (!Adopt(caller, mine, *fresh))
            {
                return;
            }

            report->measuredAt = clock_.Now();

            if (onMeasured)
            {
                onMeasured(*report);
            }
        });
}

FolderSizeReport SizeService::WalkFolders(const std::vector<std::filesystem::path>& folders,
                                          const std::map<std::string, MeasuredTree>& known,
                                          std::map<std::string, MeasuredTree>& fresh,
                                          const std::function<bool(const SizeProgress&)>& onProgress) const
{
    std::vector<std::filesystem::path> wanted;
    std::set<std::string> seen;

    for (const std::filesystem::path& folder : folders)
    {
        if (seen.insert(ComparablePath(folder)).second)
        {
            wanted.push_back(folder);
        }
    }

    Walk walk{.filesystemProbe = filesystemProbe_,
              .known = known,
              .fresh = fresh,
              .onProgress = onProgress,
              .total = wanted.size()};

    FolderSizeReport report;
    report.folders.reserve(wanted.size());

    for (const std::filesystem::path& folder : wanted)
    {
        report.folders.push_back(WeighOne(walk, folder));
        report.bytes += report.folders.back().bytes;
        report.measured += report.folders.back().measured ? 1 : 0;
    }

    report.complete = !walk.cancelled;

    return report;
}

SizeReport SizeService::MeasureLibraries(const std::vector<std::filesystem::path>& libraryRoots,
                                         const std::map<std::string, MeasuredTree>& known,
                                         std::map<std::string, MeasuredTree>& fresh,
                                         const std::function<bool(const SizeProgress&)>& onProgress) const
{
    std::vector<TreeNode> trees;
    trees.reserve(libraryRoots.size());
    for (const std::filesystem::path& root : libraryRoots)
    {
        trees.push_back(catalog_.Scan(root));
    }

    Walk walk{.filesystemProbe = filesystemProbe_, .known = known, .fresh = fresh, .onProgress = onProgress};
    for (const TreeNode& tree : trees)
    {
        walk.total += CountAddons(tree);
    }

    SizeReport report;
    for (const TreeNode& tree : trees)
    {
        report.libraries.push_back(MeasureTree(walk, tree));
    }

    report.complete = !walk.cancelled;

    return report;
}

bool SizeService::Adopt(const MeasurementCaller caller,
                        const int request,
                        const std::map<std::string, MeasuredTree>& fresh)
{
    const bool overtaken = request != asked_[caller.id];

    for (const auto& [folder, tree] : fresh)
    {
        if (overtaken)
        {
            measured_.emplace(folder, tree);
            continue;
        }

        measured_[folder] = tree;
    }

    return !overtaken;
}

std::optional<MeasuredTree> SizeService::WhatIsKnownAbout(const std::filesystem::path& folder) const
{
    const auto known = measured_.find(ComparablePath(folder));

    return known == measured_.end() ? std::nullopt : std::optional(known->second);
}

std::optional<std::uintmax_t> SizeService::BytesOf(const std::filesystem::path& folder) const
{
    const std::optional<MeasuredTree> known = WhatIsKnownAbout(folder);

    return known.has_value() ? std::optional(known->bytes) : std::nullopt;
}

std::optional<std::size_t> SizeService::LongestEntryOf(const std::filesystem::path& folder) const
{
    const std::optional<MeasuredTree> known = WhatIsKnownAbout(folder);

    return known.has_value() ? std::optional(known->longestEntry) : std::nullopt;
}
