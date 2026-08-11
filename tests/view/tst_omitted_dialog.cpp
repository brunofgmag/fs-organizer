#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>

#include "view/presets/OmittedDialog.h"

namespace
{
    class OmittedDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryOmittedAddonIsNamedWithTheCategoryItSitsIn();
        static void TheDialogSaysTheOmittedAreNotAPileOnTopOfWhatIsTurnedOff();
        static void TheDialogIsTallerWithMoreAddonsInsteadOfPinnedToItsMinimum();
    };

    QList<OmittedAddon> Some(const int howMany)
    {
        QList<OmittedAddon> omitted;

        for (int index = 0; index < howMany; ++index)
        {
            omitted.append(
                OmittedAddon{.name = QStringLiteral("addon-%1").arg(index), .category = QStringLiteral("Sceneries")});
        }

        return omitted;
    }
}

void OmittedDialogTest::EveryOmittedAddonIsNamedWithTheCategoryItSitsIn()
{
    const OmittedDialog dialog(
        {OmittedAddon{.name = QStringLiteral("shockwave-lights"), .category = QStringLiteral("Misc")},
         OmittedAddon{.name = QStringLiteral("orbx-ybbn"), .category = QStringLiteral("Sceneries")}});

    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("OmittedAddons"));

    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("shockwave-lights"));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Misc"));
    QCOMPARE(table->item(1, 0)->text(), QStringLiteral("orbx-ybbn"));
    QCOMPARE(table->item(1, 1)->text(), QStringLiteral("Sceneries"));
}

void OmittedDialogTest::TheDialogSaysTheOmittedAreNotAPileOnTopOfWhatIsTurnedOff()
{
    const OmittedDialog dialog(Some(3));

    QStringList said;
    for (const QLabel* label : dialog.findChildren<QLabel*>())
    {
        said.append(label->text());
    }

    QVERIFY(said.join(QStringLiteral(" ")).contains(QStringLiteral("not a pile on top of it")));
}

void OmittedDialogTest::TheDialogIsTallerWithMoreAddonsInsteadOfPinnedToItsMinimum()
{
    const OmittedDialog few(Some(2));
    const OmittedDialog many(Some(14));

    QVERIFY2(many.height() > few.height(),
             qPrintable(QStringLiteral("2 addons gave %1 px and 14 gave %2").arg(few.height()).arg(many.height())));
}

QTEST_MAIN(OmittedDialogTest)

#include "tst_omitted_dialog.moc"
