#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "shared/BglAirportCodes.h"
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
        Out() << "usage: fsorg-bgl <file or folder>...\n"
              << "\n"
              << "Reads every BGL under the paths given and answers the airport codes each one carries,\n"
              << "which is what tells a scenery from a model library: no airport record, no code.\n"
              << "The code is packed base 38 inside the record, and the two record shapes disagree on\n"
              << "the shift, so the answer is measured per record and never guessed from the folder name.\n";
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
        std::string suffix = file.extension().string();

        for (char& letter : suffix)
        {
            letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        }

        return suffix == ".bgl";
    }

    [[nodiscard]] QString ReadingName(const BglReading reading)
    {
        switch (reading)
        {
        case BglReading::Read: return "read";
        case BglReading::ItCarriesNoSignature: return "not a bgl";
        case BglReading::ItEndsBeforeItSaysItDoes: return "truncated";
        }

        return "?";
    }

    struct Tally
    {
        int files = 0;
        int carrying = 0;
        int refused = 0;
    };

    void ReportOne(const std::filesystem::path& file, Tally& tally)
    {
        ++tally.files;

        if (!CouldCarryAnAirportSection(BytesOf(file, kEnoughForTheSectionTable)))
        {
            return;
        }

        const BglAirportCodes found = AirportCodesIn(BytesOf(file));

        tally.refused += found.reading == BglReading::Read ? 0 : 1;
        tally.carrying += found.codes.empty() ? 0 : 1;

        if (found.reading == BglReading::Read && found.codes.empty())
        {
            return;
        }

        QString codes;
        for (const std::string& code : found.codes)
        {
            codes += (codes.isEmpty() ? "" : " ") + QString::fromStdString(code);
        }

        Out() << "  " << ReadingName(found.reading).leftJustified(12) << codes.leftJustified(24) << AsText(file)
              << "\n";
    }

    void ReportUnder(const std::filesystem::path& path, Tally& tally)
    {
        if (std::filesystem::is_regular_file(path))
        {
            ReportOne(path, tally);
            return;
        }

        std::error_code failed;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(
                 path, std::filesystem::directory_options::skip_permission_denied, failed))
        {
            if (entry.is_regular_file(failed) && LooksLikeABgl(entry.path()))
            {
                ReportOne(entry.path(), tally);
            }
        }
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

    Tally tally;

    for (int index = 1; index < arguments.size(); ++index)
    {
        const std::filesystem::path path = AsPath(arguments[index]);

        if (!std::filesystem::exists(path))
        {
            Out() << "no such path: " << arguments[index] << "\n";
            continue;
        }

        Out() << "\n" << arguments[index] << "\n";
        ReportUnder(path, tally);
    }

    Out() << "\nfiles read " << tally.files << ", carrying an airport code " << tally.carrying << ", refused "
          << tally.refused << "\n";
    Out().flush();

    return 0;
}
