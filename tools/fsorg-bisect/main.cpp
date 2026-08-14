#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "application/BisectionService.h"
#include "application/model/AppSettings.h"
#include "domain/tree/LibraryTrees.h"
#include "infrastructure/bisection/JsonBisectionStore.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "support/PathText.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);

        return stream;
    }

    enum class Command : int
    {
        None = 0,
        Units = 1,
        Status = 2,
        Begin = 3,
        Crashed = 4,
        RanFine = 5,
        Refine = 6,
        Stop = 7,
        Carry = 8,
        Forget = 9,
    };

    struct Arguments
    {
        Command command = Command::None;
        bool go = false;
    };

    Arguments Parse(const QStringList& given)
    {
        Arguments parsed;

        for (const QString& argument : given)
        {
            if (argument == "--go")
            {
                parsed.go = true;
            }

            if (argument == "--units")
            {
                parsed.command = Command::Units;
            }

            if (argument == "--status")
            {
                parsed.command = Command::Status;
            }

            if (argument == "--begin")
            {
                parsed.command = Command::Begin;
            }

            if (argument == "--crashed")
            {
                parsed.command = Command::Crashed;
            }

            if (argument == "--ran-fine")
            {
                parsed.command = Command::RanFine;
            }

            if (argument == "--refine")
            {
                parsed.command = Command::Refine;
            }

            if (argument == "--stop")
            {
                parsed.command = Command::Stop;
            }

            if (argument == "--carry-on")
            {
                parsed.command = Command::Carry;
            }

            if (argument == "--forget")
            {
                parsed.command = Command::Forget;
            }
        }

        return parsed;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-bisect <command> [--go]\n"
              << "\n"
              << "Drives the whole halving search against the real installation, through the same\n"
              << "application service the Find the culprit screen will use. Without --go nothing is\n"
              << "written: it prints the round it would apply, which is how the guards can be read\n"
              << "before a button exists. It reads the real settings.json and never saves it back.\n"
              << "\n"
              << "  --units       scan the coupling of what is enabled now and time it\n"
              << "  --status      say where the procedure is, if one is running\n"
              << "  --begin       build the units and apply the reference round, with nothing on\n"
              << "  --crashed     answer that the simulator fell, and narrow\n"
              << "  --ran-fine    answer that it ran, and narrow\n"
              << "  --refine      open the second pass inside the group that was left\n"
              << "  --stop        put the starting configuration back and end the procedure\n"
              << "  --carry-on    apply the round the state file says, after the app was killed\n"
              << "  --forget      drop the procedure and leave the disk as it is\n"
              << "  --go          actually write: the state file, and the links of the round\n";
    }

    QString NameOf(const BisectionOutcome outcome)
    {
        switch (outcome)
        {
        case BisectionOutcome::StillSearching: return "still searching";
        case BisectionOutcome::OneAddonLeft: return "one addon left";
        case BisectionOutcome::AnIrreducibleSet: return "an irreducible set";
        case BisectionOutcome::NotAmongTheManagedOnes: return "not among the ones the app manages";
        }

        return "unknown";
    }

    QString NameOf(const BisectionRefusal refusal)
    {
        switch (refusal)
        {
        case BisectionRefusal::None: return "none";
        case BisectionRefusal::NothingIsEnabledToSearch: return "nothing is enabled to search";
        case BisectionRefusal::TheStateCouldNotBeWritten: return "the state could not be written";
        case BisectionRefusal::TheDiskMovedSinceTheLastRound: return "the disk moved since the last round";
        case BisectionRefusal::NoProcedureIsRunning: return "no procedure is running";
        case BisectionRefusal::ThisUnitDoesNotSplit: return "this unit does not split";
        }

        return "unknown";
    }

    QString NameOf(const DriftKind kind)
    {
        switch (kind)
        {
        case DriftKind::ALinkWeLeftIsGone: return "a link the app left is gone";
        case DriftKind::AnEntryWeDidNotLeaveIsThere: return "an entry the app did not leave is there";
        case DriftKind::AnEntryPointsSomewhereElse: return "an entry points somewhere else";
        case DriftKind::AnAddonLeftTheLibrary: return "an addon left the library";
        case DriftKind::AnAddonJoinedTheLibrary: return "an addon joined the library";
        }

        return "unknown";
    }

    void ReportUnits(const std::vector<SearchUnit>& units)
    {
        std::size_t addons = 0;
        std::size_t groups = 0;

        for (const SearchUnit& unit : units)
        {
            addons += unit.addons.size();

            if (unit.addons.size() > 1)
            {
                ++groups;
            }
        }

        const auto TheKind = [](const Coupling coupling)
        {
            switch (coupling)
            {
            case Coupling::Merge: return QString("merge, no file claimed twice");
            case Coupling::Shadowing: return QString("shadowing, a file is claimed twice");
            case Coupling::OnlyTheSharedModelFolder: return QString("held only by the shared model folder name");
            case Coupling::Alone:
            case Coupling::NotYetMeasured: break;
            }

            return QString("kind not measured");
        };

        Out() << units.size() << " units over " << addons << " enabled addons, " << groups
              << " of them coupled groups\n";
        Out() << "worst case: " << RoundsInTheWorstCase(units.size()) << " rounds after the reference one\n\n";

        for (const SearchUnit& unit : units)
        {
            if (unit.addons.size() == 1)
            {
                Out() << "  " << AsText(unit.addons.front().filename()) << "\n";

                continue;
            }

            const QString base =
                unit.base.has_value() ? AsText(unit.base->filename()) : QString("no base that can be told apart");

            Out() << "  a group of " << unit.addons.size() << ", " << TheKind(unit.coupling) << ", base " << base
                  << "\n";

            for (const std::filesystem::path& member : unit.addons)
            {
                Out() << "      " << AsText(member.filename()) << "\n";
            }
        }
    }

    void ReportRun(const BisectionRun& run)
    {
        Out() << "round " << run.round << ", " << run.suspects.size() << " units under suspicion, "
              << run.cleared.size() << " cleared\n";
        Out() << "pass: " << (run.pass == BisectionPass::InsideTheGroup ? "inside the group" : "over the units")
              << "\n";
        Out() << "outcome: " << NameOf(OutcomeOf(run)) << "\n";

        const BisectionRound round = TheRound(run);

        Out() << "\nthe round on the disk turns on " << round.addonsOn.size() << " addons:\n";

        for (const std::filesystem::path& addon : round.addonsOn)
        {
            Out() << "  " << AsText(addon.filename()) << "\n";
        }

        Out() << "\nthe starting configuration holds " << run.startingConfiguration.size() << " addons\n";
    }

    void ReportOutcome(const BisectionReport& report)
    {
        Out() << "refusal: " << NameOf(report.refusal) << "\n";

        for (const Divergence& divergence : report.drift)
        {
            Out() << "  drift: " << NameOf(divergence.kind) << " at " << AsText(divergence.path) << "\n";
        }

        Out() << "round " << report.round << " of at most " << report.roundsInTheWorstCase << ", over " << report.units
              << " units\n";
        Out() << "outcome: " << NameOf(report.outcome) << "\n";
        Out() << "entries carrying on out of reach: " << report.outOfReach << "\n";

        if (!report.addonsTurnedOn.empty())
        {
            Out() << "\nturned on:\n";

            for (const std::filesystem::path& addon : report.addonsTurnedOn)
            {
                Out() << "  " << AsText(addon.filename()) << "\n";
            }
        }

        if (!report.whatIsLeft.empty())
        {
            Out() << "\nwhat the search narrowed down to, which the app never calls the culprit:\n";

            for (const std::filesystem::path& addon : report.whatIsLeft)
            {
                Out() << "  " << AsText(addon.filename()) << "\n";
            }
        }

        if (report.aSecondPassIsPossible)
        {
            Out() << "\na second pass inside the group is possible: run --refine\n";
        }

        Out() << "\n" << report.results.size() << " link operations went out as one batch\n";
    }

    struct World
    {
        WindowsLinkService linkService;
        WindowsFilesystemProbe filesystemProbe;
        WindowsSidecarStore sidecars;
        JsonManifestParser manifestParser;
        FilesystemScanner catalog{manifestParser, filesystemProbe};
        SystemClock clock;
        JsonlOperationJournal journal{JournalFilePath()};
        OperationLog log{journal, clock};
        UuidLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ExeXmlStartupEntries startupEntries{{}};
        WindowsProcessProbe processProbe{{"FlightSimulator.exe", "FlightSimulator2024.exe"}};
        StartupService startup{startupEntries, processProbe, filesystemProbe, false};

        ProfileService profiles{catalog, filesystemProbe, sidecars, classifier,        linking,
                                log,     identities,      startup,  LinkType::Junction};
        CouplingScan coupling{filesystemProbe};
        JsonBisectionStore store{BisectionFolderPath()};
        BisectionService service{profiles, coupling, filesystemProbe, store};
    };
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    const Arguments arguments = Parse(QCoreApplication::arguments());

    const bool asked = QCoreApplication::arguments().contains("--help");

    if (arguments.command == Command::None || asked)
    {
        ReportUsage();
        Out().flush();

        return asked ? 0 : 2;
    }

    const JsonSettingsRepository settings(SettingsFilePath());
    const std::optional<AppSettings> stored = settings.Load();

    if (!stored.has_value() || stored->profiles.empty())
    {
        Out() << "no profile configured, so there is nothing to bisect\n";
        Out().flush();

        return 1;
    }

    const auto active = std::ranges::find_if(stored->profiles,
                                             [&stored](const SimulatorProfile& one)
                                             {
                                                 return one.id == stored->activeProfileId;
                                             });

    const SimulatorProfile& profile = active == stored->profiles.end() ? stored->profiles.front() : *active;

    World world;

    if (arguments.command == Command::Units)
    {
        const ProfileSnapshot snapshot = world.profiles.Scan(profile);
        const std::vector<std::filesystem::path> enabled = EnabledAddonFolders(snapshot.entries);

        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        const std::vector<CouplingFacts> facts = world.coupling.FactsAbout(enabled);
        const std::chrono::steady_clock::time_point grouped = std::chrono::steady_clock::now();
        const std::vector<SearchUnit> units = world.coupling.WithTheKindOfEachGroup(facts, UnitsFrom(facts));
        const std::chrono::steady_clock::time_point ended = std::chrono::steady_clock::now();

        ReportUnits(units);

        Out() << "\nthe coupling scan of " << enabled.size() << " enabled addons took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count() << " ms\n";
        Out().flush();

        return 0;
    }

    const std::optional<BisectionRun> running = world.service.WhatWasInterrupted(profile.id);

    if (arguments.command == Command::Status)
    {
        if (!running.has_value())
        {
            Out() << "no procedure is running for the profile " << QString::fromStdString(profile.id) << "\n";
            Out().flush();

            return 0;
        }

        ReportRun(*running);
        Out().flush();

        return 0;
    }

    if (!arguments.go)
    {
        Out() << "Nothing was written. This is what the next step would do.\n\n";

        if (arguments.command == Command::Begin)
        {
            const ProfileSnapshot snapshot = world.profiles.Scan(profile);
            const std::vector<std::filesystem::path> enabled = EnabledAddonFolders(snapshot.entries);

            const std::vector<CouplingFacts> facts = world.coupling.FactsAbout(enabled);

            ReportUnits(world.coupling.WithTheKindOfEachGroup(facts, UnitsFrom(facts)));
            Out() << "\nthe reference round turns everything off, and it is the first thing applied\n";
            Out().flush();

            return 0;
        }

        if (!running.has_value())
        {
            Out() << "no procedure is running for the profile " << QString::fromStdString(profile.id) << "\n";
            Out().flush();

            return 1;
        }

        if (arguments.command == Command::Crashed || arguments.command == Command::RanFine)
        {
            const BisectionAnswer answer =
                arguments.command == Command::Crashed ? BisectionAnswer::ItCrashed : BisectionAnswer::ItRanFine;

            ReportRun(AfterAnswering(*running, answer));
            Out().flush();

            return 0;
        }

        if (arguments.command == Command::Refine)
        {
            if (!ASecondPassIsPossible(*running))
            {
                Out() << "this unit does not split: there is no base that can be told apart\n";
                Out().flush();

                return 1;
            }

            ReportRun(IntoTheSecondPass(*running));
            Out().flush();

            return 0;
        }

        Out() << "it would put back the " << running->startingConfiguration.size()
              << " addons of the starting configuration and drop the state file\n";
        Out().flush();

        return 0;
    }

    BisectionReport report;

    switch (arguments.command)
    {
    case Command::Begin: report = world.service.Begin(profile, world.profiles.Scan(profile)); break;
    case Command::Crashed: report = world.service.Answer(profile, BisectionAnswer::ItCrashed); break;
    case Command::RanFine: report = world.service.Answer(profile, BisectionAnswer::ItRanFine); break;
    case Command::Refine: report = world.service.Refine(profile); break;
    case Command::Stop: report = world.service.Stop(profile); break;
    case Command::Carry: report = world.service.Resume(profile, ResumeChoice::CarryOnFromWhereItStopped); break;
    case Command::Forget: report = world.service.Resume(profile, ResumeChoice::ForgetItAndLeaveTheDiskAsItIs); break;
    case Command::None:
    case Command::Units:
    case Command::Status: break;
    }

    ReportOutcome(report);
    Out().flush();

    return report.refusal == BisectionRefusal::None ? 0 : 1;
}
