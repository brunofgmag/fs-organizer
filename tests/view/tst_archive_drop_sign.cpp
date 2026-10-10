#include <QtTest/QtTest>

#include <QtGui/QImage>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include "view/shell/ArchiveDropSign.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/ModernistTones.h"

namespace
{
    class ArchiveDropSignTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();
        static void ThePlateIsTheSentenceInDemiBoldPlusItsAir();
        static void ThePlateSitsBelowAndToTheRightOfThePointer();
        static void NearTheRightEdgeThePlateFlipsToTheLeftOfThePointer();
        static void NearTheBottomThePlateGoesAboveThePointer();
        static void WithThePointerInEachCornerThePlateStaysInsideTheWindow_data();
        static void WithThePointerInEachCornerThePlateStaysInsideTheWindow();
        static void APlateWiderThanTheWindowStartsAtTheGutter();
        static void APlateInAWindowTooShortForItNeverGoesAboveTheGutter();
        static void ThePlateIsRaisedGroundWithAOnePixelOutlineInTheAccent();
        static void TheOutlineOfARefusalIsTheEdgeToneAndNotTheAccent();
        static void TheSentenceIsPaintedInsideThePlate();
        static void TheRowIsOutlinedTwoPixelsThick();
        static void AnEmptyRowPaintsNoOutline();
        static void RefusingForgetsTheRowAndSaysTheRefusal();
        static void RefusingPaintsNoRowOutline();
        static void ASecondCallMovesThePlate();
        static void LeavingHidesTheSign();
        static void TheSignFollowsTheResizeOfItsParent();
        static void TheSignNeverTakesMouseEvents();
        static void NoSentenceIsCutInAWindowOfTenTwentyFourPixels_data();
        static void NoSentenceIsCutInAWindowOfTenTwentyFourPixels();
        static void ASentenceTooLongForTheWindowIsReportedAsCut();
    };

    constexpr int kWindowWide = 1024;
    constexpr int kWindowTall = 720;
    constexpr int kRowOutline = 2;
    constexpr int kPlateOutline = 1;

    struct Window
    {
        Window()
        {
            parent.resize(kWindowWide, kWindowTall);
            parent.show();
        }

        QWidget parent;
    };

    [[nodiscard]] QImage Painted(QWidget& sign)
    {
        QImage surface(sign.size(), QImage::Format_ARGB32_Premultiplied);
        surface.fill(Qt::magenta);

        sign.render(&surface);

        return surface;
    }

    [[nodiscard]] QColor TheAccent()
    {
        return TonesOf(CurrentColorScheme()).accent;
    }

    [[nodiscard]] QString AnInvitation()
    {
        return QStringLiteral("Drop to install into the library.");
    }

    [[nodiscard]] QString ARefusal()
    {
        return QStringLiteral("Only .zip, .rar and .7z archives and addon folders can be installed.");
    }

    [[nodiscard]] QSize PlateSizeOf(const QWidget& sign, const QString& sentence)
    {
        QFont said = sign.font();
        said.setWeight(QFont::DemiBold);

        const QFontMetrics measured(said);

        return {measured.horizontalAdvance(sentence) + 2 * kDropPlateAirAcross,
                measured.height() + 2 * kDropPlateAirAlong};
    }

    [[nodiscard]] QPoint BesideThePointer(const QPoint& pointer)
    {
        return pointer + QPoint(kDropPlateRightOfThePointer, kDropPlateBelowThePointer);
    }

    [[nodiscard]] QStringList InvitationsInEnglish()
    {
        return {QStringLiteral("Drop to install into Scenery · Airports and cities of Europe"),
                QStringLiteral("Drop to install into the library. You choose the category next.")};
    }

    [[nodiscard]] QStringList InvitationsInPortuguese()
    {
        return {QStringLiteral("Solte para instalar em Cenários · Aeroportos e cidades da Europa"),
                QStringLiteral("Solte para instalar na biblioteca. A categoria você escolhe em seguida.")};
    }

    [[nodiscard]] QStringList RefusalsInEnglish()
    {
        return {QStringLiteral("Only .zip, .rar and .7z archives and addon folders can be installed."),
                QStringLiteral("Add a library before installing an addon.")};
    }

    [[nodiscard]] QStringList RefusalsInPortuguese()
    {
        return {QStringLiteral("Só arquivo .zip, .rar ou .7z pode ser instalado."),
                QStringLiteral("Adicione uma biblioteca antes de instalar de um arquivo compactado.")};
    }
}

void ArchiveDropSignTest::initTestCase()
{
    ApplyModernistTheme(*qApp);
}

