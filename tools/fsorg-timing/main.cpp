#include <QtCore/QThread>
#include <QtWidgets/QApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>

#include <algorithm>
#include <ranges>
#include <vector>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "application/ProfileService.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "support/PathText.h"

#include "AppScroll.h"
#include "JournalScroll.h"
#include "SessionForMeasuring.h"
#include "application/Session.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/CommunityModel.h"
#include "view/library/AddonTreePage.h"
#include "view/community/CommunityPage.h"
#include "view/JournalPage.h"
#include "view/shell/MainWindow.h"
#include "view/quarantine/QuarantinePage.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/QtBackgroundRunner.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    constexpr qint64 kBudgetForTheMainThread = 300;

    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    struct Measurement
    {
        QString stage;
        bool onTheMainThread = false;
        qint64 elapsed = 0;
    };

    std::vector<Measurement> measurements;

    template<typename Work>
    void Measure(const QString& stage, const bool onTheMainThread, Work&& work)
    {
        QElapsedTimer timer;
        timer.start();

        std::forward<Work>(work)();

        measurements.push_back({.stage = stage, .onTheMainThread = onTheMainThread, .elapsed = timer.elapsed()});
    }

    const SimulatorProfile* ActiveProfile(const AppSettings& settings)
    {
        const auto match = std::ranges::find_if(settings.profiles,
                                                [&settings](const SimulatorProfile& profile)
                                                {
                                                    return profile.id == settings.activeProfileId;
                                                });

        if (match != settings.profiles.end())
        {
            return &*match;
        }

        return settings.profiles.empty() ? nullptr : &settings.profiles.front();
    }

    qint64 MainThreadTotal()
    {
        qint64 total = 0;
        for (const Measurement& measurement : measurements)
        {
            total += measurement.onTheMainThread ? measurement.elapsed : 0;
        }

        return total;
    }

    void Report()
    {
        Out() << "\n"
              << QStringLiteral("etapa").leftJustified(34) << QStringLiteral("thread").leftJustified(10) << "ms\n";

        for (const Measurement& measurement : measurements)
        {
            Out() << measurement.stage.leftJustified(34)
                  << QString(measurement.onTheMainThread ? "principal" : "worker").leftJustified(10)
                  << measurement.elapsed << "\n";
        }

        Out() << "\ntotal on the main thread: " << MainThreadTotal() << " ms (budget " << kBudgetForTheMainThread
              << " ms)\n";
        Out() << (MainThreadTotal() > kBudgetForTheMainThread ? "VERMELHO: a interface congela\n" : "VERDE\n");
        Out().flush();
    }
}

int main(int argc, char* argv[])
{
    const QApplication application(argc, argv);
    if (!QCoreApplication::arguments().contains(QStringLiteral("-style")))
    {
        QApplication::setStyle(QStringLiteral("windows11"));
    }

    JsonSettingsRepository settings(SettingsFilePath());
    JsonlOperationJournal journal(JournalFilePath());

    const AppSettings loaded = settings.Load().value_or(AppSettings{});
    const SimulatorProfile* active = ActiveProfile(loaded);

    if (active == nullptr)
    {
        Out() << "nenhum perfil configurado em " << AsText(SettingsFilePath()) << "\n";
        Out().flush();
        return 2;
    }

    const SimulatorProfile profile = *active;

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser, filesystemProbe);
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);
    ProfileService profileService(catalog, classifier, linking, log, identities, LinkType::Junction);

    const ImportEngine importEngine(filesystemProbe, files, linking, log, LinkType::Junction);
    const ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, linking, log,
                                      LinkType::Junction);
    const LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                                     LinkType::Junction);

    if (QCoreApplication::arguments().contains(QStringLiteral("--journal-scroll")))
    {
        const NoLibrariesToScan nothingToScan;
        ProfileService justTheProfile(nothingToScan, classifier, linking, log, identities, LinkType::Junction);
        OneProfileRepository onlySettings(profile);
        InlineRunner runInline;
        SilentObserver silent;
        Session session(justTheProfile, organizer, onlySettings, processProbe, runInline, silent);
        session.ShowActiveProfile();

        return MeasureTheJournalScroll(journal, session);
    }

    if (QCoreApplication::arguments().contains(QStringLiteral("--app-journal")))
    {
        MainWindow window(loaded);
        QtBackgroundRunner runner;
        SessionNotifier notifier;
        Session session(profileService, organizer, settings, processProbe, runner, notifier);

        AddonTreeModel treeModel;
        AddonTreeViewModel treeViewModel(session, profileService, treeModel, notifier);
        auto* treePage = new AddonTreePage(treeViewModel, treeModel, notifier);

        ImportViewModel importViewModel(importService, profileService, processProbe, session, runner);

        CommunityModel communityModel;
        CommunityViewModel communityViewModel(profileService, session, notifier, communityModel);
        auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

        QuarantineModel quarantineModel;
        QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel);
        auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

        JournalModel journalModel;
        JournalViewModel journalViewModel(journal, session, journalModel);
        auto* journalPage = new JournalPage(journalViewModel, journalModel);

        window.AddPage("Library", treePage);
        window.AddPage("Destinations", communityPage);
        window.AddPage("Quarantine", quarantinePage);
        window.AddPage("Journal", journalPage);

        treeViewModel.ShowActiveProfile();
        for (int pass = 0; pass < 400 && session.Snapshot().entries.empty(); ++pass)
        {
            QApplication::processEvents();
            QThread::msleep(5);
        }

        return MeasureTheAppJournal(window, *journalPage, journalViewModel, journalModel);
    }

    AddonTreeModel model;
    CommunityModel communityModel;
    InlineRunner runInline;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, processProbe, runInline, notifier);
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel);

    Measure("Session::ShowActiveProfile", false,
            [&]
            {
                session.ShowActiveProfile();
            });

    Measure("AddonTreeModel::Show", true,
            [&]
            {
                model.Show(session.Snapshot(), session.Profile());
            });
    Measure("CommunityViewModel::Show", true,
            [&]
            {
                communityViewModel.Show();
            });
    Measure("ImportService::Leftovers", true,
            [&]
            {
                static_cast<void>(importService.Leftovers(profile));
            });
    Measure("ImportService::Quarantined", true,
            [&]
            {
                static_cast<void>(importService.Quarantined(profile));
            });

    Out() << "perfil: " << QString::fromStdString(profile.id) << "  bibliotecas: " << profile.libraries.size()
          << "  entradas: " << session.Snapshot().entries.size() << "\n";

    Report();

    return MainThreadTotal() > kBudgetForTheMainThread ? 1 : 0;
}
