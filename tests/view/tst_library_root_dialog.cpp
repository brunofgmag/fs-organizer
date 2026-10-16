#include <QtTest/QtTest>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>

#include <filesystem>

#include "domain/model/RecycleLimits.h"
#include "view/library/LibraryRootDialog.h"

namespace
{
    class LibraryRootDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ARootThatLeavesRoomForAnAddonIsNotWorthAWarning();
        static void TheWarningSaysHowManyCharactersTheRootTakesAndWhatItLeaves();
        static void TheShortRootItComparesAgainstIsMeasuredAndNeverRetyped();
        static void PickingAnotherFolderRefusesAndUsingThisOneAccepts();
        static void ARootThatFillsTheWholeLimitLeavesNothingInsteadOfWrappingAround();
        static void WhatTheRootTakesCountsTheSeparatorTheAddonNameSitsAfter();
        static void TheAcceptButtonCarriesTheRoleTheThemePaints();
        static void TheWarningNamesTheCeilingItWasMeasuredAgainstAndNeverAFixedOne();
    };
}

namespace
{
    const std::filesystem::path kShallowRoot{"D:/MSFS 2024"};
    const std::filesystem::path kDeepRoot{"C:/Users/bruno/Documents/Flight Simulator Addons/MSFS 2024 Library"};

    QString TextOfTheDialog(const LibraryRootDialog& dialog)
    {
        QString said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said += label->text() + QStringLiteral("\n");
        }

        return said;
    }

    QAbstractButton* ButtonNamed(const LibraryRootDialog& dialog, const QString& name)
    {
        return dialog.findChild<QAbstractButton*>(name);
    }
}

void LibraryRootDialogTest::ARootThatLeavesRoomForAnAddonIsNotWorthAWarning()
{
    QVERIFY(MeasureTheRoot(kShallowRoot, kTheWindowsRecycleBinStopsAt).ItLeavesRoom());
    QVERIFY(!MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt).ItLeavesRoom());
}

void LibraryRootDialogTest::TheWarningSaysHowManyCharactersTheRootTakesAndWhatItLeaves()
{
    const RootDepth depth = MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt);
    const LibraryRootDialog dialog(kDeepRoot, depth);

    const QString said = TextOfTheDialog(dialog);

    QVERIFY(said.contains(QString::number(depth.characters)));
    QVERIFY(said.contains(QString::number(depth.leavesForTheAddon)));
    QVERIFY(said.contains(QString::number(kTheWindowsRecycleBinStopsAt)));
    QCOMPARE(depth.characters + depth.leavesForTheAddon, kTheWindowsRecycleBinStopsAt);
}

void LibraryRootDialogTest::TheShortRootItComparesAgainstIsMeasuredAndNeverRetyped()
{
    const LibraryRootDialog dialog(kDeepRoot, MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt));

    const QString said = TextOfTheDialog(dialog);
    const RootDepth shallow = MeasureTheRoot(kShallowRoot, kTheWindowsRecycleBinStopsAt);

    QVERIFY2(said.contains(QString::number(shallow.leavesForTheAddon)),
             "the comparison root has to be measured by the same call the production uses");
    QVERIFY(shallow.ItLeavesRoom());
}

void LibraryRootDialogTest::PickingAnotherFolderRefusesAndUsingThisOneAccepts()
{
    LibraryRootDialog refused(kDeepRoot, MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt));
    LibraryRootDialog kept(kDeepRoot, MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt));

    QVERIFY(ButtonNamed(refused, QStringLiteral("PickAnotherFolder")) != nullptr);
    QVERIFY(ButtonNamed(kept, QStringLiteral("UseThisRoot")) != nullptr);

    ButtonNamed(refused, QStringLiteral("PickAnotherFolder"))->click();
    ButtonNamed(kept, QStringLiteral("UseThisRoot"))->click();

    QCOMPARE(refused.result(), static_cast<int>(QDialog::Rejected));
    QCOMPARE(kept.result(), static_cast<int>(QDialog::Accepted));
}

void LibraryRootDialogTest::ARootThatFillsTheWholeLimitLeavesNothingInsteadOfWrappingAround()
{
    const std::filesystem::path absurd{std::string(kTheWindowsRecycleBinStopsAt + 40, 'x')};

    const RootDepth depth = MeasureTheRoot(absurd, kTheWindowsRecycleBinStopsAt);

    QCOMPARE(depth.leavesForTheAddon, std::size_t{0});
    QVERIFY(!depth.ItLeavesRoom());
}

void LibraryRootDialogTest::TheAcceptButtonCarriesTheRoleTheThemePaints()
{
    const LibraryRootDialog dialog(kDeepRoot, MeasureTheRoot(kDeepRoot, kTheWindowsRecycleBinStopsAt));

    const QAbstractButton* keep = ButtonNamed(dialog, QStringLiteral("UseThisRoot"));

    QCOMPARE(keep->property("role").toString(), QStringLiteral("primary"));
    QCOMPARE(ButtonNamed(dialog, QStringLiteral("PickAnotherFolder"))->property("role").toString(), QString());
}

void LibraryRootDialogTest::WhatTheRootTakesCountsTheSeparatorTheAddonNameSitsAfter()
{
    const std::filesystem::path child = kShallowRoot / "x";

    QCOMPARE(child.native().size(), MeasureTheRoot(kShallowRoot, kTheWindowsRecycleBinStopsAt).characters + 1);
}

void LibraryRootDialogTest::TheWarningNamesTheCeilingItWasMeasuredAgainstAndNeverAFixedOne()
{
    constexpr std::size_t kAnotherCeiling = kTheWindowsRecycleBinStopsAt + 55;
    const RootDepth depth = MeasureTheRoot(kDeepRoot, kAnotherCeiling);
    const LibraryRootDialog dialog(kDeepRoot, depth);

    const QString said = TextOfTheDialog(dialog);

    QVERIFY(said.contains(QString::number(kAnotherCeiling)));
    QVERIFY(!said.contains(QString::number(kTheWindowsRecycleBinStopsAt)));
    QVERIFY(said.contains(QStringLiteral("leaves %1 for the addon")
                              .arg(MeasureTheRoot(kShallowRoot, kAnotherCeiling).leavesForTheAddon)));
}

QTEST_MAIN(LibraryRootDialogTest)

#include "tst_library_root_dialog.moc"
