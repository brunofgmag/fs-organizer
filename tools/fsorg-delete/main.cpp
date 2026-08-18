#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <optional>
#include <string>
#include <vector>

#include "application/DeletionService.h"
#include "application/model/AppSettings.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryTrees.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JournalLinkedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
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

    class RunHereAndNow final : public BackgroundRunner
    {
    public:
        void Run(std::function<void()> work, std::function<void()> done) override
        {
            work();
            done();
        }
    };

    struct Arguments
    {
        std::vector<std::filesystem::path> addons;
        DeletionRoute route = DeletionRoute::RecycleBin;
        bool go = false;
    };

    Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] == "--go")
            {
                parsed.go = true;
                continue;
            }

            if (index + 1 >= arguments.size())
            {
                continue;
            }

            if (arguments[index] == "--addon")
            {
                parsed.addons.push_back(AsPath(arguments[index + 1]));
            }

            if (arguments[index] == "--route" && arguments[index + 1] == "permanent")
            {
                parsed.route = DeletionRoute::Permanently;
            }
        }

        return parsed;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-delete --addon <path> [--addon <path>]... [--route recycle|permanent] [--go]\n"
              << "\n"
              << "Deletes addons from the real library through the same application service the\n"
              << "Library screen will use, by either route. Without --go it plans and prints and\n"
              << "writes nothing, which is the only way to see the guards answer before they have\n"
              << "a button. It reads the real settings.json and never saves it back.\n"
              << "\n"
              << "  --addon <path>      an addon folder inside a library of the active profile\n"
              << "  --route recycle     send to the Recycle Bin of the volume, the default\n"
              << "  --route permanent   remove for good, which reaches past 260 characters\n"
              << "  --go                actually delete, and write the real journal\n";
    }

    void ReportPlan(const DeletionPlan& plan)
    {
        for (const VolumeRoom& room : plan.volumes)
        {
            const QString selected = room.selected.has_value() ? AsSize(*room.selected) : QString("not measured");
            const QString quota = room.quota.has_value() ? AsSize(*room.quota) : QString("unknown");

            Out() << "Volume " << AsText(room.volume) << ": selection " << selected << ", Recycle Bin quota " << quota
                  << (room.itRecycles ? "" : ", set to delete permanently") << "\n";
        }

        for (const AddonToDelete& addon : plan.addons)
        {
            Out() << "\n  " << AsText(addon.folder.filename()) << "\n";
            Out() << "    size          " << (addon.bytes.has_value() ? AsSize(*addon.bytes) : QString("not measured"))
                  << "\n";
            Out() << "    longest entry "
                  << (addon.longestEntry.has_value() ? QString::number(*addon.longestEntry) : QString("unknown"))
                  << " characters\n";

            for (const EnabledSomewhere& link : addon.enabled)
            {
                Out() << "    enabled in    " << QString::fromStdString(link.profileId) << " at "
                      << AsText(link.linkPath) << "\n";
            }
        }

        Out() << "\nThe Recycle Bin " << (TheRecycleBinCanTake(plan) ? "can" : "cannot") << " take this selection.\n";

        if (plan.nodesThatAreNotAddons > 0)
        {
            Out() << QString::number(plan.nodesThatAreNotAddons)
                  << " of the paths given are not addons and count zero.\n";
        }
    }

    std::vector<const TreeNode*> AddonsNamed(const std::vector<TreeNode>& libraries,
                                             const std::vector<std::filesystem::path>& wanted)
    {
        std::vector<const TreeNode*> nodes;

        for (const std::filesystem::path& folder : wanted)
        {
            const TreeNode* addon = AddonAt(libraries, folder);

            if (addon == nullptr)
            {
                Out() << "not an addon of this profile, skipped: " << AsText(folder) << "\n";
                continue;
            }

            nodes.push_back(addon);
        }

        return nodes;
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    const Arguments arguments = Parse(QCoreApplication::arguments());

    if (arguments.addons.empty() || QCoreApplication::arguments().contains("--help"))
    {
        ReportUsage();
        Out().flush();

        return arguments.addons.empty() ? 2 : 0;
    }

    const JsonSettingsRepository settings(SettingsFilePath());
    const std::optional<AppSettings> stored = settings.Load();

    if (!stored.has_value() || stored->profiles.empty())
    {
        Out() << "no profile configured, so there is no library to delete from\n";
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
    WindowsSidecarStore sidecars;
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

    RunHereAndNow runner;
    SizeService sizes(catalog, filesystemProbe, clock, runner);
    const DeletionService service(filesystemProbe, files, sidecars, linking, classifier, processProbe, log, sizes);

    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog, profile);
    const std::vector<const TreeNode*> nodes = AddonsNamed(libraries, arguments.addons);

    std::vector<std::filesystem::path> folders;
    folders.reserve(nodes.size());
    for (const TreeNode* addon : nodes)
    {
        folders.push_back(addon->path);
    }

    sizes.MeasureFolders(
        folders, sizes.NewCaller(), Freshness::MeasureAgain,
        [](const SizeProgress& progress)
        {
            Out() << "\r  measuring " << AsText(progress.folder.filename()).leftJustified(56).left(56);
            Out().flush();

            return true;
        },
        [](const FolderSizeReport&) {});

    Out() << "\r" << QString(72, ' ') << "\r";

    const DeletionPlan plan = service.Plan(profile, stored->profiles, nodes);
    ReportPlan(plan);

    if (!arguments.go)
    {
        Out() << "\nNothing was written. Pass --go to delete for real.\n";
        Out().flush();

        return 0;
    }

    Out() << "\n";

    bool everythingWent = true;
    for (const DeletionResult& result : service.Delete(stored->profiles, plan, arguments.route))
    {
        const QString outcome = Succeeded(result.result) ? QString("deleted") : Explain(result.result);

        Out() << "  " << AsText(result.folder.filename()).leftJustified(40) << outcome << "\n";

        for (const std::filesystem::path& link : result.linksRemoved)
        {
            Out() << "      link removed  " << AsText(link) << "\n";
        }

        everythingWent = everythingWent && Succeeded(result.result);
    }

    Out().flush();

    return everythingWent ? 0 : 1;
}
