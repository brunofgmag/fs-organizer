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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "view/options/OptionsPage.h"
#include "viewmodel/SessionNotifier.h"

class OptionsPageTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheTwoTabsWaitingForTheNextSliceOpenWhatExplainsTheWait();
    static void TheLinksTabOpensOnTheTypeThatIsStored();
    static void ChoosingSymlinkWritesItAndSaysWhatChanges();
    static void EachLibraryGetsARowNamingWhatIsInsideIt();
    static void TheFooterNamesTheFileEveryChangeLands();
    static void OnlyOneProfileCanBeMarkedAtATime();
    static void NothingEnabledMeansTheQuestionCarriesNoCheckboxAtAll();
    static void WhatTheCheckboxOffersSitsInsideTheLayoutOfTheQuestion();
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

    Question WhatUnregisteringAsks(const OptionsPage& page)
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

                               if (auto* box = dialog->findChild<QCheckBox*>(); box != nullptr)
                               {
                                   asked.offeredToDisable = true;
                                   asked.checkboxIsLaidOut =
                                       dialog->layout() != nullptr && dialog->layout()->indexOf(box) >= 0;
                               }

                               dialog->close();
                           });

        if (QPushButton* unregister = ButtonLabelled(page, QStringLiteral("Descadastrar")); unregister != nullptr)
        {
            unregister->click();
        }

        return asked;
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
        OptionsPage page{viewModel, kSettingsFile};
    };
}

void OptionsPageTest::TheTwoTabsWaitingForTheNextSliceOpenWhatExplainsTheWait()
{
    const Fixture f;

    auto* navigation = f.page.findChild<QListWidget*>(QStringLiteral("OptionsNav"));
    auto* panes = f.page.findChild<QStackedWidget*>();
    QVERIFY(navigation != nullptr);
    QVERIFY(panes != nullptr);
    QCOMPARE(navigation->count(), 5);

    navigation->show();

    for (const int waiting : {2, 3})
    {
        const QRect row = navigation->visualItemRect(navigation->item(waiting));
        QTest::mouseClick(navigation->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());

        QCOMPARE(navigation->currentRow(), waiting);
        QCOMPARE(panes->currentIndex(), waiting);

        const auto labels = panes->currentWidget()->findChildren<QLabel*>();
        const bool explained = std::ranges::any_of(labels,
                                                   [](const QLabel* label)
                                                   {
                                                       return label->text().contains(QStringLiteral("slice 12"));
                                                   });

        QVERIFY2(explained, "a aba abriu mas nada nela diz por que ela ainda não faz nada");
        QVERIFY(!navigation->item(waiting)->toolTip().isEmpty());
    }
}

void OptionsPageTest::TheLinksTabOpensOnTheTypeThatIsStored()
{
    Fixture f;
    f.settings.stored.linkType = LinkType::Symbolic;
    f.page.Reload();

    auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    auto* junction = f.page.findChild<QRadioButton*>(QStringLiteral("JunctionChoice"));
    auto* hashed = f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"));

    QVERIFY(symbolic != nullptr);
    QVERIFY(symbolic->isChecked());
    QVERIFY(!junction->isChecked());
    QVERIFY(!hashed->isEnabled());
}

void OptionsPageTest::ChoosingSymlinkWritesItAndSaysWhatChanges()
{
    Fixture f;

    QSignalSpy said(&f.page, &OptionsPage::StatusChanged);

    auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    symbolic->click();

    QCOMPARE(f.settings.stored.linkType, LinkType::Symbolic);
    QCOMPARE(said.count(), 1);
    QVERIFY(said.front().front().toString().contains(QStringLiteral("continuam junction")));
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

    QSignalSpy chosen(&f.page, &OptionsPage::ProfileChosen);
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

QTEST_MAIN(OptionsPageTest)

#include "tst_options_page.moc"
