#include <algorithm>

#include <QtCore/QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QStackedWidget>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeUpdateService.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "view/options/OptionsPage.h"
#include "viewmodel/SessionNotifier.h"

class OptionsPageTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheTabThatIsStillWaitingOpensWhatExplainsTheWait();
    static void TheUpdatesTabOffersTheThreeModesAndSaysWhereItStands();
    static void TheLinksTabOpensOnTheTypeThatIsStored();
    static void ChoosingSymlinkWritesItAndSaysWhatChanges();
    static void EachLibraryGetsARowNamingWhatIsInsideIt();
    static void TheFooterNamesTheFileEveryChangeLands();
    static void OnlyOneProfileCanBeMarkedAtATime();
    static void NothingEnabledMeansTheQuestionCarriesNoCheckboxAtAll();
    static void WhatTheCheckboxOffersSitsInsideTheLayoutOfTheQuestion();
    static void EditingAnotherProfileShowsItsLibrariesInsteadOfTheOnesInUse();
    static void TheProfileThatIsNotInUseOffersNoButtonThatWouldChangeIt();
    static void TheOnlyProfileCannotBeRemoved();
    static void RemovingTheProfileInUseStillCountsItsAddonsWhileAnotherIsShown();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kSettingsFile = "C:/Users/bruno/AppData/Local/fs-organizer/settings.json";

    TreeNode LibraryTree()
    {
        TreeNode addon;
        addon.kind = TreeNodeKind::Addon;
        addon.path = kAddon;
        addon.addon = Addon{kAddon, Manifest{}};

        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {std::move(addon)};

        TreeNode root;
        root.kind = TreeNodeKind::Library;
        root.path = kLibrary;
        root.children = {std::move(aircrafts)};

        return root;
    }

    struct Question
    {
        bool opened = false;
        bool offeredToDisable = false;
        bool checkboxIsLaidOut = false;
    };

    QPushButton* ButtonLabelled(const QWidget& page, const QString& text)
    {
        const auto buttons = page.findChildren<QPushButton*>();
        const auto found = std::ranges::find_if(buttons,
                                                [&text](const QPushButton* button)
                                                {
                                                    return button->text() == text;
                                                });

        return found == buttons.end() ? nullptr : *found;
    }

    QPushButton* LastButtonLabelled(const QWidget& page, const QString& text)
    {
        QPushButton* last = nullptr;

        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                last = button;
            }
        }

        return last;
    }

    Question WhatClickingAsks(QPushButton* button)
    {
        Question asked;

        QTimer::singleShot(0,
                           [&asked]
                           {
                               QWidget* dialog = QApplication::activeModalWidget();
                               if (dialog == nullptr)
                               {
                                   return;
                               }

                               asked.opened = true;

                               if (const auto* box = dialog->findChild<QCheckBox*>(); box != nullptr)
                               {
                                   asked.offeredToDisable = true;
                                   asked.checkboxIsLaidOut =
                                       dialog->layout() != nullptr && dialog->layout()->indexOf(box) >= 0;
                               }

                               dialog->close();
                           });

        if (button != nullptr)
        {
            button->click();
        }

        return asked;
    }

    Question WhatUnregisteringAsks(const OptionsPage& page)
    {
        return WhatClickingAsks(ButtonLabelled(page, QStringLiteral("Descadastrar")));
    }

    SimulatorProfile SecondProfile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2020";
        profile.variant = SimulatorVariant::MSFS2020;
        profile.destinations = {"E:/Flight Simulator 2020/Community"};
        profile.defaultDestination = "E:/Flight Simulator 2020/Community";
        profile.libraries = {Library{"library-2", "D:/MSFS 2020", "MSFS 2020"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            SimulatorProfile profile;
            profile.id = "msfs2024";
            profile.variant = SimulatorVariant::MSFS2024;
            profile.destinations = {kCommunity};
            profile.defaultDestination = kCommunity;
            profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

            settings.stored.profiles = {profile};
            settings.stored.activeProfileId = "msfs2024";

            session.ShowActiveProfile();
            page.Reload();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        OptionsViewModel viewModel{session, service, settings, notifier};
        FakeUpdateService updateService;
        UpdateViewModel updates{updateService, QStringLiteral("0.1.0"), UpdateMode::Notify, true};
        OptionsPage page{viewModel, updates, kSettingsFile};
    };
}

