#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/scenery/BglSceneryParser.h"
#include "support/PathText.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-docs --library <path> [--library <path>]... [--addon <folder>] [--charts]\n"
              << "\n"
              << "Indexes the PDFs of every addon of a real installation and separates chart from\n"
              << "document, through the same application service the documentation panel will use.\n"
              << "A chart is a PDF whose parent folder carries an airport code the addon actually\n"
              << "carries, read from inside the BGL, or a folder named Charts; everything else is a\n"
              << "document, which is why a folder named DOCS never becomes an airport.\n"
              << "\n"
              << "  --library <path>   a library root to index; repeat for more than one\n"
              << "  --addon <folder>   print the index of one addon in full instead of the summary\n"
              << "  --charts           print every airport of every addon that carries one\n"
              << "\n"
              << "The chart_id match is measured here and never copied from a document: the tail\n"
              << "says how many catalogue entries met a file, how many files no entry names, and\n"
              << "how many entries name a file that is not there.\n";
    }

    class RememberNothing final : public SceneryCache
    {
    public:
        [[nodiscard]] std::optional<RememberedScenery> Remember(const std::filesystem::path&) const override
        {
            return std::nullopt;
        }

        void Keep(const std::filesystem::path&, const RememberedScenery&) override
        {
        }

        void WriteWhatIsKept() override
        {
        }
    };

    class VersionsItCounts final : public ChartVersions
    {
    public:
        explicit VersionsItCounts(const ChartVersions& reading) : reading_(reading)
        {
        }

        [[nodiscard]] long long VersionOf(const std::filesystem::path& chart) const override
        {
            const std::chrono::steady_clock::time_point before = std::chrono::steady_clock::now();
            const long long version = reading_.VersionOf(chart);

            spent += std::chrono::steady_clock::now() - before;
            ++read;

            return version;
        }

        mutable std::size_t read = 0;
        mutable std::chrono::steady_clock::duration spent{};

    private:
        const ChartVersions& reading_;
    };

    struct Arguments
    {
        std::vector<std::filesystem::path> libraries{};
        std::string addon{};
        bool charts = false;
    };

    [[nodiscard]] Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] == "--charts")
            {
                parsed.charts = true;
                continue;
            }

            if (index + 1 >= arguments.size())
            {
                continue;
            }

            if (arguments[index] == "--library")
            {
                parsed.libraries.push_back(AsPath(arguments[index + 1]));
            }

            if (arguments[index] == "--addon")
            {
                parsed.addon = arguments[index + 1].toStdString();
            }
        }

        return parsed;
    }

    struct Tally
    {
        std::size_t addons = 0;
        std::size_t carryingADocument = 0;
        std::size_t documents = 0;
        std::size_t charts = 0;
        std::size_t airports = 0;
        std::size_t catalogued = 0;
        std::size_t lines = 0;
        std::size_t entries = 0;
        std::size_t entriesWithNoFile = 0;
        std::size_t filesNoEntryNames = 0;
        std::size_t entriesNamingTheSameFile = 0;
        std::size_t notWalked = 0;
    };

    [[nodiscard]] std::size_t PagesIn(const ChartsOfAnAirport& airport)
    {
        std::size_t pages = 0;

        for (const ChartsOfAType& group : airport.types)
        {
            for (const ChartEntry& chart : group.charts)
            {
                pages += chart.pages.size();
            }
        }

        return pages;
    }

    [[nodiscard]] std::size_t LinesIn(const ChartsOfAnAirport& airport)
    {
        std::size_t lines = 0;

        for (const ChartsOfAType& group : airport.types)
        {
            lines += group.charts.size();
        }

        return lines;
    }

    [[nodiscard]] std::size_t FilesNoEntryNamesIn(const ChartsOfAnAirport& airport)
    {
        if (!airport.catalogued)
        {
            return 0;
        }

        for (const ChartsOfAType& group : airport.types)
        {
            if (group.type.empty())
            {
                return group.charts.size();
            }
        }

        return 0;
    }

    void Count(Tally& tally, const DocumentsOfAnAddon& documents)
    {
        ++tally.addons;
        tally.notWalked += documents.itWasWalked ? 0 : 1;
        tally.documents += documents.documents.size();
        tally.carryingADocument += documents.documents.empty() && documents.airports.empty() ? 0 : 1;

        for (const ChartsOfAnAirport& airport : documents.airports)
        {
            const std::size_t pages = PagesIn(airport);
            const std::size_t orphanFiles = FilesNoEntryNamesIn(airport);
            const std::size_t named = pages - orphanFiles;

            ++tally.airports;
            tally.charts += pages;
            tally.lines += LinesIn(airport);
            tally.entries += airport.entriesInTheCatalogue;
            tally.filesNoEntryNames += orphanFiles;

            if (!airport.catalogued)
            {
                continue;
            }

            ++tally.catalogued;
            tally.entriesWithNoFile +=
                airport.entriesInTheCatalogue > named ? airport.entriesInTheCatalogue - named : 0;
            tally.entriesNamingTheSameFile +=
                named > airport.entriesInTheCatalogue ? named - airport.entriesInTheCatalogue : 0;
        }
    }

    [[nodiscard]] QString NameOf(const ChartEntry& chart)
    {
        if (chart.revision == ChartRevision::Previous)
        {
            return QString("(previous edition)");
        }

        if (chart.name.empty())
        {
            return QString("(unnamed)");
        }

        return QString::fromStdString(chart.name);
    }

    void ReportTheAirport(const ChartsOfAnAirport& airport, const bool inFull)
    {
        const QString code = airport.code.empty() ? QString("no code") : QString::fromStdString(airport.code);

        Out() << "    " << code.leftJustified(10) << QString::number(PagesIn(airport)).rightJustified(5) << " files, "
              << QString::number(LinesIn(airport)).rightJustified(4) << " lines"
              << (airport.catalogued ? QString(", catalogue of %1").arg(airport.entriesInTheCatalogue) : QString())
              << "\n";

        if (!inFull)
        {
            return;
        }

        for (const ChartsOfAType& group : airport.types)
        {
            const QString type = group.type.empty() ? QString("(no type)") : QString::fromStdString(group.type);

            Out() << "      " << type.leftJustified(10) << QString::number(group.charts.size()) << "\n";

            for (const ChartEntry& chart : group.charts)
            {
                const QString name = NameOf(chart);

                Out() << "        " << name.leftJustified(44)
                      << (chart.pages.size() > 1 ? QString("%1 pages").arg(chart.pages.size()) : QString()) << "  "
                      << AsText(chart.pages.front().filename()) << "\n";
            }
        }
    }

    void ReportTheAddon(const DocumentsOfAnAddon& documents, const bool inFull)
    {
        Out() << "  " << QString::fromStdString(documents.addon.folderName).leftJustified(52)
              << QString::number(documents.documents.size()).rightJustified(4) << " documents\n";

        for (const std::filesystem::path& document : documents.documents)
        {
            Out() << "    " << AsText(document) << "\n";
        }

        for (const ChartsOfAnAirport& airport : documents.airports)
        {
            ReportTheAirport(airport, inFull);
        }
    }

    [[nodiscard]] QString AsMilliseconds(const std::chrono::steady_clock::duration spent)
    {
        return QString::number(std::chrono::duration<double, std::milli>(spent).count(), 'f', 1) + " ms";
    }

    [[nodiscard]] std::vector<AddonToRead> AddonsOf(const CatalogScanner& scanner, const Library& library)
    {
        std::vector<AddonToRead> addons;
        const TreeNode tree = scanner.Scan(library.path);

        for (const TreeNode* addon : AddonsUnder(tree))
        {
            addons.push_back({.addon = {.libraryId = library.id, .folderName = AsUtf8(addon->path.filename())},
                              .folder = addon->path});
        }

        return addons;
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    const Arguments arguments = Parse(QCoreApplication::arguments());

    if (arguments.libraries.empty() || QCoreApplication::arguments().contains("--help"))
    {
        ReportUsage();
        Out().flush();

        return arguments.libraries.empty() ? 2 : 0;
    }

    const JsonManifestParser manifestParser;
    const JsonChartCatalogueParser catalogueParser;
    const WindowsFilesystemProbe filesystemProbe;
    const JsonlOperationJournal journal(JournalFilePath());
    const JournalImportedFolders importedFolders(journal);
    const FilesystemScanner scanner(manifestParser, filesystemProbe, importedFolders);
    const BglSceneryParser sceneryParser;
    const SystemClock clock;
    RememberNothing cache;
    SceneryService scenery(filesystemProbe, sceneryParser, clock, cache);
    const QtPdfChartVersions readTheVersions;
    const VersionsItCounts chartVersions(readTheVersions);
    const DocumentService documents(scanner, filesystemProbe, catalogueParser, chartVersions);

    std::vector<Library> libraries;
    for (const std::filesystem::path& path : arguments.libraries)
    {
        libraries.push_back({.id = AsUtf8(path), .path = path, .label = AsUtf8(path.filename())});
    }

    const std::chrono::steady_clock::time_point beforeTheCodes = std::chrono::steady_clock::now();

    std::vector<AirportsOfAnAddon> airports;
    for (const Library& library : libraries)
    {
        const std::vector<SceneryOfAnAddon> read = scenery.SceneryOfEach(AddonsOf(scanner, library), {});

        for (const AirportsOfAnAddon& carried : AirportsOfEachAddon(read))
        {
            airports.push_back(carried);
        }
    }

    const std::chrono::steady_clock::time_point beforeTheIndex = std::chrono::steady_clock::now();

    const std::vector<DocumentsOfAnAddon> indexed =
        documents.IndexWhile(libraries, airports,
                             [](const DocumentsOfAnAddon&, const std::size_t indexedSoFar, const std::size_t outOf)
                             {
                                 Out() << "\r  " << QString::number(indexedSoFar) << "/" << QString::number(outOf)
                                       << "  ";
                                 Out().flush();

                                 return true;
                             });

    const std::chrono::steady_clock::time_point afterTheIndex = std::chrono::steady_clock::now();

    Out() << "\r" << QString(40, ' ') << "\r";

    const bool oneAddonWasAskedFor = !arguments.addon.empty();
    const bool inFull = arguments.charts || oneAddonWasAskedFor;

    Tally tally;
    for (const DocumentsOfAnAddon& addon : indexed)
    {
        Count(tally, addon);

        const bool wanted = oneAddonWasAskedFor ? addon.addon.folderName == arguments.addon
                                                : !addon.documents.empty() || !addon.airports.empty();

        if (wanted)
        {
            ReportTheAddon(addon, inFull);
        }
    }

    Out() << "\naddons " << QString::number(tally.addons) << ", carrying documentation "
          << QString::number(tally.carryingADocument) << ", the probe could not walk "
          << QString::number(tally.notWalked) << "\n"
          << "documents " << QString::number(tally.documents) << ", chart files " << QString::number(tally.charts)
          << " over " << QString::number(tally.airports) << " airports, of which catalogued "
          << QString::number(tally.catalogued) << "\n"
          << "the index shows " << QString::number(tally.lines) << " lines for those " << QString::number(tally.charts)
          << " files\n"
          << "catalogue entries " << QString::number(tally.entries) << ", entries naming a file that is not there "
          << QString::number(tally.entriesWithNoFile) << ", files no entry names "
          << QString::number(tally.filesNoEntryNames) << "\n"
          << "reading the scenery " << AsMilliseconds(beforeTheIndex - beforeTheCodes) << ", walking for PDFs "
          << AsMilliseconds(afterTheIndex - beforeTheIndex) << "\n"
          << "chart versions read " << QString::number(chartVersions.read) << " of " << QString::number(tally.charts)
          << " chart files, in " << AsMilliseconds(chartVersions.spent) << "\n";

    Out().flush();

    return 0;
}