void ArchiveDropSignTest::ThePlateIsTheSentenceInDemiBoldPlusItsAir()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto({}, AnInvitation(), QPoint(100, 100));

    QCOMPARE(sign.PlateBox().size(), PlateSizeOf(sign, AnInvitation()));

    sign.Refuse(ARefusal(), QPoint(100, 100));

    QCOMPARE(sign.PlateBox().size(), PlateSizeOf(sign, ARefusal()));
}

void ArchiveDropSignTest::ThePlateSitsBelowAndToTheRightOfThePointer()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QPoint pointer(200, 150);

    sign.InviteInto({}, AnInvitation(), pointer);

    QCOMPARE(sign.PlateBox().topLeft(), BesideThePointer(pointer));
}

void ArchiveDropSignTest::NearTheRightEdgeThePlateFlipsToTheLeftOfThePointer()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QPoint pointer(kWindowWide - 40, 150);

    sign.InviteInto({}, AnInvitation(), pointer);

    const QRect plate = sign.PlateBox();

    QCOMPARE(plate.right(), pointer.x() - kDropPlateBackOffFromThePointer);
    QCOMPARE(plate.top(), BesideThePointer(pointer).y());
    QCOMPARE(plate.size(), PlateSizeOf(sign, AnInvitation()));
}

void ArchiveDropSignTest::NearTheBottomThePlateGoesAboveThePointer()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QPoint pointer(200, kWindowTall - 20);

    sign.InviteInto({}, AnInvitation(), pointer);

    const QRect plate = sign.PlateBox();

    QCOMPARE(plate.bottom(), pointer.y() - kDropPlateBackOffFromThePointer);
    QCOMPARE(plate.left(), BesideThePointer(pointer).x());
    QCOMPARE(plate.size(), PlateSizeOf(sign, AnInvitation()));
}

void ArchiveDropSignTest::WithThePointerInEachCornerThePlateStaysInsideTheWindow_data()
{
    QTest::addColumn<QPoint>("pointer");

    QTest::newRow("top left") << QPoint(0, 0);
    QTest::newRow("top right") << QPoint(kWindowWide - 1, 0);
    QTest::newRow("bottom left") << QPoint(0, kWindowTall - 1);
    QTest::newRow("bottom right") << QPoint(kWindowWide - 1, kWindowTall - 1);
}

void ArchiveDropSignTest::WithThePointerInEachCornerThePlateStaysInsideTheWindow()
{
    QFETCH(QPoint, pointer);

    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto({}, AnInvitation(), pointer);

    const QRect plate = sign.PlateBox();
    const QString where =
        QStringLiteral("plate at %1,%2 %3x%4").arg(plate.x()).arg(plate.y()).arg(plate.width()).arg(plate.height());

    QVERIFY2(window.parent.rect().contains(plate), qPrintable(where));
    QVERIFY2(plate.left() >= kPageGutter, qPrintable(where));
    QVERIFY2(plate.top() >= kPageGutter, qPrintable(where));
    QCOMPARE(plate.size(), PlateSizeOf(sign, AnInvitation()));
}

void ArchiveDropSignTest::APlateWiderThanTheWindowStartsAtTheGutter()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.Refuse(ARefusal().repeated(3), QPoint(300, 3));

    QCOMPARE(sign.PlateBox().left(), kPageGutter);
    QVERIFY(sign.PlateBox().top() >= kPageGutter);
}

void ArchiveDropSignTest::APlateInAWindowTooShortForItNeverGoesAboveTheGutter()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    window.parent.resize(kWindowWide, 40);
    QCoreApplication::processEvents();

    sign.InviteInto({}, AnInvitation(), QPoint(500, 30));

    QCOMPARE(sign.PlateBox().top(), kPageGutter);
}

void ArchiveDropSignTest::ThePlateIsRaisedGroundWithAOnePixelOutlineInTheAccent()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto({}, AnInvitation(), QPoint(200, 150));

    const QImage painted = Painted(sign);
    const QRect plate = sign.PlateBox();
    const ModernistTones tones = TonesOf(CurrentColorScheme());
    const QColor untouched(Qt::magenta);
    const int middle = plate.top() + plate.height() / 2;

    QCOMPARE(painted.pixelColor(plate.left(), middle), tones.accent);
    QCOMPARE(painted.pixelColor(plate.right(), middle), tones.accent);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.top()), tones.accent);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.bottom()), tones.accent);

    QCOMPARE(painted.pixelColor(plate.left() + kPlateOutline, plate.top() + kPlateOutline), tones.raised);
    QCOMPARE(painted.pixelColor(plate.right() - kPlateOutline, plate.bottom() - kPlateOutline), tones.raised);

    QCOMPARE(painted.pixelColor(plate.left() - 1, middle), untouched);
    QCOMPARE(painted.pixelColor(plate.right() + 1, middle), untouched);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.top() - 1), untouched);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.bottom() + 1), untouched);
}

