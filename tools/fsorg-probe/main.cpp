#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <algorithm>
#include <map>
#include <ranges>
#include <set>

#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsSimulatorLocator.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "support/PathText.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    QString VariantName(const SimulatorVariant variant)
    {
        return variant == SimulatorVariant::MSFS2020 ? "MSFS2020" : "MSFS2024";
    }

    QString ClassName(const EntryClassification classification)
    {
        switch (classification)
        {
        case EntryClassification::Managed: return "Managed";
        case EntryClassification::External: return "External";
        case EntryClassification::Broken: return "Broken";
        case EntryClassification::Unavailable: return "Unavailable";
        case EntryClassification::Unmanaged: return "Unmanaged";
        case EntryClassification::Duplicated: return "Duplicated";
        default: return "?";
        }
    }

    struct LibraryFacts
    {
        std::size_t categories = 0;
        std::size_t folders = 0;
        std::size_t addons = 0;
        std::size_t empty = 0;
        std::map<std::string, std::size_t> contentTypes;
        std::set<std::string> addonFolderNames;
        std::set<std::string> allFolderNames;
    };

    struct ConflictCounts
    {
        std::size_t againstAddons = 0;
        std::size_t againstEveryFolder = 0;
    };

    void CollectFolder(const TreeNode& folder, LibraryFacts& facts)
    {
        ++facts.folders;
        facts.allFolderNames.insert(ComparablePath(folder.path.filename()));

        if (folder.kind == TreeNodeKind::Addon)
        {
            ++facts.contentTypes[folder.addon->manifest.contentType];
            facts.addonFolderNames.insert(ComparablePath(folder.path.filename()));
            return;
        }

        if (folder.children.empty())
        {
            ++facts.empty;
            return;
        }

        for (const TreeNode& child : folder.children)
        {
            CollectFolder(child, facts);
        }
    }

    LibraryFacts CollectLibraries(const CatalogScanner& scanner, const std::vector<std::filesystem::path>& libraries)
    {
        LibraryFacts facts;

        for (const std::filesystem::path& library : libraries)
        {
            const TreeNode tree = scanner.Scan(library);
            facts.addons += CountAddons(tree);

            for (const TreeNode& category : tree.children)
            {
                ++facts.categories;

                for (const TreeNode& child : category.children)
                {
                    CollectFolder(child, facts);
                }
            }
        }

        return facts;
    }

    std::vector<std::filesystem::path> ParseLibraryArguments(const QStringList& arguments)
    {
        std::vector<std::filesystem::path> libraries;

        for (int index = 1; index + 1 < arguments.size(); ++index)
        {
            if (arguments[index] == "--library")
            {
                libraries.push_back(AsPath(arguments[index + 1]));
            }
        }

        return libraries;
    }

    std::vector<DestinationEntry> EntriesUnder(const std::vector<DestinationEntry>& entries,
                                               const std::filesystem::path& destination)
    {
        const std::string wanted = ComparablePath(destination);

        std::vector<DestinationEntry> here;
        std::ranges::copy_if(entries, std::back_inserter(here), [&wanted](const DestinationEntry& entry)
        {
            return ComparablePath(entry.path.parent_path()) == wanted;
        });

        return here;
    }

    ConflictCounts CountConflicts(const LibraryFacts& facts, const std::vector<DestinationEntry>& entries)
    {
        ConflictCounts conflicts;

        for (const DestinationEntry& entry : entries)
        {
            if (entry.classification != EntryClassification::Unmanaged)
            {
                continue;
            }

            const std::string name = ComparablePath(entry.path.filename());
            conflicts.againstAddons += facts.addonFolderNames.contains(name) ? 1 : 0;
            conflicts.againstEveryFolder += facts.allFolderNames.contains(name) ? 1 : 0;
        }

        return conflicts;
    }

    void ReportUsage()
    {
        Out() << "uso: fsorg-probe --library <caminho> [--library <caminho>]\n";
    }

    void ReportLibrary(const LibraryFacts& facts)
    {
        Out() << "\nBiblioteca\n";
        Out() << "  categorias   " << facts.categories << "\n";
        Out() << "  pastas       " << facts.folders << "\n";
        Out() << "  addons       " << facts.addons << "\n";
        Out() << "  vazias       " << facts.empty << "\n";
        Out() << "  content_type ";

        for (const auto& [contentType, count] : facts.contentTypes)
        {
            Out() << (contentType.empty() ? "<vazio>" : QString::fromStdString(contentType))
                << "=" << count << " ";
        }

        Out() << "\n";
    }

    void ReportDestination(const std::filesystem::path& destination, const std::vector<DestinationEntry>& entries)
    {
        std::map<EntryClassification, std::size_t> counts;
        std::size_t unmanagedWithManifest = 0;

        for (const DestinationEntry& entry : entries)
        {
            ++counts[entry.classification];
            if (entry.classification == EntryClassification::Unmanaged
                && std::filesystem::exists(entry.path / "manifest.json"))
            {
                ++unmanagedWithManifest;
            }
        }

        Out() << "\nDestino " << AsText(destination) << " — " << entries.size() << " entradas\n";

        for (const auto& [classification, count] : counts)
        {
            Out() << "  " << ClassName(classification).leftJustified(12) << count << "\n";
        }

        Out() << "    dos Unmanaged, " << unmanagedWithManifest << " com manifest e "
            << counts[EntryClassification::Unmanaged] - unmanagedWithManifest << " sem\n";
    }

    void ReportConflicts(const ConflictCounts& conflicts)
    {
        Out() << "    conflitos de cópia: " << conflicts.againstAddons << " contra addons, "
            << conflicts.againstEveryFolder << " contra qualquer pasta da biblioteca\n";
    }

    void ReportCandidate(const EntryClassifier& classifier,
                         const SimulatorCandidate& candidate,
                         const std::vector<std::filesystem::path>& libraries,
                         const LibraryFacts& facts)
    {
        Out() << "\n" << VariantName(candidate.variant) << "  " << AsText(candidate.packagesPath) << "\n";

        const std::vector<DestinationEntry> entries = classifier.Resolve(candidate.destinations, libraries);

        for (const std::filesystem::path& destination : candidate.destinations)
        {
            const std::vector<DestinationEntry> here = EntriesUnder(entries, destination);

            ReportDestination(destination, here);
            ReportConflicts(CountConflicts(facts, here));
        }
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    const std::vector<std::filesystem::path> libraries =
        ParseLibraryArguments(QCoreApplication::arguments());

    if (libraries.empty())
    {
        ReportUsage();
        Out().flush();
        return 2;
    }

    const WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    const JsonManifestParser manifestParser;
    const FilesystemScanner scanner(manifestParser, filesystemProbe);
    const WindowsSimulatorLocator locator(WindowsUserCfgLocations());
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const EntryClassifier classifier(linkService, filesystemProbe);

    Out() << "Simulador em execução: " << (processProbe.SimulatorIsRunning() ? "sim" : "não") << "\n";

    const LibraryFacts facts = CollectLibraries(scanner, libraries);
    ReportLibrary(facts);

    for (const SimulatorCandidate& candidate : locator.Locate())
    {
        ReportCandidate(classifier, candidate, libraries, facts);
    }

    Out().flush();

    return 0;
}
