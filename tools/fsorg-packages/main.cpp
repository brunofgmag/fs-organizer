#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <vector>

#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ContentXmlPackages.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "support/MomentText.h"
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

    QString PresenceName(const PackagePresence presence)
    {
        switch (presence)
        {
        case PackagePresence::Present: return "Present";
        case PackagePresence::Absent: return "Absent";
        case PackagePresence::Unverifiable: return "Unverifiable";
        }

        return "?";
    }

    std::vector<QString> ParsePackageArguments(const QStringList& arguments)
    {
        std::vector<QString> names;

        for (int index = 1; index + 1 < arguments.size(); ++index)
        {
            if (arguments[index] == "--package")
            {
                names.push_back(arguments[index + 1]);
            }
        }

        return names;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-packages [--package <bare name>]...\n"
              << "\n"
              << "Finds the package list of every profile the way the app finds it, reports when each\n"
              << "list was written and how much it carries, and answers presence for the names given.\n"
              << "Ask by the bare name: the generation prefixes belong to the adapter, not to the caller.\n";
    }

    void ReportList(const ContentXmlPackages& packages, const std::vector<QString>& names)
    {
        Out() << "    written    "
              << (packages.ListTakenAt().has_value() ? AsMoment(*packages.ListTakenAt()) : QString("unknown"))
              << ", and a list nobody could read carries no day\n";

        for (const QString& name : names)
        {
            Out() << "    " << name.leftJustified(48) << PresenceName(packages.PresenceOf(name.toStdString())) << "\n";
        }
    }

    void ReportNothingFound(const WindowsFilesystemProbe& filesystemProbe, const std::vector<QString>& names)
    {
        Out() << "\nNo package list was found under any profile.\n";

        const ContentXmlPackages packages(filesystemProbe, {});

        for (const QString& name : names)
        {
            Out() << "    " << name.leftJustified(48) << PresenceName(packages.PresenceOf(name.toStdString())) << "\n";
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

    const std::vector<QString> names = ParsePackageArguments(QCoreApplication::arguments());

    const WindowsFilesystemProbe filesystemProbe;
    const std::vector<ContentListLocation> locations = ContentListLocations(WindowsUserCfgLocations(), filesystemProbe);

    Out() << "Package lists found: " << locations.size() << "\n";

    if (locations.empty())
    {
        ReportNothingFound(filesystemProbe, names);
        Out().flush();

        return 0;
    }

    for (const ContentListLocation& location : locations)
    {
        Out() << "\n" << VariantName(location.variant) << "  " << AsText(location.listPath) << "\n";

        const ContentXmlPackages packages(filesystemProbe, location.listPath);
        ReportList(packages, names);
    }

    Out().flush();

    return 0;
}
