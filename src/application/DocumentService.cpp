#include "application/DocumentService.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include "domain/documents/DocumentClassification.h"
#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kDocumentSuffix = ".pdf";
    constexpr auto kCatalogueFileName = "catalogue.json";

    constexpr std::uint64_t kHashBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kHashPrime = 1099511628211ULL;

    [[nodiscard]] bool ItIsADocumentFile(const std::filesystem::path& file)
    {
        return ComparableFileName(file).ends_with(kDocumentSuffix);
    }

    [[nodiscard]] std::uint64_t Mixed(std::uint64_t hash, const std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8)
        {
            hash ^= (value >> shift) & 0xFFU;
            hash *= kHashPrime;
        }

        return hash;
    }

    [[nodiscard]] std::uint64_t Mixed(std::uint64_t hash, const std::string& text)
    {
        for (const char letter : text)
        {
            hash ^= static_cast<unsigned char>(letter);
            hash *= kHashPrime;
        }

        return Mixed(hash, static_cast<std::uint64_t>(text.size()));
    }

    [[nodiscard]] std::uint64_t HashOfAFile(const FileFingerprint& file)
    {
        std::uint64_t hash = Mixed(kHashBasis, ComparablePath(file.relativePath));
        hash = Mixed(hash, static_cast<std::uint64_t>(file.size));

        return Mixed(hash, static_cast<std::uint64_t>(file.lastWriteTime.time_since_epoch().count()));
    }

    [[nodiscard]] std::string DigestOf(const TreeFingerprint& walk, const std::vector<std::string>& codes)
    {
        std::uint64_t files = 0;

        for (const FileFingerprint& file : walk.files)
        {
            files += HashOfAFile(file);
        }

        std::uint64_t airports = 0;

        for (const std::string& code : codes)
        {
            airports += Mixed(kHashBasis, code);
        }

        std::uint64_t hash = Mixed(kHashBasis, files);
        hash = Mixed(hash, static_cast<std::uint64_t>(walk.files.size()));
        hash = Mixed(hash, airports);
        hash = Mixed(hash, static_cast<std::uint64_t>(codes.size()));

        std::array<char, 16> digits{};
        const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), hash, 16);

        return std::string(digits.data(), written.ptr);
    }

    [[nodiscard]] std::string KeyOf(const AddonId& addon)
    {
        return LoweredForComparison(addon.libraryId) + '|' + LoweredForComparison(addon.folderName);
    }

    [[nodiscard]] std::map<std::string, const DocumentsOfAnAddon*>
    KnownByAddon(const std::vector<DocumentsOfAnAddon>& before)
    {
        std::map<std::string, const DocumentsOfAnAddon*> known;

        for (const DocumentsOfAnAddon& addon : before)
        {
            known.emplace(KeyOf(addon.addon), &addon);
        }

        return known;
    }

    struct AnAirportsFolder
    {
        std::string code{};
        std::filesystem::path folder{};
    };

    [[nodiscard]] std::vector<AnAirportsFolder> WhereTheChartsOfEachAirportSit(const std::vector<ChartFile>& charts)
    {
        std::vector<AnAirportsFolder> folders;

        for (const ChartFile& chart : charts)
        {
            const AnAirportsFolder holding{.code = chart.code, .folder = chart.relativePath.parent_path()};

            const auto known =
                std::ranges::find_if(folders,
                                     [&holding](const AnAirportsFolder& seen)
                                     {
                                         return seen.code == holding.code && seen.folder == holding.folder;
                                     });

            if (holding.code.empty() || known != folders.end())
            {
                continue;
            }

            folders.push_back(holding);
        }

        return folders;
    }

    [[nodiscard]] bool AlreadyAnswered(const std::vector<CatalogueOfAnAirport>& catalogues, const std::string& code)
    {
        return std::ranges::find_if(catalogues,
                                    [&code](const CatalogueOfAnAirport& catalogue)
                                    {
                                        return catalogue.code == code;
                                    })
            != catalogues.end();
    }

    [[nodiscard]] const std::vector<std::string>* CodesOf(const std::vector<AirportsOfAnAddon>& airports,
                                                          const AddonId& addon)
    {
        for (const AirportsOfAnAddon& carried : airports)
        {
            if (carried.addon == addon)
            {
                return &carried.codes;
            }
        }

        return nullptr;
    }
}