void OptionsPageTest::TheTabThatIsStillWaitingOpensWhatExplainsTheWait()
{
    const Fixture f;

    auto* navigation = f.page.findChild<QListWidget*>(QStringLiteral("OptionsNav"));
    const auto* panes = f.page.findChild<QStackedWidget*>();
    QVERIFY(navigation != nullptr);
    QVERIFY(panes != nullptr);
    QCOMPARE(navigation->count(), 5);

    navigation->show();

    constexpr int waiting = 3;
    const QRect row = navigation->visualItemRect(navigation->item(waiting));
    QTest::mouseClick(navigation->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());

    QCOMPARE(navigation->currentRow(), waiting);
    QCOMPARE(panes->currentIndex(), waiting);

    const auto labels = panes->currentWidget()->findChildren<QLabel*>();
    const bool explained = std::ranges::any_of(labels,
                                               [](const QLabel* label)
                                               {
                                                   return label->text().contains(QStringLiteral("idioma"));
                                               });

    QVERIFY2(explained, "a aba abriu mas nada nela diz por que ela ainda não faz nada");
    QVERIFY(!navigation->item(waiting)->toolTip().isEmpty());
}

void OptionsPageTest::TheUpdatesTabOffersTheThreeModesAndSaysWhereItStands()
{
    Fixture f;

    auto* notify = f.page.findChild<QRadioButton*>(QStringLiteral("NotifyUpdateChoice"));
    auto* automatic = f.page.findChild<QRadioButton*>(QStringLiteral("AutomaticUpdateChoice"));
    auto* manual = f.page.findChild<QRadioButton*>(QStringLiteral("ManualUpdateChoice"));
    auto* status = f.page.findChild<QLabel*>(QStringLiteral("UpdateStatus"));

    QVERIFY(notify != nullptr);
    QVERIFY(automatic != nullptr);
    QVERIFY(manual != nullptr);
    QVERIFY(notify->isChecked());
    QVERIFY(!status->text().isEmpty());

    automatic->click();

    QCOMPARE(f.updates.Mode(), UpdateMode::Automatic);
    QCOMPARE(f.settings.stored.updateMode, UpdateMode::Automatic);
}

void OptionsPageTest::TheLinksTabOpensOnTheTypeThatIsStored()
{
    Fixture f;
    f.settings.stored.linkType = LinkType::Symbolic;
    f.page.Reload();

    const auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    const auto* junction = f.page.findChild<QRadioButton*>(QStringLiteral("JunctionChoice"));
    const auto* hashed = f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"));

    QVERIFY(symbolic != nullptr);
    QVERIFY(symbolic->isChecked());
    QVERIFY(!junction->isChecked());
    QVERIFY(!hashed->isEnabled());
}

void OptionsPageTest::ChoosingSymlinkWritesItAndSaysWhatChanges()
{
    const Fixture f;

    QSignalSpy said(&f.page, &OptionsPage::StatusChanged);

    auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    symbolic->click();

    QCOMPARE(f.settings.stored.linkType, LinkType::Symbolic);
    QCOMPARE(said.count(), 1);
    QVERIFY(said.front().front().toString().contains(QStringLiteral("continuam junção de diretório")));
}

void OptionsPageTest::EachLibraryGetsARowNamingWhatIsInsideIt()
{
    const Fixture f;
    const auto labels = f.page.findChildren<QLabel*>();
    const bool named = std::ranges::any_of(labels,
                                           [](const QLabel* label)
                                           {
                                               return label->text().contains(QStringLiteral("1 categoria(s)"))
                                                   && label->text().contains(QStringLiteral("1 addons"));
                                           });

    QVERIFY2(named, "nenhuma linha de biblioteca disse o que tem dentro dela");
}

void OptionsPageTest::TheFooterNamesTheFileEveryChangeLands()
{
    Fixture f;

    QSignalSpy summarised(&f.page, &OptionsPage::SummaryChanged);
    f.page.Reload();

    QCOMPARE(summarised.count(), 1);
    QVERIFY(summarised.front().front().toString().contains(QStringLiteral("settings.json")));
}

