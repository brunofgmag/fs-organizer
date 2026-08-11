#include <QtTest/QtTest>

#include <QtWidgets/QListWidget>
#include <QtWidgets/QScrollBar>

#include <filesystem>
#include <string>
#include <vector>

#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSimulatorLocator.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/PathPrinting.h"
#include "view/setup/SetupWizard.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class SetupWizardTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void PastTheWizardFloorTheHeightFollowsTheInstallationsFound();
        static void EveryInstallationFoundIsListedWithoutScrolling();
    };

    std::vector<SimulatorCandidate> Candidates(const int count)
    {
        std::vector<SimulatorCandidate> candidates;

        for (int index = 0; index < count; ++index)
        {
            SimulatorCandidate candidate;
            candidate.variant = SimulatorVariant::MSFS2024;
            candidate.packagesPath = std::filesystem::path("D:/Install-" + std::to_string(index)) / "Packages";
            candidate.destinations.push_back(candidate.packagesPath / "Community");

            candidates.push_back(candidate);
        }

        return candidates;
    }

    struct Fixture
    {
        explicit Fixture(const int count) : locator(Candidates(count))
        {
            viewModel.Detect();
        }

        InMemoryFileSystem fileSystem;
        FakeSimulatorLocator locator;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeSettingsRepository settings;
        FakeLibraryIdGenerator identities;
        FakeCatalogScanner catalog;
        SetupService service{locator, filesystemProbe, identities, catalog, settings.stored.profiles, KeepIn(settings)};
        SetupViewModel viewModel{service};
    };

    void Expose(SetupWizard& wizard)
    {
        wizard.show();
        QVERIFY(QTest::qWaitForWindowExposed(&wizard));
    }

    int HeightOf(const int count)
    {
        Fixture fixture(count);

        SetupWizard wizard(fixture.viewModel);
        Expose(wizard);

        return wizard.height();
    }
}

void SetupWizardTest::PastTheWizardFloorTheHeightFollowsTheInstallationsFound()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    QVERIFY(HeightOf(30) > HeightOf(9));
}

void SetupWizardTest::EveryInstallationFoundIsListedWithoutScrolling()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    Fixture fixture(9);

    SetupWizard wizard(fixture.viewModel);
    Expose(wizard);

    const QListWidget* simulators = wizard.findChild<QListWidget*>();

    QVERIFY(simulators != nullptr);
    QCOMPARE(simulators->count(), 9);
    QCOMPARE(simulators->verticalScrollBar()->maximum(), 0);
}

QTEST_MAIN(SetupWizardTest)

#include "tst_setup_wizard.moc"
