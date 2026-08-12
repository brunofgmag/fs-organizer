#include "application/DocumentService.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "domain/documents/DocumentClassification.h"
#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    constexpr auto kDocumentSuffix = ".pdf";
    constexpr auto kCatalogueFileName = "catalogue.json";

    [[nodiscard]] bool ItIsADocumentFile(const std::filesystem::path& file)
    {
        return ComparableFileName(file).ends_with(kDocumentSuffix);
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

DocumentService::DocumentService(const CatalogScanner& catalog,
                                 const FilesystemProbe& filesystemProbe,
                                 const ChartCatalogueParser& catalogueParser)
    : catalog_(catalog), filesystemProbe_(filesystemProbe), catalogueParser_(catalogueParser)
{
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
                                                const std::vector<std::string>& codes) const
{
    const std::optional<TreeFingerprint> walk = filesystemProbe_.FingerprintTree(folder);

    if (!walk.has_value())
    {
        return {.addon = addon, .folder = folder, .itWasWalked = false};
    }

    DocumentsOfAnAddon found{.addon = addon, .folder = folder};
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

    found.airports = ChartsGroupedByAirport(charts, CataloguesBeside(folder, charts));

    return found;
}

std::vector<DocumentsOfAnAddon> DocumentService::IndexWhile(const std::vector<Library>& libraries,
                                                            const std::vector<AirportsOfAnAddon>& airports,
                                                            const DocumentProgress& onProgress) const
{
    std::vector<std::pair<AddonId, std::filesystem::path>> addons;

    for (const Library& library : libraries)
    {
        const TreeNode tree = catalog_.Scan(library.path);

        for (const TreeNode* addon : AddonsUnder(tree))
        {
            addons.emplace_back(AddonId{.libraryId = library.id, .folderName = AsUtf8(addon->path.filename())},
                                addon->path);
        }
    }

    std::vector<DocumentsOfAnAddon> indexed;
    indexed.reserve(addons.size());

    for (const auto& [addon, folder] : addons)
    {
        const std::vector<std::string>* codes = CodesOf(airports, addon);

        indexed.push_back(DocumentsOf(addon, folder, codes == nullptr ? std::vector<std::string>{} : *codes));

        if (onProgress && !onProgress(indexed.size(), addons.size()))
        {
            break;
        }
    }

    return indexed;
}
