#include <QtCore/QThread>
#include <QtWidgets/QApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>

#include <algorithm>
#include <ranges>
#include <vector>

#include "application/ImportService.h"
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
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/CommunityModel.h"
#include "view/AddonTreePage.h"
#include "view/CommunityPage.h"
#include "view/JournalPage.h"
#include "view/MainWindow.h"
#include "view/QuarantinePage.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/QuarantineViewModel.h"

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

    template <typename Work>
    void Measure(const QString& stage, const bool onTheMainThread, Work&& work)
    {
        QElapsedTimer timer;
        timer.start();

        std::forward<Work>(work)();

        measurements.push_back({stage, onTheMainThread, timer.elapsed()});
    }

    const SimulatorProfile* ActiveProfile(const AppSettings& settings)
    {
        const auto match = std::ranges::find_if(settings.profiles, [&settings](const SimulatorProfile& profile)
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
            << QStringLiteral("etapa").leftJustified(34)
            << QStringLiteral("thread").leftJustified(10)
            << "ms\n";

        for (const Measurement& measurement : measurements)
        {
            Out() << measurement.stage.leftJustified(34)
                << QString(measurement.onTheMainThread ? "principal" : "worker").leftJustified(10)
                << measurement.elapsed << "\n";
        }

        Out() << "\ntotal na thread principal: " << MainThreadTotal() << " ms (orçamento "
            << kBudgetForTheMainThread << " ms)\n";
        Out() << (MainThreadTotal() > kBudgetForTheMainThread
                      ? "VERMELHO: a interface congela\n"
                      : "VERDE\n");
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

    const AppSettings loaded = settings.Load();
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
    ProfileService profileService(catalog, classifier, linking, journal, clock, identities, LinkType::Junction);

    const ImportEngine importEngine(filesystemProbe, files, linking, journal, clock, LinkType::Junction);
    const ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, linking,
                                      journal, clock, LinkType::Junction);

    if (QCoreApplication::arguments().contains(QStringLiteral("--journal-scroll")))
    {
        return MeasureTheJournalScroll(journal, profile);
    }

    if (QCoreApplication::arguments().contains(QStringLiteral("--app-journal")))
    {
        MainWindow window(loaded);
        AddonTreeModel treeModel;
        treeModel.ShowProfile(profile);

        AddonTreeViewModel treeViewModel(profileService, settings, processProbe, treeModel);
        auto* treePage = new AddonTreePage(treeViewModel, treeModel);

        ImportViewModel importViewModel(importService, profileService, processProbe, treeModel);

        CommunityModel communityModel;
        CommunityViewModel communityViewModel(profileService, treeModel, communityModel);
        auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

        QuarantineModel quarantineModel;
        QuarantineViewModel quarantineViewModel(importService, profileService, treeModel, quarantineModel);
        auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

        JournalModel journalModel;
        JournalViewModel journalViewModel(journal, treeModel, journalModel);
        auto* journalPage = new JournalPage(journalViewModel, journalModel);

        window.AddPage(QStringLiteral("Árvore"), treePage);
        window.AddPage(QStringLiteral("Community"), communityPage);
        window.AddPage(QStringLiteral("Quarentena"), quarantinePage);
        window.AddPage(QStringLiteral("Diário"), journalPage);

        QObject::connect(&treeViewModel, &AddonTreeViewModel::ScanFinished, &communityViewModel,
                         &CommunityViewModel::Show);

        treeViewModel.ShowActiveProfile();
        for (int pass = 0; pass < 400 && treeModel.Snapshot().entries.empty(); ++pass)
        {
            QApplication::processEvents();
            QThread::msleep(5);
        }

        return MeasureTheAppJournal(window, *journalPage, journalViewModel, journalModel);
    }

    AddonTreeModel model;
    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, model, communityModel);

    ProfileSnapshot snapshot;
    Measure("ProfileService::Scan", false, [&] { snapshot = profileService.Scan(profile); });

    Measure("AddonTreeModel::ShowSnapshot", true, [&] { model.ShowSnapshot(snapshot, profile); });
    Measure("CommunityViewModel::Show", true, [&] { communityViewModel.Show(); });
    Measure("ImportService::Leftovers", true, [&]
    {
        static_cast<void>(importService.Leftovers(profile));
    });
    Measure("ImportService::Quarantined", true, [&]
    {
        static_cast<void>(importService.Quarantined(profile));
    });

    Out() << "perfil: " << QString::fromStdString(profile.id) << "  bibliotecas: "
        << profile.libraries.size() << "  entradas: " << snapshot.entries.size() << "\n";

    Report();

    return MainThreadTotal() > kBudgetForTheMainThread ? 1 : 0;
}
