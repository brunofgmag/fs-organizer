#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <vector>

#include "application/SizeService.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/platform/SystemClock.h"
#include "support/PathText.h"
#include "support/SizeText.h"

namespace
{
    std::atomic<bool> gCancelled{false};

    BOOL WINAPI OnConsoleSignal(DWORD)
    {
        gCancelled = true;

        return TRUE;
    }

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
        std::vector<std::filesystem::path> libraries;
        std::size_t stopAfter = 0;
        bool categoriesOnly = false;
    };

    Arguments Parse(const QStringList& arguments)
    {
        Arguments parsed;

        for (int index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] == "--categories-only")
            {
                parsed.categoriesOnly = true;
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

            if (arguments[index] == "--stop-after")
            {
                parsed.stopAfter = arguments[index + 1].toULongLong();
            }
        }

        return parsed;
    }

    void ReportUsage()
    {
        Out() << "usage: fsorg-size --library <path> [--library <path>]... [--categories-only] [--stop-after <n>]\n"
              << "\n"
              << "Sums the disk each category and each addon of a library occupies, against the real\n"
              << "installation, through the same application service the diagnostics screen uses.\n"
              << "Progress advances on one line while it walks; Ctrl+C stops it, and what comes out\n"
              << "is reported as incomplete instead of passing partial sums for totals.\n"
              << "\n"
              << "  --library <path>    a library root to measure; repeat for more than one\n"
              << "  --categories-only   leave the addons out and print category totals only\n"
              << "  --stop-after <n>    cancel on its own after n addons, to exercise the same path\n"
              << "                      Ctrl+C takes without anyone having to press it\n";
    }

    void ReportSize(const MeasuredNode& node, const QString& label, const int indent)
    {
        const QString size = node.measured || node.bytes > 0 ? AsSize(node.bytes) : QString("never reached");
        const QString qualifier = node.measured ? QString() : QString("   (incomplete)");

        Out() << QString(indent, ' ') << label.leftJustified(50 - indent) << size.rightJustified(12) << qualifier
              << "\n";
    }

    void ReportNode(const MeasuredNode& node, const int depth, const bool categoriesOnly)
    {
        if (node.kind == TreeNodeKind::Addon && categoriesOnly)
        {
            return;
        }

        ReportSize(node, AsText(node.path.filename()), 2 + depth * 2);

        for (const MeasuredNode& child : node.children)
        {
            ReportNode(child, depth + 1, categoriesOnly);
        }
    }

    void ReportLibrary(const MeasuredNode& library, const bool categoriesOnly)
    {
        Out() << "\nLibrary " << AsText(library.path) << "\n";
        ReportSize(library, "total", 2);
        Out() << "\n";

        for (const MeasuredNode& child : library.children)
        {
            ReportNode(child, 0, categoriesOnly);
        }
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

    SetConsoleCtrlHandler(OnConsoleSignal, TRUE);

    const JsonManifestParser manifestParser;
    const WindowsFilesystemProbe filesystemProbe;
    const JsonlOperationJournal journal(JournalFilePath());
    const JournalImportedFolders importedFolders(journal);
    const FilesystemScanner scanner(manifestParser, filesystemProbe, importedFolders);
    const SystemClock clock;
    RunHereAndNow runner;
    SizeService service(scanner, filesystemProbe, clock, runner);

    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();

    SizeReport report;
    service.Measure(
        arguments.libraries, service.NewCaller(), Freshness::MeasureAgain,
        [&arguments](const SizeProgress& progress)
        {
            Out() << "\r  " << QString::number(progress.measured + 1) << "/" << QString::number(progress.total) << "  "
                  << AsText(progress.folder.filename()).leftJustified(56).left(56);
            Out().flush();

            return !gCancelled && (arguments.stopAfter == 0 || progress.measured < arguments.stopAfter);
        },
        [&report](const SizeReport& measured)
        {
            report = measured;
        });

    const std::chrono::milliseconds elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

    Out() << "\r" << QString(72, ' ') << "\r";
    Out() << "Measured in " << QString::number(static_cast<double>(elapsed.count()) / 1000.0, 'f', 2) << " s\n";

    if (!report.complete)
    {
        Out() << "Cancelled: the numbers below are incomplete, and what was never reached says so.\n";
    }

    for (const MeasuredNode& library : report.libraries)
    {
        ReportLibrary(library, arguments.categoriesOnly);
    }

    Out().flush();

    return report.complete ? 0 : 1;
}
