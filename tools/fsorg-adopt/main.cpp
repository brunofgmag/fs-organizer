#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <algorithm>
#include <optional>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "application/model/AppSettings.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JournalLinkedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "support/PathText.h"
#include "viewmodel/FailureText.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    struct Arguments
    {
        bool go = false;
        bool takeBack = false;
    };

    Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            parsed.go = parsed.go || arguments[index] == "--go";
            parsed.takeBack = parsed.takeBack || arguments[index] == "--take-back";
        }

        return parsed;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-adopt [--take-back] [--go]\n"
              << "\n"
              << "Declares as categories the folders of every library of the active profile that\n"
              << "the user built, through the same application service the Options screen will\n"
              << "use. A folder is the user's structure when it holds no manifest of its own and\n"
              << "the importer never brought it in, and the journal is what answers the second\n"
              << "half. Without --go it reads and prints and writes nothing.\n"
              << "\n"
              << "  --take-back   remove every marker instead of writing them, which is what\n"
              << "                uninstalling the app has to offer\n"
              << "  --go          actually write, and write the real journal\n";
    }

    void ReportFolders(const QString& heading, const std::vector<std::filesystem::path>& folders)
    {
        Out() << "  " << heading << ": " << folders.size() << "\n";

        for (const std::filesystem::path& folder : folders)
        {
            Out() << "    " << AsText(folder) << "\n";
        }
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);
    const Arguments arguments = Parse(QCoreApplication::arguments());

    if (QCoreApplication::arguments().contains("--help"))
    {
        ReportUsage();
        Out().flush();

        return 0;
    }

    const JsonSettingsRepository settings(SettingsFilePath());
    const std::optional<AppSettings> stored = settings.Load();

    if (!stored.has_value() || stored->profiles.empty())
    {
        Out() << "no profile configured, so there is no library to adopt\n";
        Out().flush();

        return 1;
    }

    const auto active = std::ranges::find_if(stored->profiles,
                                             [&stored](const SimulatorProfile& profile)
                                             {
                                                 return profile.id == stored->activeProfileId;
                                             });

    const SimulatorProfile& profile = active == stored->profiles.end() ? stored->profiles.front() : *active;

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    const JsonManifestParser manifestParser;
    JsonlOperationJournal journal(JournalFilePath());

    const JournalImportedFolders importedFolders(journal);
    const JournalLinkedFolders theAppLinked(journal);

    const FilesystemScanner catalog(manifestParser, filesystemProbe, importedFolders);
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe, theAppLinked);
    const OperationLog log(journal, clock);

    const LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                                     stored->linkType);

    bool everythingWent = true;

    for (const Library& library : profile.libraries)
    {
        Out() << "\n" << AsText(library.path) << "\n";

        const LibraryGrouping grouping = organizer.HowItIsGrouped(library);
        ReportFolders("already declared", grouping.alreadyDeclared);
        ReportFolders(arguments.takeBack ? "would stay as they are" : "would be declared", grouping.notYetDeclared);

        if (!arguments.go)
        {
            continue;
        }

        const std::vector<FileOperationResult> written = arguments.takeBack
            ? organizer.TakeBackEveryMarkerItWrote(profile, library)
            : organizer.AdoptTheStructure(profile, library);

        for (const FileOperationResult& result : written)
        {
            if (!Succeeded(result.result))
            {
                Out() << "    refused  " << AsText(result.path) << ": " << Explain(result.result) << "\n";
                everythingWent = false;
            }
        }

        Out() << "  wrote " << written.size() << "\n";
    }

    if (!arguments.go)
    {
        Out() << "\nNothing was written. Pass --go to write for real.\n";
    }

    Out().flush();

    return everythingWent ? 0 : 1;
}
