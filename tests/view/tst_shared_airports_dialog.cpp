#include <QtTest/QtTest>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <vector>

#include "tests/support/ButtonLookup.h"
#include "view/library/SharedAirportsDialog.h"

namespace
{
    class SharedAirportsDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BothAddonsAreNamedAndTheAirportRidesAlongTheLine();
        static void AThousandCodesOfOnePairStayOneLine();
        static void NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites();
        static void NothingIsRememberedUntilAPairIsChecked();
        static void OnlyTheCheckedPairsComeBack();
        static void NeitherSideOfTheOverlapIsCalledAProblem();
        static void TheWarningSaysTheCodeWasReadFromTheScenery();
    };

    [[nodiscard]] SharedAirportsLine Sharing(const QString& turningOn, const QString& alreadyOn, QStringList codes)
    {
        return {.turningOn = turningOn,
                .alreadyOn = alreadyOn,
                .codes = std::move(codes),
                .one = {.libraryId = "library-1", .folderName = turningOn.toStdString()},
                .other = {.libraryId = "library-1", .folderName = alreadyOn.toStdString()}};
    }

    [[nodiscard]] std::vector<QCheckBox*> PairBoxesOf(const QDialog& dialog)
    {
        std::vector<QCheckBox*> boxes;

        for (QCheckBox* box : dialog.findChildren<QCheckBox*>())
        {
            if (box->text().isEmpty())
            {
                boxes.push_back(box);
            }
        }

        return boxes;
    }

    [[nodiscard]] QStringList TextsOf(const QDialog& dialog)
    {
        QStringList said;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said;
    }
}

void SharedAirportsDialogTest::BothAddonsAreNamedAndTheAirportRidesAlongTheLine()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("flytampa-airport-eham-amsterdam"), QStringLiteral("asobo-airport-eham-amsterdam"),
                 {QStringLiteral("EHAM")})});

    const QStringList said = TextsOf(dialog);

    QVERIFY(said.contains(QStringLiteral("flytampa-airport-eham-amsterdam")));
    QVERIFY2(said.contains(QStringLiteral("asobo-airport-eham-amsterdam")),
             "the one that was already on is the half the user cannot see from the gesture they just made");
    QVERIFY(said.contains(QStringLiteral("EHAM")));
}

void SharedAirportsDialogTest::AThousandCodesOfOnePairStayOneLine()
{
    QStringList many;
    for (int at = 0; at < 1403; ++at)
    {
        many << QStringLiteral("EH%1").arg(at);
    }

    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("navigraph-nav-jepp"), QStringLiteral("navigraph-nav-base"), many)});

    const QStringList said = TextsOf(dialog);

    QVERIFY2(said.filter(QStringLiteral("navigraph-nav-base")).size() == 1,
             "the two data packages of the reference installation share 1403 codes, and the unit of the list is the "
             "pair of addons, never the code");
    QVERIFY2(said.join(QLatin1Char(' ')).contains(QStringLiteral("1399 more")),
             "the codes are the detail of the pair, spelled out while they are few and counted once they are not");
}

void SharedAirportsDialogTest::NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites()
{
    SharedAirportsDialog agreeing(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});
    QSignalSpy accepted(&agreeing, &QDialog::accepted);
    PairBoxesOf(agreeing).front()->setChecked(true);
    ButtonSaying(agreeing, QStringLiteral("Remember the checked ones"))->click();
    QCOMPARE(accepted.count(), 1);

    SharedAirportsDialog refusing(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Leave them both on"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
    QVERIFY2(refusing.Chosen().empty(), "refusing writes nothing, which is what it promises");
}

void SharedAirportsDialogTest::NothingIsRememberedUntilAPairIsChecked()
{
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});

    QVERIFY2(!ButtonSaying(dialog, QStringLiteral("Remember the checked ones"))->isEnabled(),
             "the pairs come unchecked, so the answer that writes has nothing to write yet");
    QVERIFY(dialog.Chosen().empty());

    PairBoxesOf(dialog).front()->setChecked(true);

    QVERIFY(ButtonSaying(dialog, QStringLiteral("Remember the checked ones"))->isEnabled());
}

void SharedAirportsDialogTest::OnlyTheCheckedPairsComeBack()
{
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")}),
                                 Sharing(QStringLiteral("other"), QStringLiteral("hers"), {QStringLiteral("LPMA")}),
                                 Sharing(QStringLiteral("third"), QStringLiteral("yours"), {QStringLiteral("LEBL")})});

    const std::vector<QCheckBox*> boxes = PairBoxesOf(dialog);
    QCOMPARE(boxes.size(), std::size_t{3});

    boxes.at(1)->setChecked(true);

    const std::vector<CoexistingPair> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{1});
    QVERIFY2(chosen.front().one.folderName == "other" && chosen.front().other.folderName == "hers",
             "enabling a category raises many pairs at once, and the user settles them one by one");
}

void SharedAirportsDialogTest::NeitherSideOfTheOverlapIsCalledAProblem()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("flytampa-airport-eham-amsterdam"), QStringLiteral("asobo-airport-eham-amsterdam"),
                 {QStringLiteral("EHAM")})});

    for (const QString& said : TextsOf(dialog))
    {
        for (const QString& blame : {QStringLiteral("problem"), QStringLiteral("broken"), QStringLiteral("wrong"),
                                     QStringLiteral("conflict"), QStringLiteral("error")})
        {
            QVERIFY2(!said.contains(blame, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("the rule of 2026-07-31 forbids calling either side a problem, and "
                                               "this line says \"%1\": %2")
                                    .arg(blame, said)));
        }
    }
}

void SharedAirportsDialogTest::TheWarningSaysTheCodeWasReadFromTheScenery()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});

    const QString said = TextsOf(dialog).join(QLatin1Char(' '));

    QVERIFY2(said.contains(QStringLiteral("scenery files")) && said.contains(QStringLiteral("read so far")),
             "the claim and where it came from travel together, and here the second half is also the limit: the app "
             "compares against the scenery it has read and not against the whole library");
}

QTEST_MAIN(SharedAirportsDialogTest)

#include "tst_shared_airports_dialog.moc"
