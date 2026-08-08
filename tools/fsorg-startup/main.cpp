#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "application/StartupService.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
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

    QString TargetState(const std::filesystem::path& target)
    {
        std::error_code error;

        return std::filesystem::exists(target, error) ? QString("      ") : QString("MISSING");
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-startup\n"
              << "\n"
              << "Finds the EXE.xml of every profile the way the app finds it, beside the UserCfg.opt and\n"
              << "by the exact name, and lists the startup entries it carries with their switch and their\n"
              << "target. It reports whether the simulator is running, which is what refuses a write.\n"
              << "\n"
              << "It never writes: not the file, not the backup. The write has its own tests, against a\n"
              << "copy in a temporary folder, because the real file belongs to the user.\n";
    }

    void ReportEntries(const StartupService& service)
    {
        const std::vector<StartupEntry> entries = service.Entries();

        Out() << "    entries    " << entries.size() << "\n";

        for (const StartupEntry& entry : entries)
        {
            Out() << "    " << (entry.enabled ? "on " : "off") << "  " << TargetState(entry.path) << "  "
                  << QString::fromStdString(entry.label).leftJustified(44) << AsText(entry.path) << "\n";
        }
    }
}

int main(int argc, char* argv[])
{
    const QCoreApplication application(argc, argv);

    if (QCoreApplication::arguments().contains("--help"))
    {
        ReportUsage();
        Out().flush();

        return 0;
    }

    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const std::optional<std::string> running = processProbe.RunningSimulator();

    Out() << "Simulator running: " << (running.has_value() ? QString::fromStdString(*running) : QString("no")) << "\n";

    const WindowsFilesystemProbe filesystemProbe;
    const std::vector<StartupFileLocation> locations = StartupFileLocations(WindowsUserCfgLocations(), filesystemProbe);

    Out() << "Startup files found: " << locations.size() << "\n";

    for (const StartupFileLocation& location : locations)
    {
        Out() << "\n" << VariantName(location.variant) << "  " << AsText(location.filePath) << "\n";
        Out() << "    backup     " << AsText(BackupOfStartupFile(location.filePath)) << "\n";

        ExeXmlStartupEntries startup(location.filePath);
        const StartupService service(startup, processProbe);

        ReportEntries(service);
    }

    Out().flush();

    return 0;
}
