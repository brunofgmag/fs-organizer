#include "application/SizeService.h"

#include <memory>
#include <numeric>
#include <ranges>
#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    using MeasuredBytes = std::map<std::string, std::uintmax_t>;

    struct Walk
    {
        const FilesystemProbe& filesystemProbe;
        const MeasuredBytes& known;
        MeasuredBytes& fresh;
        const std::function<bool(const SizeProgress&)>& onProgress;
        std::size_t total = 0;
        std::size_t done = 0;
        bool cancelled = false;
    };

    [[nodiscard]] std::optional<std::uintmax_t> BytesUnder(const Walk& walk, const std::filesystem::path& folder)
    {
        const std::string key = ComparablePath(folder);

        if (const auto measured = walk.known.find(key); measured != walk.known.end())
        {
            return measured->second;
        }

        const std::optional<std::vector<FileFingerprint>> files = walk.filesystemProbe.FingerprintTree(folder);
        if (!files.has_value())
        {
            return std::nullopt;
        }

        const auto sizes = *files | std::views::transform(&FileFingerprint::size);
        const std::uintmax_t bytes = std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});

        walk.fresh[key] = bytes;

        return bytes;
    }

    [[nodiscard]] MeasuredNode MeasureAddon(Walk& walk, const TreeNode& node)
    {
        MeasuredNode measured{.kind = node.kind, .path = node.path, .measured = false};

        if (walk.cancelled)
        {
            return measured;
        }

        if (walk.onProgress
            && !walk.onProgress(SizeProgress{.folder = node.path, .measured = walk.done, .total = walk.total}))
        {
            walk.cancelled = true;

            return measured;
        }

        const std::optional<std::uintmax_t> bytes = BytesUnder(walk, node.path);
        ++walk.done;

        measured.measured = bytes.has_value();
        measured.bytes = bytes.value_or(0);

        return measured;
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

void SizeService::Measure(const std::vector<std::filesystem::path>& libraryRoots,
                          const MeasurementCaller caller,
                          const Freshness freshness,
                          std::function<bool(const SizeProgress&)> onProgress,
                          std::function<void(const SizeReport&)> onMeasured)
{
    const int mine = ++asked_[caller.id];

    const auto known =
        std::make_shared<MeasuredBytes>(freshness == Freshness::ReuseWhatIsKnown ? bytes_ : MeasuredBytes{});
    const auto fresh = std::make_shared<MeasuredBytes>();
    const auto report = std::make_shared<SizeReport>();

    runner_.Run(
        [this, libraryRoots, known, fresh, report, onProgress = std::move(onProgress)]
        {
            *report = MeasureLibraries(libraryRoots, *known, *fresh, onProgress);
        },
        [this, caller, mine, fresh, report, onMeasured = std::move(onMeasured)]
        {
            Adopt(caller, mine, *fresh, *report, onMeasured);
        });
}

SizeReport SizeService::MeasureLibraries(const std::vector<std::filesystem::path>& libraryRoots,
                                         const std::map<std::string, std::uintmax_t>& known,
                                         std::map<std::string, std::uintmax_t>& fresh,
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

void SizeService::Adopt(const MeasurementCaller caller,
                        const int request,
                        const std::map<std::string, std::uintmax_t>& fresh,
                        SizeReport& report,
                        const std::function<void(const SizeReport&)>& onMeasured)
{
    const bool overtaken = request != asked_[caller.id];

    for (const auto& [folder, bytes] : fresh)
    {
        if (overtaken)
        {
            bytes_.emplace(folder, bytes);
            continue;
        }

        bytes_[folder] = bytes;
    }

    if (overtaken)
    {
        return;
    }

    report.measuredAt = clock_.Now();

    if (onMeasured)
    {
        onMeasured(report);
    }
}

std::optional<std::uintmax_t> SizeService::BytesOf(const std::filesystem::path& folder) const
{
    const auto measured = bytes_.find(ComparablePath(folder));

    return measured == bytes_.end() ? std::nullopt : std::optional(measured->second);
}
