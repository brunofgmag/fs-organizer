#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>

#include "view/shell/ImportProgressDialog.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/SizeSummary.h"

namespace
{
    class ImportProgressDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheCheckingBarIsHiddenUntilTheVerificationStepArrives();
        static void EachStepFillsItsOwnBarAndTheCopyOneKeepsWhatItReached();
        static void TheDialogKeepsItsWidthWhenTheStepChanges();
        static void TheBytesLineSpeaksTheSameSizesTheRestOfTheAppDoes();
    };
}

namespace
{
    constexpr qulonglong kTotal = 419430400;
    constexpr qulonglong kChecked = 50331648;

    [[nodiscard]] QProgressBar* BarUnder(const ImportProgressDialog& dialog, const OperationKind step)
    {
        const QString wanted = NameOfImportStep(step);

        const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
        const QList<QProgressBar*> bars = dialog.findChildren<QProgressBar*>();

        for (int at = 0; at < labels.size(); ++at)
        {
            if (labels[at]->text() != wanted)
            {
                continue;
            }

            const int which = step == OperationKind::ImportVerifyStaging ? 1 : 0;

            return bars.value(which);
        }

        return nullptr;
    }
}

void ImportProgressDialogTest::TheCheckingBarIsHiddenUntilTheVerificationStepArrives()
{
    ImportProgressDialog dialog(1, nullptr);
    dialog.show();

    QProgressBar* checking = BarUnder(dialog, OperationKind::ImportVerifyStaging);
    QVERIFY(checking != nullptr);
    QVERIFY2(checking->isHidden(), "an import without the hash never shows a second bar");

    dialog.ShowTheStep(OperationKind::ImportVerifyStaging, NameOfImportStep(OperationKind::ImportVerifyStaging));

    QVERIFY2(!checking->isHidden(), "the second bar arrives with the verification step");
}

void ImportProgressDialogTest::EachStepFillsItsOwnBarAndTheCopyOneKeepsWhatItReached()
{
    ImportProgressDialog dialog(1, nullptr);
    dialog.show();

    dialog.ShowTheStep(OperationKind::ImportCopyToStaging, NameOfImportStep(OperationKind::ImportCopyToStaging));
    dialog.ShowTheBytes(kTotal, kTotal, 1, OperationKind::ImportCopyToStaging);

    dialog.ShowTheStep(OperationKind::ImportVerifyStaging, NameOfImportStep(OperationKind::ImportVerifyStaging));
    dialog.ShowTheBytes(kChecked, kTotal, 1, OperationKind::ImportVerifyStaging);

    QProgressBar* copying = BarUnder(dialog, OperationKind::ImportCopyToStaging);
    QProgressBar* checking = BarUnder(dialog, OperationKind::ImportVerifyStaging);

    QVERIFY(copying != nullptr && checking != nullptr);
    QCOMPARE(copying->value(), 100);
    QCOMPARE(checking->value(), 12);
}

void ImportProgressDialogTest::TheDialogKeepsItsWidthWhenTheStepChanges()
{
    ImportProgressDialog dialog(1, nullptr);
    dialog.show();

    dialog.ShowTheStep(OperationKind::ImportCopyToStaging, NameOfImportStep(OperationKind::ImportCopyToStaging));
    dialog.ShowTheBytes(kTotal, kTotal, 1, OperationKind::ImportCopyToStaging);
    dialog.adjustSize();

    const int copying = dialog.width();

    dialog.ShowTheStep(OperationKind::ImportVerifyStaging, NameOfImportStep(OperationKind::ImportVerifyStaging));
    dialog.ShowTheBytes(kChecked, kTotal, 1, OperationKind::ImportVerifyStaging);
    dialog.adjustSize();

    QCOMPARE(dialog.width(), copying);
}

void ImportProgressDialogTest::TheBytesLineSpeaksTheSameSizesTheRestOfTheAppDoes()
{
    ImportProgressDialog dialog(1, nullptr);
    dialog.show();

    dialog.ShowTheBytes(kChecked, kTotal, 1, OperationKind::ImportVerifyStaging);

    const QString wanted = QStringLiteral("%1 of %2").arg(AsSize(kChecked), AsSize(kTotal));

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();

    QVERIFY2(std::ranges::any_of(labels,
                                 [&wanted](const QLabel* label)
                                 {
                                     return label->text() == wanted;
                                 }),
             "the byte line mirrors AsSize instead of retyping a format");
}

QTEST_MAIN(ImportProgressDialogTest)

#include "tst_import_progress_dialog.moc"
