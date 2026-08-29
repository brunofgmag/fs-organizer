#include <QtTest/QtTest>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QRadioButton>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/model/RecycleLimits.h"
#include "domain/profile/ExternalOrigins.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/library/DeleteDialog.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DeleteDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void CancellingDeletesNothingAndTellsNobodyItDidAnything();
        static void TheRouteThatCannotTakeEverythingSaysWhichAddonsItLeavesBehind();
        static void ARouteNoAddonCanTakeIsOfferedDisabledAndTheOtherOneIsChosen();
        static void TheGestureConcludesAndTheOutcomeOfEachAddonComesBackSeparately();
        static void AnEnabledAddonSaysWhereTheLinkIsAndInWhichProfile();
        static void TheConfirmationCarriesTheNameOfTheRouteItWillRun();
        static void OnlyASelectionThatCameFromAnotherProgramIsOfferedTheWayBack();
        static void TheWayBackNamesTheFolderAndAsksForTheGiveBackInsteadOfADeletion();
    };
}

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;
    constexpr std::uintmax_t kGigabyte = 1024 * kMegabyte;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kAircrafts = "D:/Library/Aircrafts";
    const std::filesystem::path kCrj = "D:/Library/Aircrafts/aerosoft-crj";
    const std::filesystem::path kAtr = "D:/Library/Aircrafts/hype-atr";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = kAircrafts;
        aircrafts.children = {AddonNode(kCrj), AddonNode(kAtr)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kCrj);
            fileSystem.AddDirectory(kAtr);
            fileSystem.AddFile(kCrj / "manifest.json", kMegabyte);
            fileSystem.AddFile(kAtr / "manifest.json", kMegabyte);
            fileSystem.SetRecycleBinQuota("D:", 10 * kGigabyte);

            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();
        }

        [[nodiscard]] const TreeNode* Node(const std::filesystem::path& folder) const
        {
            return AddonAt(session.Snapshot().libraries, folder);
        }

        [[nodiscard]] DeletionPlan PlanFor(const std::vector<const TreeNode*>& nodes)
        {
            const QSignalSpy planned(&viewModel, &DeletionViewModel::Planned);
            viewModel.PlanToDelete(nodes);

            return planned.isEmpty() ? DeletionPlan{} : planned.back().front().value<DeletionPlan>();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier{};
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        DeletionService service{filesystemProbe, files, sidecars, linking, classifier, processProbe, log, sizes};
        DeletionViewModel viewModel{session, profiles, service, sizes, runner};
    };

    void BuryAFileUnder(InMemoryFileSystem& fileSystem, const std::filesystem::path& folder, const std::size_t reach)
    {
        const std::string name(reach - ComparablePath(folder).size() - 1, 'x');

        fileSystem.AddFile(folder / PathFromUtf8(name), kMegabyte);
    }

    QAbstractButton* ButtonLabelled(const DeleteDialog& dialog, const QString& text)
    {
        for (QAbstractButton* button : dialog.findChild<QDialogButtonBox*>()->buttons())
        {
            if (button->text().remove('&') == text)
            {
                return button;
            }
        }

        return nullptr;
    }

    QRadioButton* RouteLabelled(const DeleteDialog& dialog, const QString& text)
    {
        for (QRadioButton* route : dialog.findChildren<QRadioButton*>())
        {
            if (route->text().remove('&') == text)
            {
                return route;
            }
        }

        return nullptr;
    }

    QString EverythingItSays(const DeleteDialog& dialog)
    {
        QStringList said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said.join(QStringLiteral("\n"));
    }
}

void DeleteDialogTest::CancellingDeletesNothingAndTellsNobodyItDidAnything()
{
    Fixture f;
    DeleteDialog dialog(f.PlanFor({f.Node(kCrj)}), f.viewModel);
    const QSignalSpy deleted(&f.viewModel, &DeletionViewModel::Deleted);

    QAbstractButton* cancel = ButtonLabelled(dialog, QStringLiteral("Cancel"));
    QVERIFY(cancel != nullptr);
    cancel->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
    QCOMPARE(deleted.count(), 0);
    QVERIFY(f.fileSystem.Exists(kCrj));
    QCOMPARE(f.journal.appended.size(), std::size_t{0});
}

void DeleteDialogTest::TheRouteThatCannotTakeEverythingSaysWhichAddonsItLeavesBehind()
{
    Fixture f;
    BuryAFileUnder(f.fileSystem, kAtr, kTheRecycleBinStopsAt);

    const DeleteDialog dialog(f.PlanFor({f.Node(kCrj), f.Node(kAtr)}), f.viewModel);
    const QString said = EverythingItSays(dialog);

    const QRadioButton* recycle = RouteLabelled(dialog, QStringLiteral("Move to the Recycle Bin"));
    QVERIFY(recycle != nullptr);
    QVERIFY(recycle->isEnabled());
    QVERIFY(said.contains(QStringLiteral("hype-atr")));
    QVERIFY(said.contains(Explain(FileResult::TheRecycleBinCannotReachIt)));
}

void DeleteDialogTest::ARouteNoAddonCanTakeIsOfferedDisabledAndTheOtherOneIsChosen()
{
    Fixture f;
    BuryAFileUnder(f.fileSystem, kCrj, kTheRecycleBinStopsAt);

    const DeleteDialog dialog(f.PlanFor({f.Node(kCrj)}), f.viewModel);

    const QRadioButton* recycle = RouteLabelled(dialog, QStringLiteral("Move to the Recycle Bin"));
    const QRadioButton* forGood = RouteLabelled(dialog, QStringLiteral("Delete permanently"));
    QVERIFY(recycle != nullptr);
    QVERIFY(forGood != nullptr);
    QVERIFY(!recycle->isEnabled());
    QVERIFY(forGood->isChecked());
    QVERIFY(EverythingItSays(dialog).contains(Explain(FileResult::TheRecycleBinCannotReachIt)));
}

