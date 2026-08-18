#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>
#include <QtGui/QStyleHints>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

#include "application/DeletionService.h"
#include "application/ImportService.h"
#include "application/SizeService.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/BisectionService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonChartCatalogueParser.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/documents/JsonDocumentIndexCache.h"
#include "infrastructure/documents/QtPdfChartVersions.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "application/CoverageService.h"
#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "infrastructure/scenery/BglSceneryParser.h"
#include "infrastructure/scenery/JsonSceneryCache.h"
#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ContentXmlPackageList.h"
#include "viewmodel/CoverageViewModel.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/LoadingReportLocations.h"
#include "infrastructure/sim/ProfileLoadingReport.h"
#include "infrastructure/sim/ProfilePackages.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "infrastructure/update/GithubUpdateService.h"
#include "shared/DisposableState.h"
#include "support/PathText.h"
#include "view/JournalPage.h"
#include "view/PresetsPage.h"
#include "view/community/CommunityPage.h"
#include "domain/tree/AddonTree.h"
#include "view/documents/DocumentsPage.h"
#include "view/library/AddonTreePage.h"
#include "domain/support/PathUtils.h"
#include "view/library/LibraryRootDialog.h"
#include "view/community/ImportDialog.h"
#include "view/library/StartupEntryDialog.h"
#include "view/library/SwapDialog.h"
#include "view/options/OptionsPage.h"
#include "infrastructure/bisection/JsonBisectionStore.h"
#include "view/diagnostics/DiagnosticsPage.h"
#include "view/simulator/PackageListPage.h"
#include "view/simulator/SimulatorPage.h"
#include "view/simulator/StartupPage.h"
#include "view/quarantine/QuarantinePage.h"
#include "view/shell/ImportProgressDialog.h"
#include "view/shell/LanguageSwitch.h"
#include "view/shell/MainWindow.h"
#include "view/shell/PageNames.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/DeletionViewModel.h"
#include "viewmodel/AddonDocumentsViewModel.h"
#include "viewmodel/DocumentsViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/BisectionViewModel.h"
#include "viewmodel/DiagnosticsViewModel.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/StartupViewModel.h"
#include "viewmodel/UpdateViewModel.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    class RunsRightHere final : public BackgroundRunner
    {
    public:
        void Run(const std::function<void()> work, const std::function<void()> doneOnTheCallingThread) override
        {
            work();
            doneOnTheCallingThread();
        }
    };

    void LetTheLayoutSettle()
    {
        for (int pass = 0; pass < 4; ++pass)
        {
            QCoreApplication::processEvents();
        }
    }

    QAbstractItemView* TheViewThatCarriesTheRows(const QWidget& page)
    {
        if (auto* tree = page.findChild<QTreeView*>(); tree != nullptr)
        {
            tree->expandAll();

            return tree;
        }

        return page.findChild<QTableView*>();
    }

    void KeepTheEventLoopTurning(const int passes)
    {
        for (int pass = 0; pass < passes; ++pass)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            QThread::msleep(20);
        }
    }

    void ClickTheRow(QTreeWidget& index, const QTreeWidgetItem* line)
    {
        const QPointF at = index.visualItemRect(line).center();
        const QPointF onTheScreen = index.viewport()->mapToGlobal(at);

        QMouseEvent press(QEvent::MouseButtonPress, at, onTheScreen, Qt::LeftButton, Qt::LeftButton, {});
        QMouseEvent release(QEvent::MouseButtonRelease, at, onTheScreen, Qt::LeftButton, Qt::NoButton, {});

        QApplication::sendEvent(index.viewport(), &press);
        QApplication::sendEvent(index.viewport(), &release);
    }

    QTreeWidgetItem* TheDeepestFirstLineOf(QTreeWidgetItem* heading)
    {
        if (heading->childCount() == 0)
        {
            return heading;
        }

        heading->setExpanded(true);

        return TheDeepestFirstLineOf(heading->child(0));
    }

    QList<QTreeWidgetItem*> EveryLineUnderAGroupOf(QTreeWidget& index)
    {
        QList<QTreeWidgetItem*> lines;
        QList<QTreeWidgetItem*> pending;

        for (int group = 0; group < index.topLevelItemCount(); ++group)
        {
            pending.append(index.topLevelItem(group));
        }

        while (!pending.isEmpty())
        {
            QTreeWidgetItem* heading = pending.takeFirst();

            if (heading->childCount() > 0)
            {
                heading->setExpanded(true);
            }

            for (int within = 0; within < heading->childCount(); ++within)
            {
                pending.append(heading->child(within));
            }

            if (heading->childCount() == 0 && heading->parent() != nullptr)
            {
                lines.append(heading);
            }
        }

        return lines;
    }

    QTreeWidgetItem* TheFirstLineUnderAGroupOf(QTreeWidget& index)
    {
        for (int group = 0; group < index.topLevelItemCount(); ++group)
        {
            if (index.topLevelItem(group)->childCount() > 0)
            {
                return TheDeepestFirstLineOf(index.topLevelItem(group));
            }
        }

        return nullptr;
    }

    bool SelectTheAddonNamed(const QWidget& page, const QString& folderName)
    {
        QAbstractItemView* view = TheViewThatCarriesTheRows(page);
        if (view == nullptr || view->model() == nullptr)
        {
            return false;
        }

        const QModelIndexList found = view->model()->match(view->model()->index(0, 0, {}), Qt::DisplayRole, folderName,
                                                           1, Qt::MatchExactly | Qt::MatchRecursive);
        if (found.isEmpty())
        {
            return false;
        }

        view->setCurrentIndex(found.front());
        view->selectionModel()->select(found.front(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        view->scrollTo(found.front());

        return true;
    }

    bool SelectTheFirstRows(const QWidget& page, const int many)
    {
        QAbstractItemView* view = TheViewThatCarriesTheRows(page);
        if (view == nullptr || view->model() == nullptr)
        {
            return false;
        }

        QItemSelection chosen;
        QModelIndex last;

        for (int row = 0; row < many; ++row)
        {
            const QModelIndex position = view->model()->index(row, 0, {});
            if (!position.isValid())
            {
                break;
            }

            chosen.select(position, position);
            last = position;
        }

        if (!last.isValid())
        {
            return false;
        }

        view->setCurrentIndex(last);
        view->selectionModel()->select(chosen, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

        return true;
    }

    bool Save(QWidget& window, const QDir& folder, const QString& name)
    {
        const QString file = folder.filePath(name + ".png");
        const QPixmap shot = window.grab();

        if (!shot.save(file))
        {
            Out() << "could not write " << file << "\n";
            return false;
        }

        Out() << shot.width() << "x" << shot.height() << "  " << file << "\n";
        return true;
    }

    bool SaveTheDialogOpenedBy(const std::function<void()>& opensIt, const QDir& folder, const QString& name)
    {
        QPixmap shot;
        bool opened = false;

        QTimer::singleShot(0, QCoreApplication::instance(),
                           [&shot, &opened]
                           {
                               QWidget* dialog = QApplication::activeModalWidget();
                               if (dialog == nullptr)
                               {
                                   return;
                               }

                               opened = true;
                               LetTheLayoutSettle();
                               shot = dialog->grab();
                               dialog->close();
                           });

        opensIt();

        if (!opened)
        {
            Out() << "no modal dialog opened for " << name << "\n";
            return false;
        }

        const QString file = folder.filePath(name + ".png");
        if (!shot.save(file))
        {
            Out() << "could not write " << file << "\n";
            return false;
        }

        Out() << shot.width() << "x" << shot.height() << "  " << file << "\n";
        return true;
    }

    bool ClickingReaches(const QListWidget& navigation, const int row)
    {
        QWidget* viewport = navigation.viewport();
        const QPoint at = navigation.visualItemRect(navigation.item(row)).center();
        const QPointF global = viewport->mapToGlobal(at);

        QMouseEvent press(QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);

        QApplication::sendEvent(viewport, &press);
        QApplication::sendEvent(viewport, &release);

        return navigation.currentRow() == row;
    }

    QPushButton* ButtonNamed(const QWidget& page, const QString& objectName)
    {
        QPushButton* button = page.findChild<QPushButton*>(objectName);
        if (button == nullptr)
        {
            Out() << "no button called " << objectName << " on this page, so the shot it opens is missing\n";
        }

        return button;
    }

    QString TheFirstAddonOf(const ProfileSnapshot& snapshot)
    {
        for (const TreeNode& library : snapshot.libraries)
        {
            if (const std::vector<const TreeNode*> addons = AddonsUnder(library); !addons.empty())
            {
                return AsText(addons.front()->path.filename());
            }
        }

        return {};
    }

    std::optional<TakenPlace> APlaceWorthShowingAsTaken(const ProfileSnapshot& snapshot)
    {
        std::vector<const TreeNode*> addons;

        for (const TreeNode& library : snapshot.libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                addons.push_back(addon);
            }
        }

        if (addons.size() < 2)
        {
            return std::nullopt;
        }

        return TakenPlace{.addonFolder = addons[1]->path,
                          .linkPath = addons[0]->path.parent_path() / addons[0]->path.filename(),
                          .occupant = addons[0]->path};
    }

    std::vector<StartupLine> AStartupEntryWorthShowing(StartupViewModel& startup)
    {
        startup.Show();

        std::vector<StartupLine> carried;

        for (const StartupLine& line : startup.Lines())
        {
            if (line.enabled && line.reach == StartupReach::InsideAnAddon)
            {
                carried.push_back(line);
            }
        }

        return carried;
    }

    QSize SizeFrom(const QString& text)
    {
        const QStringList parts = text.split(QLatin1Char('x'), Qt::SkipEmptyParts);
        if (parts.size() != 2)
        {
            return {};
        }

        return {parts[0].toInt(), parts[1].toInt()};
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FS Organizer"));
    QApplication::setApplicationVersion(QStringLiteral(FSORG_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Writes a PNG of every FS Organizer screen, building the real widgets against a disposable copy of your "
        "settings, journal and presets, so a click that saves never reaches your install.\n"
        "For the Windows scale, run it with QT_SCALE_FACTOR=1.25.");
    parser.addHelpOption();

    const QCommandLineOption out({"o", "out"}, "Folder to write the PNGs into.", "folder", QDir::currentPath());
    const QCommandLineOption theme({"t", "theme"}, "dark, light or system.", "palette", "system");
    const QCommandLineOption size({"s", "size"}, "Window size, WIDTHxHEIGHT.", "size", "1140x760");
    const QCommandLineOption language({"l", "lang"}, "en or pt_BR.", "language", "en");
    const QCommandLineOption select({"S", "select"},
                                    "Folder name of an addon to select before the Library, Destinations, Journal "
                                    "and Quarantine shots, so "
                                    "the context panel is in the picture.",
                                    "addon folder", "");
    const QCommandLineOption batch({"b", "batch"},
                                   "How many top rows to select before the Library, Destinations and Quarantine "
                                   "shots, so the batch panel is in the picture. Wins over --select.",
                                   "rows", "0");
    parser.addOption(out);
    parser.addOption(theme);
    parser.addOption(size);
    parser.addOption(language);
    parser.addOption(select);
    parser.addOption(batch);
    parser.process(app);

    if (const QString wanted = parser.value(theme); wanted != QLatin1String("system"))
    {
        QGuiApplication::styleHints()->setColorScheme(wanted == QLatin1String("dark") ? Qt::ColorScheme::Dark
                                                                                      : Qt::ColorScheme::Light);
    }

    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
    {
        Out() << "the offscreen plugin applies neither the colour scheme nor the fill of list items, so the PNG "
                 "would come out similar and wrong. Run it without QT_QPA_PLATFORM.\n";
        return 1;
    }

    const QString wantedLanguage = parser.value(language);
    if (!LanguageSwitch::IsOffered(wantedLanguage))
    {
        Out() << "unknown language: " << wantedLanguage << ". Offered: ";
        for (const LanguageSwitch::Offer& offer : LanguageSwitch::Offered())
        {
            Out() << offer.code << " ";
        }
        Out() << "\n";
        return 1;
    }

    QTranslator interface;
    const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/i18n/app_%1").arg(wantedLanguage);
    if (!interface.load(beside))
    {
        Out() << "no catalogue at " << beside
              << ".qm, so every screen would come out in the source language and "
                 "the plurals in the singular. Build fsorg-shot again.\n";
        return 1;
    }

    QCoreApplication::installTranslator(&interface);

    QTranslator nativeWidgets;
    if (LanguageSwitch::LoadNativeWidgets(nativeWidgets, wantedLanguage))
    {
        QCoreApplication::installTranslator(&nativeWidgets);
    }
    else if (wantedLanguage != QLatin1String("en"))
    {
        Out() << "no Qt catalogue for " << wantedLanguage
              << ", so the standard buttons would come out in English while the app has them "
                 "translated. Build fsorg-shot again.\n";
        return 1;
    }

    ApplyModernistTheme(app);

    const QDir folder(parser.value(out));
    if (!folder.exists())
    {
        Out() << "no such folder: " << folder.path() << "\n";
        return 1;
    }

    const QSize window = SizeFrom(parser.value(size));
    if (window.isEmpty())
    {
        Out() << "invalid size: " << parser.value(size) << "\n";
        return 1;
    }

    const std::optional<DisposableState> staged = StageStateWhereWritingIsHarmless("fsorg-shot");
    if (!staged.has_value())
    {
        Out() << "could not stage a disposable copy of the state, so nothing ran\n";
        return 1;
    }

    Out() << "reading a copy, so your install is never written: " << AsText(staged->settingsFile.parent_path()) << "\n";

    JsonSettingsRepository settings(staged->settingsFile);

    const std::optional<AppSettings> stored = settings.Load();
    if (!stored.has_value())
    {
        Out() << "settings.json exists and could not be read: " << AsText(staged->settingsFile) << "\n";
        return 1;
    }

    if (stored->profiles.empty())
    {
        Out() << "no profile configured, so there is no screen with content to write\n";
        return 1;
    }

    WindowsLinkService linkService;
    const WindowsFilesystemProbe filesystemProbe;
    WindowsFileOperations files;
    WindowsSidecarStore sidecars;
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    JsonlOperationJournal journal(staged->journalFile);

    const JournalImportedFolders importedFolders(journal);

    const FilesystemScanner catalog(manifestParser, filesystemProbe, importedFolders);
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);

    const std::vector<StartupFileLocation> startupFiles =
        StartupFileLocations(WindowsUserCfgLocations(), filesystemProbe);
    ExeXmlStartupEntries startupEntries{{}};
    StartupService startupService(startupEntries, processProbe, filesystemProbe, true);

    ProfileService profileService(catalog, filesystemProbe, sidecars, classifier, linking, log, identities,
                                  startupService, stored->linkType);
    ImportEngine importEngine(filesystemProbe, files, sidecars, linking, log, stored->linkType, stored->verification);
    ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, sidecars, linking, log,
                                stored->linkType);
    LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                               stored->linkType);

    MainWindow shell(*stored);
    RunsRightHere runner;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, *stored, processProbe, runner, notifier);

    QObject::connect(&notifier, &SessionNotifier::ScanFinished, &shell,
                     [&session, &startupEntries, &startupFiles]
                     {
                         startupEntries.Use(StartupFileOf(startupFiles, session.Profile().variant));
                         session.RefreshStartupEntries();
                     });

    SizeService sizes(catalog, filesystemProbe, clock, runner);

    AddonTreeModel treeModel;
    ProfilePackages packages(filesystemProbe, ContentListLocations(WindowsUserCfgLocations(), filesystemProbe));
    packages.Reload(session.Profile().variant);
    AddonTreeViewModel treeViewModel(session, profileService, treeModel, packages, sizes, notifier);
    const DeletionService deletionService(filesystemProbe, files, sidecars, linking, classifier, processProbe, log,
                                          sizes);
    DeletionViewModel deletionViewModel(session, profileService, deletionService, sizes);
    ImportViewModel importViewModel(importService, profileService, processProbe, session, runner);

    ContentXmlPackageList packageList(
        ChooseContentList(ContentListLocations(WindowsUserCfgLocations(), filesystemProbe), session.Profile().variant)
            .value_or(ChosenContentList{})
            .listPath);
    CoverageService coverageService(packageList, processProbe, true);

    const BglSceneryParser sceneryParser;
    JsonSceneryCache sceneryCache(staged->settingsFile.parent_path() / "scenery-cache.json");
    SceneryService sceneryService(filesystemProbe, sceneryParser, clock, sceneryCache);
    CoverageViewModel coverageViewModel(coverageService, sceneryService, session, clock);

    const JsonChartCatalogueParser catalogueParser;
    const QtPdfChartVersions chartVersions;
    const DocumentService documentService(catalog, filesystemProbe, catalogueParser, chartVersions);
    AddonDocumentsViewModel addonDocumentsViewModel(documentService, sceneryService, session, runner);
    JsonDocumentIndexCache documentIndexCache(staged->settingsFile.parent_path() / "document-index.json");
    NoManualToFetch manual;

    DocumentsViewModel documentsViewModel(documentService, sceneryService, session, runner, documentIndexCache, manual,
                                          clock);
    auto* documentsPage = new DocumentsPage(documentsViewModel);

    auto* libraryPage = new AddonTreePage(treeViewModel, deletionViewModel, importViewModel, coverageViewModel,
                                          addonDocumentsViewModel, treeModel, notifier);

    CommunityModel communityModel;
    CommunityViewModel communityViewModel(profileService, session, notifier, communityModel, sizes);
    auto* communityPage = new CommunityPage(communityViewModel, importViewModel, communityModel);

    QuarantineModel quarantineModel;
    QuarantineViewModel quarantineViewModel(importService, profileService, session, notifier, quarantineModel, sizes,
                                            runner);
    auto* quarantinePage = new QuarantinePage(quarantineViewModel, quarantineModel);

    JournalModel journalModel;
    JournalViewModel journalViewModel(journal, session, journalModel);
    auto* journalPage = new JournalPage(journalViewModel, journalModel);

    FilePresetRepository presetRepository(staged->presetsFolder);
    PresetService presetService(presetRepository, profileService, startupService);
    PresetViewModel presetViewModel(session, presetService, profileService);
    auto* presetsPage = new PresetsPage(presetViewModel, notifier);

    ProfileLoadingReport loadingReport(
        filesystemProbe,
        LoadingReportOf(LoadingReportLocations(WindowsUserCfgLocations(), filesystemProbe), session.Profile().variant));

    DiagnosticsViewModel diagnosticsViewModel(importService, sizes, sceneryService, session, loadingReport, clock,
                                              runner);
    const CouplingScan coupling(filesystemProbe);
    JsonBisectionStore bisectionStore(staged->settingsFile.parent_path() / "bisection");
    BisectionService bisectionService(profileService, coupling, filesystemProbe, bisectionStore, clock);
    BisectionViewModel bisectionViewModel(bisectionService, session, runner);

    auto* diagnosticsPage = new DiagnosticsPage(diagnosticsViewModel, bisectionViewModel);

    StartupViewModel startupViewModel(startupService, session, clock);
    auto* startupPage = new StartupPage(startupViewModel);
    auto* packageListPage = new PackageListPage(coverageViewModel);
    auto* simulatorPage = new SimulatorPage(startupPage, packageListPage);

    GithubUpdateService updateService({}, QCoreApplication::applicationVersion(),
                                      QDir::tempPath() + QStringLiteral("/fsorg-shot-updates"));
    UpdateViewModel updateViewModel(updateService, UpdateMode::Notify, false);

    OptionsViewModel optionsViewModel(session, profileService, notifier);
    auto* optionsPage = new OptionsPage(optionsViewModel, updateViewModel, staged->settingsFile);

    PageTab* libraryTab = shell.AddPage(PageNames::kLibrary, libraryPage);
    PageTab* communityTab = shell.AddPage(PageNames::kDestinations, communityPage);
    PageTab* simulatorTab = shell.AddPage(PageNames::kSimulator, simulatorPage);
    PageTab* presetsTab = shell.AddPage(PageNames::kPresets, presetsPage);
    PageTab* documentsTab = shell.AddPage(PageNames::kDocuments, documentsPage);
    PageTab* quarantineTab = shell.AddPage(PageNames::kQuarantine, quarantinePage);
    PageTab* diagnosticsTab = shell.AddPage(PageNames::kDiagnostics, diagnosticsPage);
    PageTab* journalTab = shell.AddPage(PageNames::kJournal, journalPage);
    shell.CarryOptionsOn(optionsPage);

    shell.CarryTriageOn(libraryPage);
    shell.CarryTriageOn(communityPage);

    QObject::connect(&communityViewModel, &CommunityViewModel::BreakdownChanged, &shell,
                     [&shell](const AttentionBreakdown& breakdown)
                     {
                         shell.ShowTriage(breakdown);
                     });

    treeViewModel.ShowActiveProfile();

    shell.resize(window);
    shell.show();
    LetTheLayoutSettle();

    Out() << "window " << shell.size().width() << "x" << shell.size().height() << " at scale "
          << shell.devicePixelRatioF() << ", so every PNG comes out at that ratio.\n";

    if (shell.size() != window)
    {
        Out() << "the shell does not shrink to " << window.width() << "x" << window.height()
              << ": the content asks for more. Each line below says the size that was written.\n";
    }

    bool landed = true;

    const auto shoot = [&](PageTab* tab, const QString& name, const std::function<void()>& before)
    {
        tab->click();
        if (before)
        {
            before();
        }
        LetTheLayoutSettle();
        landed = Save(shell, folder, name) && landed;
    };

    const QString wantedAddon = parser.value(select);
    const int wantedRows = parser.value(batch).toInt();

    const auto selectIfAsked = [&wantedAddon, wantedRows](const QWidget& page, const QString& where)
    {
        if (wantedRows > 0)
        {
            if (!SelectTheFirstRows(page, wantedRows))
            {
                Out() << "no row at all in " << where << ", so the panel stays closed\n";
            }

            return;
        }

        if (!wantedAddon.isEmpty() && !SelectTheAddonNamed(page, wantedAddon))
        {
            Out() << "no row named " << wantedAddon << " in " << where << ", so the panel stays closed\n";
        }
    };

    shoot(libraryTab, QStringLiteral("01-library"),
          [libraryPage, &selectIfAsked]
          {
              selectIfAsked(*libraryPage, QStringLiteral("the library tree"));
          });
    shoot(communityTab, QStringLiteral("02-community"),
          [&communityViewModel, communityPage, &selectIfAsked]
          {
              communityViewModel.Show();
              selectIfAsked(*communityPage, QStringLiteral("Destinations"));
          });
    shoot(presetsTab, QStringLiteral("03-presets"), {});
    shoot(presetsTab, QStringLiteral("03b-presets-plan"),
          [presetsPage]
          {
              if (auto* plan = presetsPage->findChild<QPushButton*>(QStringLiteral("PresetPlanTab")))
              {
                  plan->click();
              }
          });
    shoot(presetsTab, QStringLiteral("03c-presets-startup-empty"),
          [presetsPage]
          {
              if (auto* startup = presetsPage->findChild<QPushButton*>(QStringLiteral("PresetStartupTab")))
              {
                  startup->click();
              }
          });
    shoot(presetsTab, QStringLiteral("03d-presets-startup-table"),
          [presetsPage]
          {
              if (auto* startup = presetsPage->findChild<QPushButton*>(QStringLiteral("PresetStartupTab")))
              {
                  startup->click();
              }
              if (auto* governs = presetsPage->findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup")))
              {
                  if (!governs->isChecked())
                  {
                      governs->click();
                  }
              }
          });
    shoot(journalTab, QStringLiteral("04-journal"),
          [&journalViewModel, journalPage, &selectIfAsked]
          {
              journalViewModel.Show();
              selectIfAsked(*journalPage, QStringLiteral("the Journal"));
          });
    shoot(quarantineTab, QStringLiteral("05-quarantine"),
          [&quarantineViewModel, quarantinePage, &selectIfAsked]
          {
              quarantineViewModel.Show();
              selectIfAsked(*quarantinePage, QStringLiteral("the Quarantine"));
          });

    if (SelectTheFirstRows(*quarantinePage, 4))
    {
        QPushButton* restore = ButtonNamed(*quarantinePage, QStringLiteral("RestoreChosen"));

        landed = restore != nullptr
            && SaveTheDialogOpenedBy(
                     [restore]
                     {
                         restore->click();
                     },
                     folder, QStringLiteral("16-quarantine-restore"))
            && landed;
    }
    else
    {
        Out() << "nothing in the quarantine, so there is no restore dialog to write\n";
    }

    if (const std::optional<TakenPlace> pretend = APlaceWorthShowingAsTaken(session.Snapshot()); pretend.has_value())
    {
        SwapDialog swapDialog({*pretend}, treeViewModel, &shell);

        treeViewModel.WeighTheSwaps({*pretend},
                                    [&swapDialog](const std::vector<WeighedSwap>& weighed)
                                    {
                                        swapDialog.ShowTheSizes(weighed);
                                    });

        landed = SaveTheDialogOpenedBy(
                     [&swapDialog]
                     {
                         static_cast<void>(swapDialog.exec());
                     },
                     folder, QStringLiteral("17-library-swap"))
            && landed;

        libraryTab->click();
        LetTheLayoutSettle();

        if (const std::vector<StartupLine> carried = AStartupEntryWorthShowing(startupViewModel); !carried.empty())
        {
            StartupEntryDialog carriedDialog(carried, &shell);

            landed = SaveTheDialogOpenedBy(
                         [&carriedDialog]
                         {
                             static_cast<void>(carriedDialog.exec());
                         },
                         folder, QStringLiteral("22-library-startup-entry"))
                && landed;

            libraryTab->click();
            LetTheLayoutSettle();
        }
        else
        {
            Out() << "no enabled startup entry reaches into an addon, so there is no warning to write\n";
        }

        QPushButton* remove = libraryPage->findChild<QPushButton*>(QStringLiteral("PanelDeleteAction"));

        if (SelectTheAddonNamed(*libraryPage, TheFirstAddonOf(session.Snapshot())) && remove != nullptr
            && remove->isEnabled())
        {
            landed = SaveTheDialogOpenedBy(
                         [remove]
                         {
                             remove->click();
                         },
                         folder, QStringLiteral("18-library-delete"))
                && landed;
        }
        else
        {
            Out() << "no addon offers the delete action, so there is no deletion dialog to write\n";
        }
    }
    else
    {
        Out() << "fewer than two addons in the libraries, so there is no swap to picture\n";
    }

    {
        ImportRequest owned;
        owned.source = PathFromUtf8("C:/Users/bruno/AppData/Roaming/Microsoft Flight Simulator/Packages/Community/"
                                    "fsdreamteam-gsx-pro");
        owned.externalSource = PathFromUtf8("C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro");

        ImportDialog importDialog({owned}, session.Snapshot().libraries, session.Profile(), 2147483648ULL, &shell);

        landed = SaveTheDialogOpenedBy(
                     [&importDialog]
                     {
                         static_cast<void>(importDialog.exec());
                     },
                     folder, QStringLiteral("27-community-import"))
            && landed;
    }

    {
        const std::filesystem::path deepRoot =
            PathFromUtf8("C:/Users/bruno/Documents/Flight Simulator Addons/MSFS 2024 Library");
        LibraryRootDialog rootDialog(deepRoot, MeasureTheRoot(deepRoot), &shell);

        landed = SaveTheDialogOpenedBy(
                     [&rootDialog]
                     {
                         static_cast<void>(rootDialog.exec());
                     },
                     folder, QStringLiteral("21-library-deep-root"))
            && landed;
    }

    {
        constexpr qulonglong kFolderSize = 2147483648ULL;

        ImportProgressDialog progress(3, &shell);
        progress.ShowTheStep(OperationKind::ImportCopyToStaging, NameOfImportStep(OperationKind::ImportCopyToStaging));
        progress.ShowTheBytes(1503238553ULL, kFolderSize, 2, OperationKind::ImportCopyToStaging);
        progress.show();
        LetTheLayoutSettle();

        landed = Save(progress, folder, QStringLiteral("32-import-progress-copying")) && landed;

        progress.ShowTheBytes(kFolderSize, kFolderSize, 2, OperationKind::ImportCopyToStaging);
        progress.ShowTheStep(OperationKind::ImportVerifyStaging, NameOfImportStep(OperationKind::ImportVerifyStaging));
        progress.ShowTheBytes(268435456ULL, kFolderSize, 2, OperationKind::ImportVerifyStaging);
        LetTheLayoutSettle();

        landed = Save(progress, folder, QStringLiteral("33-import-progress-checking")) && landed;

        progress.close();
    }

    {
        documentsTab->click();
        documentsViewModel.ReadTheLibrary();
        LetTheLayoutSettle();

        auto* index = documentsPage->findChild<QTreeWidget*>(QStringLiteral("ChartsIndex"));

        if (index != nullptr && index->topLevelItemCount() > 0)
        {
            documentsPage->Show(DocumentPanel::Charts);

            for (int group = 0; group < index->topLevelItemCount(); ++group)
            {
                index->topLevelItem(group)->setExpanded(true);

                for (int within = 0; within < index->topLevelItem(group)->childCount(); ++within)
                {
                    index->topLevelItem(group)->child(within)->setExpanded(true);
                }
            }

            if (QTreeWidgetItem* line = TheFirstLineUnderAGroupOf(*index); line != nullptr)
            {
                ClickTheRow(*index, line);
            }

            KeepTheEventLoopTurning(160);

            landed = Save(*documentsPage, folder, QStringLiteral("26-documents")) && landed;
        }
        else
        {
            Out() << "the library carries no chart, so there is no documentation tab to write\n";
        }
    }

    {
        documentsPage->Show(DocumentPanel::Documents);
        LetTheLayoutSettle();

        auto* index = documentsPage->findChild<QTreeWidget*>(QStringLiteral("DocumentsIndex"));

        QTreeWidgetItem* withAnOutline = nullptr;
        QTreeWidgetItem* withoutOne = nullptr;

        if (index != nullptr)
        {
            for (QTreeWidgetItem* line : EveryLineUnderAGroupOf(*index))
            {
                if (withAnOutline != nullptr && withoutOne != nullptr)
                {
                    break;
                }

                ClickTheRow(*index, line);
                KeepTheEventLoopTurning(4);

                auto* pane = documentsPage->findChild<QTreeWidget*>(QStringLiteral("ReadingOutline"));
                auto* forward = documentsPage->findChild<QPushButton*>(QStringLiteral("NextPage"));

                if (pane == nullptr || forward == nullptr || !forward->isEnabled())
                {
                    continue;
                }

                if (pane->topLevelItemCount() > 1 && withAnOutline == nullptr)
                {
                    withAnOutline = line;
                    continue;
                }

                if (pane->topLevelItemCount() == 0 && withoutOne == nullptr)
                {
                    withoutOne = line;
                }
            }
        }

        if (withAnOutline == nullptr)
        {
            Out() << "no document of this library carries an outline, so 28-documents-outline is not written\n";
        }
        else
        {
            ClickTheRow(*index, withAnOutline);
            KeepTheEventLoopTurning(4);

            auto* pane = documentsPage->findChild<QTreeWidget*>(QStringLiteral("ReadingOutline"));

            ClickTheRow(*pane, pane->topLevelItem(pane->topLevelItemCount() / 2));
            KeepTheEventLoopTurning(6);

            if (auto* mark = documentsPage->findChild<QPushButton*>(QStringLiteral("BookmarkThePage")))
            {
                mark->click();
            }

            KeepTheEventLoopTurning(6);

            landed = Save(*documentsPage, folder, QStringLiteral("28-documents-outline")) && landed;
        }

        if (withoutOne == nullptr)
        {
            Out() << "every document of this library carries an outline, so 29-documents-bookmark is not written\n";
        }
        else
        {
            ClickTheRow(*index, withoutOne);
            KeepTheEventLoopTurning(4);

            auto* forward = documentsPage->findChild<QPushButton*>(QStringLiteral("NextPage"));

            for (int step = 0; step < 12 && forward->isEnabled(); ++step)
            {
                forward->click();
            }

            KeepTheEventLoopTurning(6);

            if (auto* mark = documentsPage->findChild<QPushButton*>(QStringLiteral("BookmarkThePage")))
            {
                mark->click();
            }

            KeepTheEventLoopTurning(6);

            landed = Save(*documentsPage, folder, QStringLiteral("29-documents-bookmark")) && landed;

            if (auto* detach = documentsPage->findChild<QPushButton*>(QStringLiteral("DetachTheReading")))
            {
                detach->click();

                KeepTheEventLoopTurning(160);

                for (QWidget* top : QApplication::topLevelWidgets())
                {
                    auto* alone = qobject_cast<QDialog*>(top);

                    if (alone == nullptr || !alone->isVisible())
                    {
                        continue;
                    }

                    landed = Save(*alone, folder, QStringLiteral("30-documents-detached")) && landed;
                    alone->close();

                    KeepTheEventLoopTurning(80);

                    break;
                }
            }
        }
    }

    auto* sections = diagnosticsPage->findChild<QListWidget*>(QStringLiteral("SectionRail"));
    const QStringList diagnostics{QStringLiteral("06-diagnostics-entries"),
                                  QStringLiteral("07-diagnostics-broken"),
                                  QStringLiteral("08-diagnostics-quarantine"),
                                  QStringLiteral("09-diagnostics-size"),
                                  QStringLiteral("24-diagnostics-scenery"),
                                  QStringLiteral("25-diagnostics-load"),
                                  QString(),
                                  QStringLiteral("31-diagnostics-bisection")};

    diagnosticsTab->click();
    diagnosticsViewModel.Show();

    for (int section = 0; section < diagnostics.size(); ++section)
    {
        if (diagnostics[section].isEmpty())
        {
            continue;
        }

        if (!ClickingReaches(*sections, section))
        {
            Out() << "the section " << diagnostics[section] << " does not select on click, so no user reaches it\n";
            continue;
        }

        LetTheLayoutSettle();
        landed = Save(shell, folder, diagnostics[section]) && landed;
    }

    simulatorTab->click();
    simulatorPage->ShowTheStartupEntries();
    startupViewModel.Show();
    LetTheLayoutSettle();
    landed = Save(shell, folder, QStringLiteral("19-simulator-startup")) && landed;

    startupViewModel.Manage(false);
    LetTheLayoutSettle();
    landed = Save(shell, folder, QStringLiteral("20-simulator-startup-loose")) && landed;
    startupViewModel.Manage(true);

    simulatorPage->ShowThePackageList();
    coverageViewModel.Show();
    LetTheLayoutSettle();
    landed = Save(shell, folder, QStringLiteral("22-simulator-packages")) && landed;

    coverageViewModel.Manage(false);
    LetTheLayoutSettle();
    landed = Save(shell, folder, QStringLiteral("23-simulator-packages-loose")) && landed;
    coverageViewModel.Manage(true);

    libraryTab->click();
    shell.ShowOptions();
    optionsPage->Reload();

    auto* navigation = optionsPage->findChild<QListWidget*>(QStringLiteral("SectionRail"));
    const QStringList panes{QStringLiteral("10-options-profiles"), QStringLiteral("11-options-links"),
                            QStringLiteral("12-options-updates"), QStringLiteral("13-options-language"),
                            QStringLiteral("14-options-about")};

    for (int pane = 0; pane < panes.size(); ++pane)
    {
        if (!ClickingReaches(*navigation, pane))
        {
            Out() << "the tab " << panes[pane] << " does not select on click, so no user reaches it\n";
            continue;
        }

        LetTheLayoutSettle();
        landed = Save(shell, folder, panes[pane]) && landed;
    }

    static_cast<void>(ClickingReaches(*navigation, 0));
    LetTheLayoutSettle();

    if (QPushButton* unregister = optionsPage->findChild<QPushButton*>(QStringLiteral("UnregisterLibrary"));
        unregister != nullptr)
    {
        landed = SaveTheDialogOpenedBy(
                     [unregister]
                     {
                         unregister->click();
                     },
                     folder, QStringLiteral("15-options-unregister"))
            && landed;
    }
    else
    {
        Out() << "no library registered, so there is no unregister dialog to write\n";
    }

    if (QPushButton* categories = optionsPage->findChild<QPushButton*>(QStringLiteral("DeclareCategories"));
        categories != nullptr)
    {
        landed = SaveTheDialogOpenedBy(
                     [categories]
                     {
                         categories->click();
                     },
                     folder, QStringLiteral("15b-options-categories"))
            && landed;
    }

    PageTab* back = nullptr;
    for (PageTab* tab : shell.findChildren<PageTab*>())
    {
        if (tab->Label().startsWith(QChar(0x2190)))
        {
            back = tab;
        }
    }

    if (back == nullptr)
    {
        Out() << "could not find the back tab, so the recorded return would not be the user's\n";
        return 1;
    }

    back->click();
    LetTheLayoutSettle();
    landed = Save(shell, folder, QStringLiteral("12-came-back")) && landed;

    return landed ? 0 : 1;
}
