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
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeUpdateService.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "view/options/OptionsPage.h"
#include "view/shell/LanguageSwitch.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class OptionsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheLanguageTabOffersBothAndOpensOnTheStoredOne();
        static void TheUpdatesTabOffersTheThreeModesAndSaysWhereItStands();
        static void TheLinksTabOpensOnTheTypeThatIsStored();
        static void ChoosingSymlinkWritesItAndSaysWhatChanges();
        static void BothChecksAreOfferedAndTheTabOpensOnTheOneThatIsStored();
        static void ChoosingTheHashWritesItAndSaysWhatEveryImportWillDo();
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
}

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
        addon.addon = Addon{.folderPath = kAddon, .manifest = Manifest{}};

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
        return WhatClickingAsks(ButtonLabelled(page, QStringLiteral("Unregister")));
    }

    SimulatorProfile SecondProfile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2020";
        profile.variant = SimulatorVariant::MSFS2020;
        profile.destinations = {"E:/Flight Simulator 2020/Community"};
        profile.defaultDestination = "E:/Flight Simulator 2020/Community";
        profile.libraries = {Library{.id = "library-2", .path = "D:/MSFS 2020", .label = "MSFS 2020"}};

        return profile;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

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

            session.ShowActiveProfile();
            page.Reload();
        }

        void Seed(const std::function<void(AppSettings&)>& change)
        {
            static_cast<void>(session.Rewrite(
                [&change](AppSettings& settings)
                {
                    change(settings);

                    return true;
                }));
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        OptionsViewModel viewModel{session, service, notifier};
        FakeUpdateService updateService;
        UpdateViewModel updates{updateService, UpdateMode::Notify, true};
        OptionsPage page{viewModel, updates, kSettingsFile};
    };
}

void OptionsPageTest::TheLanguageTabOffersBothAndOpensOnTheStoredOne()
{
    Fixture f;

    auto* navigation = f.page.findChild<QListWidget*>(QStringLiteral("SectionRail"));
    const auto* panes = f.page.findChild<QStackedWidget*>();
    QVERIFY(navigation != nullptr);
    QVERIFY(panes != nullptr);
    QCOMPARE(navigation->count(), 5);

    navigation->show();

    constexpr int language = 3;
    const QRect row = navigation->visualItemRect(navigation->item(language));
    QTest::mouseClick(navigation->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());

    QCOMPARE(navigation->currentRow(), language);
    QCOMPARE(panes->currentIndex(), language);

    auto* english = f.page.findChild<QRadioButton*>(QStringLiteral("EnglishChoice"));
    auto* brazilian = f.page.findChild<QRadioButton*>(QStringLiteral("BrazilianChoice"));
    QVERIFY2(english != nullptr && brazilian != nullptr, "the Language tab does not offer both languages");

    const QString resolved = LanguageSwitch::Resolve({});
    QCOMPARE(brazilian->isChecked(), resolved == QStringLiteral("pt_BR"));
    QCOMPARE(english->isChecked(), resolved == QStringLiteral("en"));

    brazilian->click();

    QCOMPARE(f.settings.stored.language, std::string{"pt_BR"});

    f.page.Reload();

    QVERIFY2(brazilian->isChecked(), "with pt_BR written, the tab must open on Brazilian Portuguese");
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
    f.Seed(
        [](AppSettings& settings)
        {
            settings.linkType = LinkType::Symbolic;
        });
    f.page.Reload();

    const auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    const auto* junction = f.page.findChild<QRadioButton*>(QStringLiteral("JunctionChoice"));
    const auto* hashed = f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"));

    QVERIFY(symbolic != nullptr);
    QVERIFY(symbolic->isChecked());
    QVERIFY(!junction->isChecked());
    QVERIFY(hashed != nullptr);
}

void OptionsPageTest::ChoosingSymlinkWritesItAndSaysWhatChanges()
{
    const Fixture f;

    QSignalSpy said(&f.page, &OptionsPage::StatusChanged);

    auto* symbolic = f.page.findChild<QRadioButton*>(QStringLiteral("SymbolicChoice"));
    symbolic->click();

    QCOMPARE(f.settings.stored.linkType, LinkType::Symbolic);
    QCOMPARE(said.count(), 1);
    QVERIFY(said.front().front().toString().contains(QStringLiteral("stay directory junctions")));
}

