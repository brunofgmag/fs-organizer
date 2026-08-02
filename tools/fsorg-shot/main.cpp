#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDir>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>
#include <QtGui/QStyleHints>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "infrastructure/update/GithubUpdateService.h"
#include "support/PathText.h"
#include "view/JournalPage.h"
#include "view/PresetsPage.h"
#include "view/community/CommunityPage.h"
#include "view/library/AddonTreePage.h"
#include "view/options/OptionsPage.h"
#include "view/quarantine/QuarantinePage.h"
#include "view/shell/MainWindow.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/JournalViewModel.h"
#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/SessionNotifier.h"
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

    QPushButton* ButtonLabelled(const QWidget& page, const QString& text)
    {
        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                return button;
            }
        }

        return nullptr;
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
        "Writes a PNG of every FS Organizer screen, building the real widgets against the real installation.\n"
        "Para a escala do Windows, rode com QT_SCALE_FACTOR=1.25.");
    parser.addHelpOption();

    const QCommandLineOption out({"o", "out"}, "Pasta onde gravar os PNG.", "pasta", QDir::currentPath());
    const QCommandLineOption theme({"t", "theme"}, "dark, light ou system.", "paleta", "system");
    const QCommandLineOption size({"s", "size"}, "Tamanho da janela, LARGURAxALTURA.", "tamanho", "1140x760");
    parser.addOption(out);
    parser.addOption(theme);
    parser.addOption(size);
    parser.process(app);

    if (const QString wanted = parser.value(theme); wanted != QLatin1String("system"))
    {
        QGuiApplication::styleHints()->setColorScheme(wanted == QLatin1String("dark") ? Qt::ColorScheme::Dark
                                                                                      : Qt::ColorScheme::Light);
    }

    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
    {
        Out() << "the offscreen plugin applies neither the colour scheme nor the fill of list items, so the PNG "
                 "sairia parecido e errado. Rode sem QT_QPA_PLATFORM.\n";
        return 1;
    }

    ApplyModernistTheme(app);

    const QDir folder(parser.value(out));
    if (!folder.exists())
    {
        Out() << "pasta inexistente: " << folder.path() << "\n";
        return 1;
    }

    const QSize window = SizeFrom(parser.value(size));
    if (window.isEmpty())
    {
        Out() << "invalid size: " << parser.value(size) << "\n";
        return 1;
    }

    JsonSettingsRepository settings(SettingsFilePath());

    const std::optional<AppSettings> stored = settings.Load();
    if (!stored.has_value())
    {
        Out() << "settings.json exists and could not be read: " << AsText(SettingsFilePath()) << "\n";
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
    const UuidLibraryIdGenerator identities;
    const JsonManifestParser manifestParser;
    const FilesystemScanner catalog(manifestParser, filesystemProbe);
    const WindowsProcessProbe processProbe({"FlightSimulator.exe", "FlightSimulator2024.exe"});
    const SystemClock clock;
    JsonlOperationJournal journal(JournalFilePath());

    const LinkingEngine linking(linkService, filesystemProbe);
    const EntryClassifier classifier(linkService, filesystemProbe);
    const OperationLog log(journal, clock);

    ProfileService profileService(catalog, classifier, linking, log, identities, stored->linkType);
    ImportEngine importEngine(filesystemProbe, files, linking, log, stored->linkType);
    ImportService importService(importEngine, processProbe, filesystemProbe, catalog, files, linking, log,
                                stored->linkType);
    LibraryOrganizer organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log,
                               stored->linkType);

    MainWindow shell(*stored);
    RunsRightHere runner;
    SessionNotifier notifier;
    Session session(profileService, organizer, settings, processProbe, runner, notifier);

    AddonTreeModel treeModel;
    AddonTreeViewModel treeViewModel(session, profileService, treeModel, notifier);
    auto* libraryPage = new AddonTreePage(treeViewModel, treeModel, notifier);

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

    FilePresetRepository presetRepository(PresetsFolderPath());
    PresetService presetService(presetRepository, profileService);
    PresetViewModel presetViewModel(session, presetService);
    auto* presetsPage = new PresetsPage(presetViewModel, notifier);

    GithubUpdateService updateService({}, QCoreApplication::applicationVersion(),
                                      QDir::tempPath() + QStringLiteral("/fsorg-shot-updates"));
    UpdateViewModel updateViewModel(updateService, QCoreApplication::applicationVersion(), UpdateMode::Notify, false);

    OptionsViewModel optionsViewModel(session, profileService, settings, notifier);
    auto* optionsPage = new OptionsPage(optionsViewModel, updateViewModel, SettingsFilePath());

    PageTab* libraryTab = shell.AddPage("Library", libraryPage);
    PageTab* communityTab = shell.AddPage("Destinations", communityPage);
    PageTab* presetsTab = shell.AddPage("Presets", presetsPage);
    PageTab* journalTab = shell.AddPage("Journal", journalPage);
    PageTab* quarantineTab = shell.AddPage("Quarantine", quarantinePage);
    shell.CarryOptionsOn(optionsPage);

    shell.CarryTriageOn(libraryPage);
    shell.CarryTriageOn(communityPage);

    QObject::connect(&communityViewModel, &CommunityViewModel::BreakdownChanged, &shell,
                     [&shell](const AttentionBreakdown& breakdown)
                     {
                         shell.ShowTriage(breakdown.broken, breakdown.conflicts, breakdown.duplicated,
                                          breakdown.unmanaged);
                     });

    treeViewModel.ShowActiveProfile();

    shell.resize(window);
    shell.show();
    LetTheLayoutSettle();

    Out() << "janela " << shell.size().width() << "x" << shell.size().height() << " em escala "
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

    shoot(libraryTab, QStringLiteral("01-biblioteca"), {});
    shoot(communityTab, QStringLiteral("02-community"),
          [&communityViewModel]
          {
              communityViewModel.Show();
          });
    shoot(presetsTab, QStringLiteral("03-presets"), {});
    shoot(journalTab, QStringLiteral("04-diario"),
          [&journalViewModel]
          {
              journalViewModel.Show();
          });
    shoot(quarantineTab, QStringLiteral("05-quarentena"),
          [&quarantineViewModel]
          {
              quarantineViewModel.Show();
          });

    libraryTab->click();
    shell.ShowOptions();
    optionsPage->Reload();

    auto* navigation = optionsPage->findChild<QListWidget*>(QStringLiteral("OptionsNav"));
    const QStringList panes{QStringLiteral("06-opcoes-perfis"), QStringLiteral("07-opcoes-links"),
                            QStringLiteral("08-opcoes-atualizacoes"), QStringLiteral("09-opcoes-idioma"),
                            QStringLiteral("10-opcoes-sobre")};

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

    if (QPushButton* unregister = ButtonLabelled(*optionsPage, QObject::tr("Unregister")); unregister != nullptr)
    {
        landed = SaveTheDialogOpenedBy(
                     [unregister]
                     {
                         unregister->click();
                     },
                     folder, QStringLiteral("11-opcoes-descadastrar"))
            && landed;
    }
    else
    {
        Out() << "no library registered, so there is no unregister dialog to write\n";
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
    landed = Save(shell, folder, QStringLiteral("12-voltou")) && landed;

    return landed ? 0 : 1;
}