void ArchiveDropSignTest::TheOutlineOfARefusalIsTheEdgeToneAndNotTheAccent()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.Refuse(ARefusal(), QPoint(200, 150));

    const QImage painted = Painted(sign);
    const QRect plate = sign.PlateBox();
    const ModernistTones tones = TonesOf(CurrentColorScheme());
    const int middle = plate.top() + plate.height() / 2;

    QVERIFY(sign.Refuses());
    QVERIFY(tones.edge != tones.accent);
    QCOMPARE(painted.pixelColor(plate.left(), middle), tones.edge);
    QCOMPARE(painted.pixelColor(plate.right(), middle), tones.edge);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.top()), tones.edge);
    QCOMPARE(painted.pixelColor(plate.center().x(), plate.bottom()), tones.edge);
    QCOMPARE(painted.pixelColor(plate.left() + kPlateOutline, plate.top() + kPlateOutline), tones.raised);
}

void ArchiveDropSignTest::TheSentenceIsPaintedInsideThePlate()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto({}, AnInvitation(), QPoint(200, 150));

    const QImage painted = Painted(sign);
    const QRect plate = sign.PlateBox();
    const QRect inside = plate.adjusted(kPlateOutline, kPlateOutline, -kPlateOutline, -kPlateOutline);
    const ModernistTones tones = TonesOf(CurrentColorScheme());
    int inked = 0;
    int inTheTextTone = 0;

    for (int y = inside.top(); y <= inside.bottom(); ++y)
    {
        for (int x = inside.left(); x <= inside.right(); ++x)
        {
            inked += painted.pixelColor(x, y) != tones.raised ? 1 : 0;
            inTheTextTone += painted.pixelColor(x, y) == tones.text ? 1 : 0;
        }
    }

    QVERIFY2(inked > AnInvitation().size(), qPrintable(QString::number(inked)));
    QVERIFY2(inTheTextTone > 0, "no pixel of the sentence is in the text tone");
}

void ArchiveDropSignTest::TheRowIsOutlinedTwoPixelsThick()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QRect row(100, 300, 300, 200);

    sign.InviteInto(row, AnInvitation(), QPoint(700, 100));

    const QImage painted = Painted(sign);
    const QColor accent = TheAccent();
    const QColor untouched(Qt::magenta);

    QCOMPARE(sign.Target(), row);
    QVERIFY(!sign.Refuses());

    for (int along = 0; along < kRowOutline; ++along)
    {
        QCOMPARE(painted.pixelColor(row.left() + along, row.top() + 50), accent);
        QCOMPARE(painted.pixelColor(row.right() - along, row.top() + 50), accent);
        QCOMPARE(painted.pixelColor(row.left() + 50, row.top() + along), accent);
        QCOMPARE(painted.pixelColor(row.left() + 50, row.bottom() - along), accent);
    }

    QCOMPARE(painted.pixelColor(row.left() + kRowOutline, row.top() + 50), untouched);
    QCOMPARE(painted.pixelColor(row.right() - kRowOutline, row.top() + 50), untouched);
    QCOMPARE(painted.pixelColor(row.left() + 50, row.top() + kRowOutline), untouched);
    QCOMPARE(painted.pixelColor(row.left() + 50, row.bottom() - kRowOutline), untouched);
    QCOMPARE(painted.pixelColor(row.center()), untouched);
}

void ArchiveDropSignTest::AnEmptyRowPaintsNoOutline()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto({}, AnInvitation(), QPoint(700, 100));

    const QImage painted = Painted(sign);
    const QColor untouched(Qt::magenta);

    QVERIFY(sign.Target().isEmpty());
    QCOMPARE(painted.pixelColor(0, 0), untouched);
    QCOMPARE(painted.pixelColor(kWindowWide - 1, 0), untouched);
    QCOMPARE(painted.pixelColor(0, kWindowTall - 1), untouched);
    QCOMPARE(painted.pixelColor(kWindowWide - 1, kWindowTall - 1), untouched);
    QCOMPARE(painted.pixelColor(kWindowWide / 2, 0), untouched);
    QCOMPARE(painted.pixelColor(kWindowWide / 2, kWindowTall - 1), untouched);
    QCOMPARE(painted.pixelColor(0, kWindowTall / 2), untouched);
    QCOMPARE(painted.pixelColor(kWindowWide - 1, kWindowTall / 2), untouched);
}

void ArchiveDropSignTest::RefusingForgetsTheRowAndSaysTheRefusal()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.InviteInto(QRect(100, 100, 300, 200), AnInvitation(), QPoint(500, 500));
    sign.Refuse(ARefusal(), QPoint(500, 500));

    QVERIFY(sign.Refuses());
    QVERIFY(sign.Target().isEmpty());
    QCOMPARE(sign.Sentence(), ARefusal());
    QCOMPARE(sign.accessibleName(), sign.Sentence());
}