void OptionsPageTest::BothChecksAreOfferedAndTheTabOpensOnTheOneThatIsStored()
{
    Fixture f;

    const auto* structure = f.page.findChild<QRadioButton*>(QStringLiteral("StructureChoice"));
    const auto* hashed = f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"));

    QVERIFY(structure != nullptr);
    QVERIFY(hashed != nullptr);
    QVERIFY2(structure->isEnabled() && hashed->isEnabled(), "both were dead until this slice, and neither is now");
    QVERIFY(structure->isChecked());
    QVERIFY(!hashed->isChecked());

    f.Seed(
        [](AppSettings& settings)
        {
            settings.verification = Verification::ByHash;
        });
    f.page.Reload();

    QVERIFY(f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"))->isChecked());
    QVERIFY(!f.page.findChild<QRadioButton*>(QStringLiteral("StructureChoice"))->isChecked());
}

void OptionsPageTest::ChoosingTheHashWritesItAndSaysWhatEveryImportWillDo()
{
    const Fixture f;

    QSignalSpy said(&f.page, &OptionsPage::StatusChanged);

    f.page.findChild<QRadioButton*>(QStringLiteral("HashChoice"))->click();

    QCOMPARE(f.settings.stored.verification, Verification::ByHash);
    QCOMPARE(said.count(), 1);
    QVERIFY(said.front().front().toString().contains(QStringLiteral("read both sides in full")));

    f.page.findChild<QRadioButton*>(QStringLiteral("StructureChoice"))->click();

    QCOMPARE(f.settings.stored.verification, Verification::ByStructure);
    QCOMPARE(said.count(), 2);
}

void OptionsPageTest::EachLibraryGetsARowNamingWhatIsInsideIt()
{
    const Fixture f;
    const auto labels = f.page.findChildren<QLabel*>();
    const bool named = std::ranges::any_of(labels,
                                           [](const QLabel* label)
                                           {
                                               return label->text().contains(QStringLiteral("1 category"))
                                                   && label->text().contains(QStringLiteral("1 addon"));
                                           });

    QVERIFY2(named, "no library row said what is inside it");

    const bool pluralOfOne = std::ranges::any_of(labels,
                                                 [](const QLabel* label)
                                                 {
                                                     return label->text().contains(QStringLiteral("1 categories"))
                                                         || label->text().contains(QStringLiteral("1 addons"));
                                                 });

    QVERIFY2(!pluralOfOne, "a row counted one thing in the plural");
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
    f.Seed(
        [&legacy](AppSettings& settings)
        {
            settings.profiles.push_back(legacy);
        });
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

    QVERIFY2(asked.opened, "the unregister dialog did not open, so nothing was looked at");
    QVERIFY2(!asked.offeredToDisable, "the disable box appeared with nothing enabled to disable");
}

void OptionsPageTest::WhatTheCheckboxOffersSitsInsideTheLayoutOfTheQuestion()
{
    Fixture f;
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.ShowActiveProfile();
    f.page.Reload();

    const Question asked = WhatUnregisteringAsks(f.page);

    QVERIFY2(asked.opened, "the unregister dialog did not open, so nothing was looked at");
    QVERIFY2(asked.offeredToDisable, "with an addon enabled, the disable checkbox was not offered");
    QVERIFY2(asked.checkboxIsLaidOut,
             "the checkbox was created but left out of the layout, which is how it ends up over the text");
}

void OptionsPageTest::EditingAnotherProfileShowsItsLibrariesInsteadOfTheOnesInUse()
{
    Fixture f;
    f.Seed(
        [](AppSettings& settings)
        {
            settings.profiles.push_back(SecondProfile());
        });
    f.page.Reload();

    QVERIFY(LastButtonLabelled(f.page, QStringLiteral("View…")) != nullptr);
    LastButtonLabelled(f.page, QStringLiteral("View…"))->click();

    const auto labels = f.page.findChildren<QLabel*>();
    const bool showsTheOtherLibrary =
        std::ranges::any_of(labels,
                            [](const QLabel* label)
                            {
                                return label->text().contains(QStringLiteral("MSFS 2020"));
                            });

    QVERIFY2(showsTheOtherLibrary, "clicking View left the groups below on the active profile");
}

void OptionsPageTest::TheProfileThatIsNotInUseOffersNoButtonThatWouldChangeIt()
{
    Fixture f;
    f.Seed(
        [](AppSettings& settings)
        {
            settings.profiles.push_back(SecondProfile());
        });
    f.page.Reload();

    LastButtonLabelled(f.page, QStringLiteral("View…"))->click();

    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Add library…"))->isEnabled());
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Import from MSFS Addons Linker…"))->isEnabled());
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Unregister"))->isEnabled());
}

void OptionsPageTest::TheOnlyProfileCannotBeRemoved()
{
    const Fixture f;

    QVERIFY(ButtonLabelled(f.page, QStringLiteral("Remove")) != nullptr);
    QVERIFY(!ButtonLabelled(f.page, QStringLiteral("Remove"))->isEnabled());
}

void OptionsPageTest::RemovingTheProfileInUseStillCountsItsAddonsWhileAnotherIsShown()
{
    Fixture f;
    f.Seed(
        [](AppSettings& settings)
        {
            settings.profiles.push_back(SecondProfile());
        });
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "pmdg-aircraft-77w", kAddon);
    f.session.ShowActiveProfile();
    f.page.Reload();

    LastButtonLabelled(f.page, QStringLiteral("View…"))->click();

    const Question asked = WhatClickingAsks(ButtonLabelled(f.page, QStringLiteral("Remove")));

    QVERIFY2(asked.opened, "the remove question did not open, so nothing was looked at");
    QVERIFY2(asked.offeredToDisable,
             "removing the profile in use with another profile on screen counted zero enabled addons");
}

QTEST_MAIN(OptionsPageTest)

#include "tst_options_page.moc"
