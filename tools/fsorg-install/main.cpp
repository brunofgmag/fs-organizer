#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <algorithm>
#include <map>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include "application/ArchiveInstallService.h"
#include "application/model/AppSettings.h"
#include "infrastructure/archive/ArchiveOrFolderReader.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JournalLinkedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "platform/Platform.h"
#include "support/PathText.h"
#include "support/SizeText.h"
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
        bool help = false;
        bool go = false;
        bool discardQuarantined = false;
        bool understood = true;
        QString archive{};
        QString category{};
        QString survey{};
    };

    Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            const QString& argument = arguments[index];
            const bool hasAValueAfterIt = index + 1 < arguments.size();

            if (argument == "--help")
            {
                parsed.help = true;
            }
            else if (argument == "--go")
            {
                parsed.go = true;
            }
            else if (argument == "--discard-quarantined")
            {
                parsed.discardQuarantined = true;
            }
            else if (argument == "--category" && hasAValueAfterIt)
            {
                parsed.category = arguments[++index];
            }
            else if (argument == "--survey" && hasAValueAfterIt)
            {
                parsed.survey = arguments[++index];
            }
            else if (!argument.startsWith("--") && parsed.archive.isEmpty())
            {
                parsed.archive = argument;
            }
            else
            {
                parsed.understood = false;
            }
        }

        return parsed;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-install <archive-or-folder> [--category <folder>]\n"
              << "       fsorg-install <archive-or-folder> --category <folder> --go [--discard-quarantined]\n"
              << "       fsorg-install --survey <folder>\n"
              << "\n"
              << "Looks inside a .zip, .rar or .7z archive, or a folder of addons, through the same\n"
              << "application service the install screen uses, and prints what it holds.\n"
              << "\n"
              << "  <archive-or-folder>     print the packages it holds: folder name,\n"
              << "                          version in the archive, size and what the library\n"
              << "                          would do with each. Needs --category to say anything\n"
              << "                          about the library\n"
              << "  --category <folder>     the category of the active profile's library to\n"
              << "                          install into\n"
              << "  --go                    install every package of the archive into that\n"
              << "                          category of the real library and write the real\n"
              << "                          journal. Without it nothing is written\n"
              << "  --discard-quarantined   with --go, let a package replace an installed copy\n"
              << "                          even when the library quarantine already holds an\n"
              << "                          earlier copy, which is then discarded\n"
              << "  --survey <folder>       inspect every .zip, .rar and .7z below the folder\n"
              << "                          without a category and print one tab-separated line\n"
              << "                          per archive: path, failure, refusal, packages, bytes.\n"
              << "                          It only lists and reads manifests\n";
    }

    QString NameOf(const ArchiveFailure failure)
    {
        switch (failure)
        {
        case ArchiveFailure::None: return "None";
        case ArchiveFailure::TheToolIsMissing: return "TheToolIsMissing";
        case ArchiveFailure::CouldNotOpen: return "CouldNotOpen";
        case ArchiveFailure::NeedsAPassword: return "NeedsAPassword";
        case ArchiveFailure::CouldNotExtract: return "CouldNotExtract";
        case ArchiveFailure::TookTooLong: return "TookTooLong";
        case ArchiveFailure::Cancelled: return "Cancelled";
        }

        return "ArchiveFailure(?)";
    }

    QString NameOf(const NothingToInstall refusal)
    {
        switch (refusal)
        {
        case NothingToInstall::None: return "None";
        case NothingToInstall::NoPackage: return "NoPackage";
        case NothingToInstall::OnlyAnInstaller: return "OnlyAnInstaller";
        case NothingToInstall::AnEntryIsNotSafe: return "AnEntryIsNotSafe";
        case NothingToInstall::ThePackageHasNoFolder: return "ThePackageHasNoFolder";
        case NothingToInstall::TwoPackagesShareAName: return "TwoPackagesShareAName";
        case NothingToInstall::ThePackageNameIsReserved: return "ThePackageNameIsReserved";
        }

        return "NothingToInstall(?)";
    }

    QString Shown(const std::string& text)
    {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }

    std::optional<SimulatorProfile> ActiveProfileOf(const AppSettings& settings)
    {
        if (settings.profiles.empty())
        {
            return std::nullopt;
        }

        const auto active = std::ranges::find_if(settings.profiles,
                                                 [&settings](const SimulatorProfile& profile)
                                                 {
                                                     return profile.id == settings.activeProfileId;
                                                 });

        return active == settings.profiles.end() ? settings.profiles.front() : *active;
    }

    std::vector<std::filesystem::path> LibraryRootsOf(const SimulatorProfile& profile)
    {
        std::vector<std::filesystem::path> roots;
        roots.reserve(profile.libraries.size());

        for (const Library& library : profile.libraries)
        {
            roots.push_back(library.path);
        }

        return roots;
    }

    struct Wiring
    {
        Wiring(PlatformParts parts, const LinkType linkType, const Verification verification)
            : platform(std::move(parts)),
              importEngine(filesystemProbe, files, sidecars, linking, log, linkType, verification),
              importing(importEngine, processProbe, filesystemProbe, catalog, files, sidecars, linking, log, linkType),
              installing(archives, manifestParser, filesystemProbe, files, log, importing)
        {
        }

        PlatformParts platform;
        LinkService& linkService{*platform.links};
        const FilesystemProbe& filesystemProbe{*platform.filesystem};
        FileOperations& files{*platform.files};
        SidecarStore& sidecars{*platform.sidecars};
        JsonManifestParser manifestParser{};
        JsonlOperationJournal journal{platform.folders.journalFile};
        JournalImportedFolders importedFolders{journal};
        JournalLinkedFolders theAppLinked{journal};
        FilesystemScanner catalog{manifestParser, filesystemProbe, importedFolders};
        const ProcessProbe& processProbe{*platform.processes};
        SystemClock clock{};
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe, theAppLinked};
        OperationLog log{journal, clock};
        ArchiveOrFolderReader archives{*platform.folderArchives, *platform.packedArchives};
        ImportEngine importEngine;
        ImportService importing;
        ArchiveInstallService installing;
    };

    QString AdmissionText(const AdmissionCheck& check)
    {
        if (check.result == FileResult::TheOutcomeIsUnknown)
        {
            return "not checked, no category given";
        }

        if (!Succeeded(check.result))
        {
            return "refused: " + Explain(check.result);
        }

        if (check.occupant.empty())
        {
            return "new, lands at " + AsText(check.target);
        }

        const QString version = check.occupantVersion.empty() ? QString("unknown") : Shown(check.occupantVersion);
        const QString replaces = "replaces version " + version + " at " + AsText(check.occupant);

        return check.theQuarantineHoldsAnEarlierCopy ? replaces + "; the quarantine holds an earlier copy" : replaces;
    }

    bool ReportWhyNothingIsToInstall(const WhatIsInTheArchive& found)
    {
        if (found.failure != ArchiveFailure::None)
        {
            Out() << "  could not read the archive: " << NameOf(found.failure) << "\n";
        }

        if (found.refusal != NothingToInstall::None)
        {
            Out() << "  nothing to install: " << NameOf(found.refusal) << "\n";
        }

        return found.failure != ArchiveFailure::None || found.refusal != NothingToInstall::None;
    }

    void ReportWhatIsIn(const WhatIsInTheArchive& found, const bool withACategory)
    {
        if (ReportWhyNothingIsToInstall(found))
        {
            return;
        }

        for (const PackageToInstall& candidate : found.packages)
        {
            const QString version =
                candidate.versionInTheArchive.empty() ? QString("unknown") : Shown(candidate.versionInTheArchive);

            Out() << "  " << Shown(FolderNameOf(candidate.package).Text()) << "\n"
                  << "    version in the archive: " << version << "\n"
                  << "    size: " << AsSize(candidate.package.bytes) << ", " << candidate.package.files << " files\n"
                  << "    library: " << AdmissionText(candidate.admission) << "\n";
        }

        if (withACategory)
        {
            Out() << "  room for the whole archive: " << Explain(found.room) << "\n";
        }
    }

    int Inspect(const Wiring& wiring, const SimulatorProfile& profile, const Arguments& arguments)
    {
        const std::filesystem::path category =
            arguments.category.isEmpty() ? std::filesystem::path{} : AsPath(arguments.category);

        Out() << AsText(AsPath(arguments.archive)) << "\n";
        ReportWhatIsIn(wiring.installing.Inspect(profile, AsPath(arguments.archive), category), !category.empty());
        Out() << "\nNothing was written. Pass --go with --category to install for real.\n";

        return 0;
    }

    ArchiveInstallRequest
    EveryPackageOf(const WhatIsInTheArchive& found, const Arguments& arguments, const std::filesystem::path& category)
    {
        ArchiveInstallRequest request{.archive = AsPath(arguments.archive), .category = category};

        for (const PackageToInstall& candidate : found.packages)
        {
            request.packages.push_back(ChosenPackage{.root = EntryPathOf(candidate.package),
                                                     .discardTheEarlierQuarantinedCopy = arguments.discardQuarantined,
                                                     .replaces = candidate.admission.occupant});
        }

        return request;
    }

    int ReportWhatInstallingDid(const WhatInstallingDid& did)
    {
        if (did.theStagingStayed)
        {
            Out() << "  the folder it was extracted into could not be removed\n";
        }

        if (did.failure != ArchiveFailure::None || did.refusal != NothingToInstall::None)
        {
            Out() << "  nothing was installed: " << NameOf(did.failure) << " " << NameOf(did.refusal) << "\n";

            return 1;
        }

        if (!Succeeded(did.stopped))
        {
            Out() << "  the install stopped before placing anything: " << Explain(did.stopped) << "\n";

            return 1;
        }

        bool everythingWent = true;

        for (const PackageInstalled& installed : did.packages)
        {
            everythingWent = everythingWent && Succeeded(installed.result);

            Out() << "  " << Shown(installed.root) << ": " << Explain(installed.result)
                  << (installed.replaced ? " (replaced an installed copy)" : "")
                  << (installed.landed.empty() ? QString() : " at " + AsText(installed.landed)) << "\n";
        }

        return everythingWent ? 0 : 1;
    }

    int Install(const Wiring& wiring, const SimulatorProfile& profile, const Arguments& arguments)
    {
        const std::filesystem::path category = AsPath(arguments.category);
        const WhatIsInTheArchive found = wiring.installing.Inspect(profile, AsPath(arguments.archive), category);

        Out() << AsText(AsPath(arguments.archive)) << "\n";

        if (ReportWhyNothingIsToInstall(found))
        {
            return 1;
        }

        const std::vector<DestinationEntry> entries =
            wiring.classifier.Resolve(profile.destinations, LibraryRootsOf(profile));

        return ReportWhatInstallingDid(
            wiring.installing.Install(profile, entries, EveryPackageOf(found, arguments, category)));
    }

    bool IsAnArchive(const std::filesystem::path& path)
    {
        const QString name = AsText(path);

        return name.endsWith(".zip", Qt::CaseInsensitive) || name.endsWith(".rar", Qt::CaseInsensitive)
            || name.endsWith(".7z", Qt::CaseInsensitive);
    }

    std::vector<std::filesystem::path> ArchivesBelow(const std::filesystem::path& folder)
    {
        std::vector<std::filesystem::path> archives;
        std::error_code error;

        for (std::filesystem::recursive_directory_iterator walk(
                 folder, std::filesystem::directory_options::skip_permission_denied, error);
             !error && walk != std::filesystem::recursive_directory_iterator(); walk.increment(error))
        {
            if (walk->is_regular_file(error) && IsAnArchive(walk->path()))
            {
                archives.push_back(walk->path());
            }
        }

        std::ranges::sort(archives);

        return archives;
    }

    QString TheKindOfResultOf(const WhatIsInTheArchive& found)
    {
        if (found.failure != ArchiveFailure::None)
        {
            return "failure:" + NameOf(found.failure);
        }

        return found.refusal == NothingToInstall::None ? QString("installable") : NameOf(found.refusal);
    }

    int Survey(const Wiring& wiring, const SimulatorProfile& profile, const QString& folder)
    {
        std::map<QString, int> counts;
        const std::vector<std::filesystem::path> archives = ArchivesBelow(AsPath(folder));

        for (const std::filesystem::path& archive : archives)
        {
            const WhatIsInTheArchive found = wiring.installing.Inspect(profile, archive, {});

            Out() << AsText(archive) << "\t" << NameOf(found.failure) << "\t" << NameOf(found.refusal) << "\t"
                  << found.packages.size() << "\t" << found.bytes << "\n";
            Out().flush();
            ++counts[TheKindOfResultOf(found)];
        }

        Out() << "summary\tarchives=" << archives.size();

        for (const auto& [kind, count] : counts)
        {
            Out() << "\t" << kind << "=" << count;
        }

        Out() << "\n";

        return 0;
    }

    int Run(const Wiring& wiring, const SimulatorProfile& profile, const Arguments& arguments)
    {
        if (!arguments.survey.isEmpty())
        {
            return Survey(wiring, profile, arguments.survey);
        }

        if (arguments.go)
        {
            return Install(wiring, profile, arguments);
        }

        return Inspect(wiring, profile, arguments);
    }

    int RefuseTheArguments()
    {
        Out() << "fsorg-install: the arguments were not understood, see --help\n";
        Out().flush();

        return 2;
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);
    const Arguments arguments = Parse(QCoreApplication::arguments());

    if (arguments.help)
    {
        ReportUsage();
        Out().flush();

        return 0;
    }

    const bool surveying = !arguments.survey.isEmpty();
    const bool inspecting = !arguments.archive.isEmpty();
    const bool installs = arguments.go && inspecting;
    const bool theUseIsClear = arguments.understood && surveying != inspecting && !(arguments.go && surveying)
        && !(installs && arguments.category.isEmpty());

    if (!theUseIsClear)
    {
        return RefuseTheArguments();
    }

    PlatformParts platform = BuildThePlatform();
    const JsonSettingsRepository settings(platform.folders.settingsFile);
    const AppSettings stored = settings.Load().value_or(AppSettings{});
    const std::optional<SimulatorProfile> profile = ActiveProfileOf(stored);

    if (!profile.has_value() && !arguments.category.isEmpty())
    {
        Out() << "no profile configured, so there is no library to look at\n";
        Out().flush();

        return 1;
    }

    const Wiring wiring(std::move(platform), stored.linkType, stored.verification);
    const SimulatorProfile theProfile = profile.value_or(SimulatorProfile{});

    const int exitCode = Run(wiring, theProfile, arguments);

    Out().flush();

    return exitCode;
}