void OptionsPageTest::OnlyOneProfileCanBeMarkedAtATime()
{
    Fixture f;

    SimulatorProfile legacy;
    legacy.id = "msfs2020";
    legacy.variant = SimulatorVariant::MSFS2020;
    legacy.destinations = {"C:/Packages/Community"};
    f.settings.stored.profiles.push_back(legacy);
    f.page.Reload();

    const auto marks = f.page.findChildren<QRadioButton*>();
    QList<QRadioButton*> profiles;
    for (QRadioButton* mark : marks)
    {
        if (mark->text().startsWith(QStringLiteral("Flight Simulator")))
        {
            profiles.append(mark);
        }
    }

    QCOMPARE(profiles.size(), 2);
    QCOMPARE(std::ranges::count_if(profiles,
                                   [](const QRadioButton* mark)
                                   {
                                       return mark->isChecked();
                                   }),
             1);

    const QSignalSpy chosen(&f.page, &OptionsPage::ProfileChosen);
    profiles[1]->click();

    QCOMPARE(chosen.count(), 1);
    QCOMPARE(std::ranges::count_if(profiles,
                                   [](const QRadioButton* mark)
                                   {
                                       return mark->isChecked();
                                   }),
             1);
    QVERIFY(profiles[1]->isChecked());
    QVERIFY(!profiles[0]->isChecked());
}

void OptionsPageTest::NothingEnabledMeansTheQuestionCarriesNoCheckboxAtAll()
{
    const Fixture f;
    const Question asked = WhatUnregisteringAsks(f.page);

    QVERIFY2(asked.opened, "o diálogo de descadastrar não abriu, então nada foi olhado");
    QVERIFY2(!asked.offeredToDisable, "a caixa de desabilitar apareceu sem nada habilitado para desabilitar");
}

void OptionsPageTest::WhatTheCheckboxOffersSitsInsideTheLayoutOfTheQuestion()
{
    Fixture f;
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.ShowActiveProfile();
    f.page.Reload();

    const Question asked = WhatUnregisteringAsks(f.page);

    QVERIFY2(asked.opened, "o diálogo de descadastrar não abriu, então nada foi olhado");
    QVERIFY2(asked.offeredToDisable, "com um addon habilitado, a caixa de desabilitar não foi oferecida");
    QVERIFY2(asked.checkboxIsLaidOut,
             "a caixa foi criada mas ficou fora do layout, que é como ela se sobrepõe ao texto");
}

void OptionsPageTest::EditingAnotherProfileShowsItsLibrariesInsteadOfTheOnesInUse()
{
    Fixture f;
    f.settings.stored.profiles.push_back(SecondProfile());
    f.page.Reload();

    QVERIFY(LastButtonLabelled(f.page, QStringLiteral("Ver…")) != nullptr);
    LastButtonLabelled(f.page, QStringLiteral("Ver…"))->click();

    const auto labels = f.page.findChildren<QLabel*>();
    const bool showsTheOtherLibrary =
        std::ranges::any_of(labels,
                            [](const QLabel* label)
                            {
                                return label->text().contains(QStringLiteral("MSFS 2020"));
                            });

    QVERIFY2(showsTheOtherLibrary, "clicar em Ver deixou os grupos de baixo no perfil ativo");
}

void OptionsPageTest::TheProfileThatIsNotInUseOffersNoButtonThatWouldChangeIt()
{
    Fixture f;
    f.settings.stored.profiles.push_back(SecondProfile());
    f.page.Reload();

    LastButtonLabelled(f.page, QStringLiteral("Ver…"))->click();

    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Adicionar biblioteca…"))->isEnabled());
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Importar do MSFS Addons Linker…"))->isEnabled());
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Descadastrar"))->isEnabled());
}

void OptionsPageTest::TheOnlyProfileCannotBeRemoved()
{
    const Fixture f;

    QVERIFY(ButtonLabelled(f.page, QStringLiteral("Remover")) != nullptr);
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Remover"))->isEnabled());
}

void OptionsPageTest::RemovingTheProfileInUseStillCountsItsAddonsWhileAnotherIsShown()
{
    Fixture f;
    f.settings.stored.profiles.push_back(SecondProfile());
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.ShowActiveProfile();
    f.page.Reload();

    LastButtonLabelled(f.page, QStringLiteral("Ver…"))->click();

    const Question asked = WhatClickingAsks(ButtonLabelled(f.page, QStringLiteral("Remover")));

    QVERIFY2(asked.opened, "a pergunta de remover não abriu, então nada foi olhado");
    QVERIFY2(asked.offeredToDisable, "remover o perfil em uso com outro perfil na tela contou zero addons habilitados");
}

QTEST_MAIN(OptionsPageTest)

#include "tst_options_page.moc"