void DeleteDialogTest::TheGestureConcludesAndTheOutcomeOfEachAddonComesBackSeparately()
{
    Fixture f;
    BuryAFileUnder(f.fileSystem, kAtr, kTheRecycleBinStopsAt);

    DeleteDialog dialog(f.PlanFor({f.Node(kCrj), f.Node(kAtr)}), f.viewModel);
    const QSignalSpy deleted(&f.viewModel, &DeletionViewModel::Deleted);

    QAbstractButton* confirm = ButtonLabelled(dialog, QStringLiteral("Move to the Recycle Bin"));
    QVERIFY(confirm != nullptr);
    confirm->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(deleted.count(), 1);

    const auto results = deleted.back().front().value<std::vector<DeletionResult>>();

    QCOMPARE(results.size(), std::size_t{2});
    QCOMPARE(results.front().result, FileResult::Completed);
    QCOMPARE(results.back().result, FileResult::TheRecycleBinCannotReachIt);
    QVERIFY(!f.fileSystem.Exists(kCrj));
    QVERIFY(f.fileSystem.Exists(kAtr));
    QVERIFY(Describe(results.front(), DeletionRoute::RecycleBin)
            != Describe(results.back(), DeletionRoute::RecycleBin));
}

void DeleteDialogTest::AnEnabledAddonSaysWhereTheLinkIsAndInWhichProfile()
{
    Fixture f;
    f.fileSystem.AddLink(kDestination / "aerosoft-crj", kCrj);

    const DeleteDialog dialog(f.PlanFor({f.Node(kCrj)}), f.viewModel);
    const QString said = EverythingItSays(dialog);

    QVERIFY(said.contains(QStringLiteral("Community")));
    QVERIFY(said.contains(f.viewModel.LabelOfProfile("msfs2024")));
}

void DeleteDialogTest::TheConfirmationCarriesTheNameOfTheRouteItWillRun()
{
    Fixture f;
    const DeleteDialog dialog(f.PlanFor({f.Node(kCrj)}), f.viewModel);

    QVERIFY(ButtonLabelled(dialog, QStringLiteral("Move to the Recycle Bin")) != nullptr);

    QRadioButton* forGood = RouteLabelled(dialog, QStringLiteral("Delete permanently"));
    QVERIFY(forGood != nullptr);
    forGood->setChecked(true);

    QVERIFY(ButtonLabelled(dialog, QStringLiteral("Delete permanently")) != nullptr);
}

namespace
{
    const std::filesystem::path kVendorFolder = "C:/Addon Manager/aerosoft-crj";

    void TheCrjCameFromAnotherProgram(Fixture& f)
    {
        f.fileSystem.AddDirectory(kVendorFolder.parent_path());
        f.fileSystem.AddLink(kVendorFolder, kCrj);

        SimulatorProfile stored = Profile();
        RememberWhereItCameFrom(stored, kCrj, kVendorFolder);

        static_cast<void>(f.session.Rewrite(
            [&stored](AppSettings& settings)
            {
                settings.profiles = {stored};

                return true;
            }));

        f.session.ShowActiveProfile();
    }
}

void DeleteDialogTest::OnlyASelectionThatCameFromAnotherProgramIsOfferedTheWayBack()
{
    Fixture ordinary;
    const DeleteDialog withoutIt(ordinary.PlanFor({ordinary.Node(kCrj)}), ordinary.viewModel);

    QCOMPARE(RouteLabelled(withoutIt, QStringLiteral("Give it back to the other program")), nullptr);

    Fixture managed;
    TheCrjCameFromAnotherProgram(managed);

    const DeleteDialog mixed(managed.PlanFor({managed.Node(kCrj), managed.Node(kAtr)}), managed.viewModel);

    QVERIFY2(RouteLabelled(mixed, QStringLiteral("Give it back to the other program")) == nullptr,
             "the route runs over the whole selection, so one addon that never came from outside takes it away");
}

void DeleteDialogTest::TheWayBackNamesTheFolderAndAsksForTheGiveBackInsteadOfADeletion()
{
    Fixture f;
    TheCrjCameFromAnotherProgram(f);

    DeleteDialog dialog(f.PlanFor({f.Node(kCrj)}), f.viewModel);
    const QSignalSpy asked(&dialog, &DeleteDialog::GiveBackRequested);
    const QSignalSpy deleted(&f.viewModel, &DeletionViewModel::Deleted);

    QRadioButton* back = RouteLabelled(dialog, QStringLiteral("Give it back to the other program"));
    QVERIFY(back != nullptr);
    QVERIFY(EverythingItSays(dialog).contains(QStringLiteral("Addon Manager")));

    back->setChecked(true);

    QAbstractButton* confirm = ButtonLabelled(dialog, QStringLiteral("Give it back"));
    QVERIFY2(confirm != nullptr, "giving it back is not deleting, and the button cannot keep saying it is");
    confirm->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(deleted.count(), 0);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.front().front().value<std::vector<std::filesystem::path>>().front(), kCrj);
    QVERIFY(f.fileSystem.Exists(kCrj));
}

QTEST_MAIN(DeleteDialogTest)

#include "tst_delete_dialog.moc"