void ArchiveDropSignTest::RefusingPaintsNoRowOutline()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QRect row(100, 100, 300, 200);

    sign.InviteInto(row, AnInvitation(), QPoint(700, 500));
    sign.Refuse(ARefusal(), QPoint(700, 500));

    const QImage painted = Painted(sign);

    QCOMPARE(painted.pixelColor(row.left(), row.top() + 50), QColor(Qt::magenta));
    QCOMPARE(painted.pixelColor(row.left() + 50, row.top()), QColor(Qt::magenta));
}

void ArchiveDropSignTest::ASecondCallMovesThePlate()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QPoint first(100, 100);
    const QPoint second(500, 400);

    sign.InviteInto({}, AnInvitation(), first);

    const QRect before = sign.PlateBox();

    sign.InviteInto({}, AnInvitation(), second);

    const QRect after = sign.PlateBox();
    const QImage painted = Painted(sign);

    QCOMPARE(before.topLeft(), BesideThePointer(first));
    QCOMPARE(after.topLeft(), BesideThePointer(second));
    QCOMPARE(painted.pixelColor(before.left(), before.center().y()), QColor(Qt::magenta));
    QCOMPARE(painted.pixelColor(after.left(), after.center().y()), TheAccent());
}

void ArchiveDropSignTest::LeavingHidesTheSign()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    QVERIFY(!sign.isVisible());

    sign.InviteInto({}, AnInvitation(), QPoint(100, 100));

    QVERIFY(sign.isVisible());

    sign.Leave();

    QVERIFY(!sign.isVisible());

    sign.Refuse(QStringLiteral("Add a library before installing an addon."), QPoint(100, 100));

    QVERIFY(sign.isVisible());

    sign.Leave();

    QVERIFY(!sign.isVisible());
}

void ArchiveDropSignTest::TheSignFollowsTheResizeOfItsParent()
{
    Window window;
    ArchiveDropSign sign(&window.parent);
    const QPoint pointer(kWindowWide - 40, kWindowTall - 20);

    sign.InviteInto({}, AnInvitation(), pointer);

    QCOMPARE(sign.geometry(), window.parent.rect());

    window.parent.resize(1280, 800);
    QCoreApplication::processEvents();

    QCOMPARE(sign.geometry(), window.parent.rect());
    QCOMPARE(sign.PlateBox().topLeft(), BesideThePointer(pointer));

    sign.Leave();
    window.parent.resize(900, 600);
    QCoreApplication::processEvents();

    QCOMPARE(sign.geometry(), window.parent.rect());
}

void ArchiveDropSignTest::TheSignNeverTakesMouseEvents()
{
    Window window;
    auto* beneath = new QPushButton(QStringLiteral("beneath"), &window.parent);
    beneath->setGeometry(10, 10, 120, 40);
    beneath->show();

    ArchiveDropSign sign(&window.parent);
    sign.InviteInto(QRect(0, 0, 400, 300), AnInvitation(), QPoint(500, 400));

    QVERIFY(sign.isVisible());
    QVERIFY(sign.testAttribute(Qt::WA_TransparentForMouseEvents));
    QCOMPARE(window.parent.childAt(20, 20), beneath);

    const QRect plate = sign.PlateBox();

    QVERIFY(window.parent.childAt(plate.center()) == nullptr);
}

void ArchiveDropSignTest::NoSentenceIsCutInAWindowOfTenTwentyFourPixels_data()
{
    QTest::addColumn<QString>("sentence");

    for (const QString& said :
         InvitationsInEnglish() + InvitationsInPortuguese() + RefusalsInEnglish() + RefusalsInPortuguese())
    {
        QTest::newRow(qPrintable(said)) << said;
    }
}

void ArchiveDropSignTest::NoSentenceIsCutInAWindowOfTenTwentyFourPixels()
{
    QFETCH(QString, sentence);

    Window window;
    ArchiveDropSign sign(&window.parent);

    QCOMPARE(window.parent.width(), 1024);

    sign.InviteInto(QRect(100, 100, 300, 200), sentence, QPoint(100, 100));

    QVERIFY2(!sign.IsCut(), "the invitation is cut");

    sign.Refuse(sentence, QPoint(100, 100));

    QVERIFY2(!sign.IsCut(), "the refusal is cut");
}

void ArchiveDropSignTest::ASentenceTooLongForTheWindowIsReportedAsCut()
{
    Window window;
    ArchiveDropSign sign(&window.parent);

    sign.Refuse(RefusalsInEnglish().join(QLatin1Char(' ')).repeated(3), QPoint(100, 100));

    QVERIFY(sign.IsCut());
}

QTEST_MAIN(ArchiveDropSignTest)

#include "tst_archive_drop_sign.moc"
