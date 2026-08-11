#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "domain/model/SceneryFolder.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/scenery/AirportCoverage.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
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
        Out() << "usage: fsorg-bgl [--library <path>]... [<file or folder>...]\n"
              << "\n"
              << "Answers the airport code of scenery, read from inside the BGL and never from the\n"
              << "folder name. The code is packed base 38 inside the record, and the two record\n"
              << "shapes disagree on the shift, so it is measured per record and never guessed.\n"
              << "\n"
              << "  --library <path>   walk a library root through the same scan the app uses and\n"
              << "                     answer per addon, separating an addon that carries no airport\n"
              << "                     record from one whose record was there and did not decode.\n"
              << "                     It looks inside the scenery folder of each addon and nowhere\n"
              << "                     else, which is 46 times cheaper than walking the addon whole\n"
              << "                     and cost no addon code in the reference installation.\n"
              << "                     Repeat for more than one library, and the groups of addons\n"
              << "                     sharing a code come out at the end: that is the real yield,\n"
              << "                     measured here and never copied from a document.\n"
              << "  <file or folder>   answer per BGL file instead, which is how a single scenery\n"
              << "                     gets checked against a code known from outside.\n";
    }

    constexpr std::size_t kEnoughForTheSectionTable = 64 * 1024;

    [[nodiscard]] std::vector<std::uint8_t> BytesOf(const std::filesystem::path& file, const std::size_t most)
    {
        std::ifstream stream(file, std::ios::binary);
        std::vector<std::uint8_t> bytes(most, 0);

        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(most));
        bytes.resize(static_cast<std::size_t>(stream.gcount()));

        return bytes;
    }

    [[nodiscard]] std::vector<std::uint8_t> BytesOf(const std::filesystem::path& file)
    {
        std::error_code failed;
        const std::uintmax_t wide = std::filesystem::file_size(file, failed);

        return BytesOf(file, failed ? kEnoughForTheSectionTable : static_cast<std::size_t>(wide));
    }

    [[nodiscard]] bool LooksLikeABgl(const std::filesystem::path& file)
    {
        return ComparableFileName(file).ends_with(".bgl");
    }

    [[nodiscard]] QString ReadingName(const SceneryReading reading)
    {
        switch (reading)
        {
        case SceneryReading::Read: return "read";
        case SceneryReading::ItCarriesNoSignature: return "not a bgl";
        case SceneryReading::ItEndsBeforeItSaysItDoes: return "truncated";
        }

        return "?";
    }

    [[nodiscard]] QString EvidenceName(const AirportEvidence evidence)
    {
        switch (evidence)
        {
        case AirportEvidence::ItCarriesNoAirportRecord: return "no airport record";
        case AirportEvidence::ARecordWasNotRead: return "record not read";
        case AirportEvidence::TheCodeWasRead: return "read";
        }

        return "?";
    }

    [[nodiscard]] QString Joined(const std::vector<std::string>& codes)
    {
        QString text;
        for (const std::string& code : codes)
        {
            text += (text.isEmpty() ? "" : " ") + QString::fromStdString(code);
        }

        return text;
    }

    struct Tally
    {
        int files = 0;
        int opened = 0;
        int carrying = 0;
        int refused = 0;
        std::chrono::steady_clock::duration scanning{};
        std::chrono::steady_clock::duration reading{};
    };

    [[nodiscard]] QString AsMilliseconds(const std::chrono::steady_clock::duration spent)
    {
        return QString::number(std::chrono::duration<double, std::milli>(spent).count(), 'f', 1) + " ms";
    }

    [[nodiscard]] SceneryCodes ReadOne(const SceneryParser& parser, const std::filesystem::path& file, Tally& tally)
    {
        ++tally.files;

        if (!parser.CouldCarryAnAirportSection(BytesOf(file, kEnoughForTheSectionTable)))
        {
            return {};
        }

        ++tally.opened;

        SceneryCodes found = parser.Parse(BytesOf(file));

        tally.refused += found.reading == SceneryReading::Read ? 0 : 1;
        tally.carrying += found.codes.empty() ? 0 : 1;

        return found;
    }

    [[nodiscard]] std::vector<std::filesystem::path> BglsUnder(const std::filesystem::path& folder)
    {
        std::vector<std::filesystem::path> files;
        std::error_code failed;

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(
                 folder, std::filesystem::directory_options::skip_permission_denied, failed))
        {
            if (entry.is_regular_file(failed) && LooksLikeABgl(entry.path()))
            {
                files.push_back(entry.path());
            }
        }

        return files;
    }

    [[nodiscard]] std::vector<std::filesystem::path> BglsOfTheAddon(const std::filesystem::path& addon)
    {
        std::vector<std::filesystem::path> files;
        std::error_code failed;

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(
                 addon, std::filesystem::directory_options::skip_permission_denied, failed))
        {
            if (entry.is_directory(failed) && ItIsTheSceneryFolderOfAnAddon(entry.path()))
            {
                for (const std::filesystem::path& file : BglsUnder(entry.path()))
                {
                    files.push_back(file);
                }
            }
        }

        return files;
    }

    [[nodiscard]] std::filesystem::path Resolved(const std::filesystem::path& folder)
    {
        std::error_code failed;
        const std::filesystem::path resolved = std::filesystem::weakly_canonical(folder, failed);

        return failed ? folder : resolved;
    }

    void ReportFiles(const SceneryParser& parser, const std::filesystem::path& path, Tally& tally)
    {
        const std::vector<std::filesystem::path> files =
            std::filesystem::is_regular_file(path) ? std::vector{path} : BglsUnder(path);

        for (const std::filesystem::path& file : files)
        {
            const SceneryCodes found = ReadOne(parser, file, tally);

            if (found.reading == SceneryReading::Read && found.codes.empty() && !found.anIdentifierDidNotDecode)
            {
                continue;
            }

            Out() << "  " << ReadingName(found.reading).leftJustified(12)
                  << (found.anIdentifierDidNotDecode ? QString("not decoded ") : QString("            "))
                  << Joined(found.codes).leftJustified(24) << AsText(file) << "\n";
        }
    }

    [[nodiscard]] SceneryOfAnAddon
    SceneryOf(const SceneryParser& parser, const TreeNode& addon, const std::filesystem::path& library, Tally& tally)
    {
        SceneryOfAnAddon read{.addon = {.libraryId = AsUtf8(library), .folderName = AsUtf8(addon.path.filename())},
                              .resolvedPath = Resolved(addon.path)};

        for (const std::filesystem::path& file : BglsOfTheAddon(addon.path))
        {
            read.files.push_back(ReadOne(parser, file, tally));
        }

        return read;
    }

    void ReportGroups(const std::vector<AirportGroup>& groups)
    {
        Out() << "\ngroups of addons sharing a code: " << QString::number(groups.size()) << "\n";

        for (const AirportGroup& group : groups)
        {
            Out() << "  " << QString::fromStdString(group.code).leftJustified(8);

            for (const AddonId& addon : group.addons)
            {
                Out() << QString::fromStdString(addon.folderName) << "  ";
            }

            Out() << "\n";
        }
    }

    struct Arguments
    {
        std::vector<std::filesystem::path> libraries{};
        std::vector<std::filesystem::path> files{};
    };

    [[nodiscard]] Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] != "--library")
            {
                parsed.files.push_back(AsPath(arguments[index]));
                continue;
            }

            if (index + 1 < arguments.size())
            {
                parsed.libraries.push_back(AsPath(arguments[index + 1]));
                ++index;
            }
        }

        return parsed;
    }

    [[nodiscard]] bool Announce(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            Out() << "no such path: " << AsText(path) << "\n";

            return false;
        }

        Out() << "\n" << AsText(path) << "\n";

        return true;
    }

    void ReadTheLibrary(const SceneryParser& parser,
                        const CatalogScanner& scanner,
                        const std::filesystem::path& library,
                        std::vector<SceneryOfAnAddon>& scenery,
                        Tally& tally)
    {
        const std::chrono::steady_clock::time_point beforeTheScan = std::chrono::steady_clock::now();
        const TreeNode tree = scanner.Scan(library);
        const std::chrono::steady_clock::time_point afterTheScan = std::chrono::steady_clock::now();

        tally.scanning += afterTheScan - beforeTheScan;

        for (const TreeNode* addon : AddonsUnder(tree))
        {
            scenery.push_back(SceneryOf(parser, *addon, library, tally));
        }

        tally.reading += std::chrono::steady_clock::now() - afterTheScan;
    }

    void ReportAddons(const std::vector<AirportsOfAnAddon>& airports)
    {
        int carrying = 0;
        int notRead = 0;

        for (const AirportsOfAnAddon& addon : airports)
        {
            carrying += addon.evidence == AirportEvidence::TheCodeWasRead ? 1 : 0;
            notRead += addon.evidence == AirportEvidence::ARecordWasNotRead ? 1 : 0;

            if (addon.evidence == AirportEvidence::ItCarriesNoAirportRecord)
            {
                continue;
            }

            Out() << "  " << EvidenceName(addon.evidence).leftJustified(20) << Joined(addon.codes).leftJustified(24)
                  << QString::fromStdString(addon.addon.folderName) << "\n";
        }

        Out() << "\naddons " << QString::number(airports.size()) << ", carrying a code " << QString::number(carrying)
              << ", carrying a record that did not decode " << QString::number(notRead)
              << ", carrying no airport record "
              << QString::number(static_cast<int>(airports.size()) - carrying - notRead) << "\n";
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    const QStringList arguments = QCoreApplication::arguments();

    if (arguments.size() < 2 || arguments.contains("--help"))
    {
        ReportUsage();
        Out().flush();

        return arguments.size() < 2 ? 2 : 0;
    }

    const Arguments parsed = Parse(arguments);

    const BglSceneryParser parser;
    const JsonManifestParser manifestParser;
    const WindowsFilesystemProbe filesystemProbe;
    const FilesystemScanner scanner(manifestParser, filesystemProbe);

    Tally tally;
    std::vector<SceneryOfAnAddon> scenery;

    for (const std::filesystem::path& path : parsed.files)
    {
        if (Announce(path))
        {
            ReportFiles(parser, path, tally);
        }
    }

    for (const std::filesystem::path& library : parsed.libraries)
    {
        if (!Announce(library))
        {
            continue;
        }

        ReadTheLibrary(parser, scanner, library, scenery, tally);
    }

    if (!scenery.empty())
    {
        const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon(scenery);

        ReportAddons(airports);
        ReportGroups(GroupsOfTheSameAirport(airports));
    }

    Out() << "\nbgl files seen " << QString::number(tally.files) << ", opened past the section table "
          << QString::number(tally.opened) << ", carrying a code " << QString::number(tally.carrying) << ", refused "
          << QString::number(tally.refused) << "\n";

    if (tally.scanning > std::chrono::steady_clock::duration::zero())
    {
        Out() << "opening the tree " << AsMilliseconds(tally.scanning) << ", reading the scenery "
              << AsMilliseconds(tally.reading)
              << ": the second number is what this feature costs, and it is spent outside the scan\n";
    }

    Out().flush();

    return 0;
}