DocumentService::DocumentService(const FilesystemProbe& filesystemProbe,
                                 const ChartCatalogueParser& catalogueParser,
                                 const ChartVersions& chartVersions)
    : filesystemProbe_(filesystemProbe), catalogueParser_(catalogueParser), chartVersions_(chartVersions)
{
}

std::vector<ChartVersion> DocumentService::TheVersionsOf(const std::vector<std::filesystem::path>& charts,
                                                         const std::filesystem::path& folder) const
{
    std::vector<ChartVersion> versions;
    versions.reserve(charts.size());

    for (const std::filesystem::path& chart : charts)
    {
        versions.push_back({.file = chart, .version = chartVersions_.VersionOf(PathUnder(folder, chart))});
    }

    return versions;
}

std::vector<CatalogueOfAnAirport> DocumentService::CataloguesBeside(const std::filesystem::path& folder,
                                                                    const std::vector<ChartFile>& charts) const
{
    std::vector<CatalogueOfAnAirport> catalogues;

    for (const AnAirportsFolder& airport : WhereTheChartsOfEachAirportSit(charts))
    {
        if (AlreadyAnswered(catalogues, airport.code))
        {
            continue;
        }

        const std::filesystem::path beside =
            PathUnder(folder, PathUnder(airport.folder, PathFromUtf8(kCatalogueFileName)));
        const std::optional<std::string> content = filesystemProbe_.ContentsOf(beside);

        if (!content.has_value())
        {
            continue;
        }

        std::optional<ChartCatalogue> catalogue = catalogueParser_.Parse(*content);

        if (catalogue.has_value())
        {
            catalogues.push_back({.code = airport.code, .catalogue = std::move(*catalogue)});
        }
    }

    return catalogues;
}

DocumentsOfAnAddon DocumentService::DocumentsOf(const AddonId& addon,
                                                const std::filesystem::path& folder,
                                                const std::vector<std::string>& codes,
                                                const DocumentsOfAnAddon* before) const
{
    const std::optional<TreeFingerprint> walk = filesystemProbe_.FingerprintTree(folder);

    if (!walk.has_value())
    {
        return {.addon = addon, .folder = folder, .itWasWalked = false};
    }

    const std::string digest = DigestOf(*walk, codes);

    if (before != nullptr && before->digest == digest)
    {
        return *before;
    }

    DocumentsOfAnAddon found{.addon = addon, .folder = folder, .digest = digest};
    std::vector<ChartFile> charts;

    for (const FileFingerprint& file : walk->files)
    {
        if (!ItIsADocumentFile(file.relativePath))
        {
            continue;
        }

        const ClassifiedDocument classified = ClassifyDocument(file.relativePath, codes);

        if (classified.kind == DocumentKind::Document)
        {
            found.documents.push_back(file.relativePath);
            continue;
        }

        charts.push_back({.relativePath = file.relativePath, .code = classified.code});
    }

    const std::vector<CatalogueOfAnAirport> catalogues = CataloguesBeside(folder, charts);

    found.airports =
        ChartsGroupedByAirport(charts, catalogues, TheVersionsOf(FilesOfARepeatedPage(charts, catalogues), folder));

    return found;
}

std::vector<DocumentsOfAnAddon> DocumentService::IndexWhile(const std::vector<AddonToRead>& addons,
                                                            const std::vector<AirportsOfAnAddon>& airports,
                                                            const std::vector<DocumentsOfAnAddon>& before,
                                                            const DocumentProgress& onProgress) const
{
    const std::map<std::string, const DocumentsOfAnAddon*> known = KnownByAddon(before);

    std::vector<DocumentsOfAnAddon> indexed;
    indexed.reserve(addons.size());

    for (const AddonToRead& addon : addons)
    {
        const std::vector<std::string>* codes = CodesOf(airports, addon.addon);
        const auto earlier = known.find(KeyOf(addon.addon));
        const bool itIsTheSameFolder =
            earlier != known.end() && ComparablePath(earlier->second->folder) == ComparablePath(addon.folder);

        indexed.push_back(DocumentsOf(addon.addon, addon.folder, codes == nullptr ? std::vector<std::string>{} : *codes,
                                      itIsTheSameFolder ? earlier->second : nullptr));

        if (onProgress && !onProgress(indexed.back(), indexed.size(), addons.size()))
        {
            break;
        }
    }

    return indexed;
}
