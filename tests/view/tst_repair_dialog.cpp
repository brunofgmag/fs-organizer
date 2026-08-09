#include <QtTest/QtTest>

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>

#include <filesystem>
#include <string>
#include <vector>

#include "domain/linking/RepairPlan.h"
#include "tests/support/PathPrinting.h"
#include "view/community/RepairDialog.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class RepairDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryCandidateIsShownWithoutScrolling();
        static void MoreCandidatesMakeTheDialogTaller();
    };

    std::vector<RepairCandidate> Candidates(const int count)
    {
        std::vector<RepairCandidate> candidates;

        for (int index = 0; index < count; ++index)
        {
            const std::string name = "addon-" + std::to_string(index);

            candidates.push_back(RepairCandidate{
                .entry = DestinationEntry{.path = std::filesystem::path("D:/MSFS/Community") / name,
                                          .target = std::filesystem::path("D:/Library/Aircraft") / name}});
        }

        return candidates;
    }

    QScrollArea* TheScrollIn(const RepairDialog& dialog)
    {
        return dialog.findChild<QScrollArea*>();
    }

    void Expose(RepairDialog& dialog)
    {
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    }

    int HeightOf(const int count)
    {
        RepairDialog dialog(Candidates(count));
        Expose(dialog);

        return dialog.height();
    }
}

void RepairDialogTest::EveryCandidateIsShownWithoutScrolling()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    RepairDialog dialog(Candidates(9));
    Expose(dialog);

    QScrollArea* scroll = TheScrollIn(dialog);

    QVERIFY(scroll != nullptr);
    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
}

void RepairDialogTest::MoreCandidatesMakeTheDialogTaller()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    QVERIFY(HeightOf(9) > HeightOf(2));
}

QTEST_MAIN(RepairDialogTest)

#include "tst_repair_dialog.moc"
