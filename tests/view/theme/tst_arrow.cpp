#include <cstdlib>

#include <QtCore/QtMath>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>

#include "view/theme/ArrowButton.h"
#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"

namespace
{
    class ArrowTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheLeftArrowIsTenByEightWithAShaftOfTwoRowsAtFullScale();
        static void TheLeftArrowIsThirteenByTenWithAShaftOfTwoRowsAtAHundredAndTwentyFivePercent_data();
        static void TheLeftArrowIsThirteenByTenWithAShaftOfTwoRowsAtAHundredAndTwentyFivePercent();
        static void UpIsTheTransposeOfLeftAndRightAndDownAreTheirMirrors_data();
        static void UpIsTheTransposeOfLeftAndRightAndDownAreTheirMirrors();
        static void TheArrowPointingBothWaysIsTheSameFromEitherEnd();
        static void TheExtentOfAnUprightArrowIsTheExtentOfALyingOneTurned();
        static void ATabLeadingWithAnArrowIsSixteenWiderAndKeepsItsLabelFreeOfGlyphs();
        static void WithTextTheArrowWidensTheButtonByItselfAndItsGap();
        static void WithoutTextOrWithoutAnArrowTheButtonIsAsWideAsAPlainOne();
        static void ADisabledButtonDrawsItsArrowInTheDisabledInk();
        static void TheGearIsPaintedAtTheRatioOfTheScreenAndNotStretchedToIt();
        static void TheGearIsTheSameOnEitherSideOfItsCentre();
    };

    constexpr int kSurface = 40;
    constexpr qreal kNudge = 0.4;
    constexpr int kTheRasterizerMayDisagreeBy = 2;

    [[nodiscard]] QImage Painted(const ArrowHeading heading, const qreal ratio, const qreal nudge = 0.0)
    {
        const int pixels = qCeil(kSurface * ratio);

        QImage surface(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
        surface.setDevicePixelRatio(ratio);
        surface.fill(Qt::transparent);

        QPainter painter(&surface);
        painter.translate(nudge, nudge);
        PaintArrow(painter, QRectF(0, 0, kSurface, kSurface), heading, Qt::white);
        painter.end();

        return surface;
    }

    [[nodiscard]] QImage InkOf(const QImage& surface)
    {
        QRect bounds;

        for (int row = 0; row < surface.height(); ++row)
        {
            for (int column = 0; column < surface.width(); ++column)
            {
                if (qAlpha(surface.pixel(column, row)) > 0)
                {
                    bounds = bounds.united(QRect(column, row, 1, 1));
                }
            }
        }

        return surface.copy(bounds);
    }

    [[nodiscard]] QImage Transposed(const QImage& image)
    {
        QImage turned(image.height(), image.width(), image.format());

        for (int row = 0; row < image.height(); ++row)
        {
            for (int column = 0; column < image.width(); ++column)
            {
                turned.setPixel(row, column, image.pixel(column, row));
            }
        }

        return turned;
    }

    [[nodiscard]] bool Alike(const QImage& one, const QImage& other)
    {
        if (one.size() != other.size())
        {
            return false;
        }

        for (int row = 0; row < one.height(); ++row)
        {
            for (int column = 0; column < one.width(); ++column)
            {
                if (std::abs(qAlpha(one.pixel(column, row)) - qAlpha(other.pixel(column, row)))
                    > kTheRasterizerMayDisagreeBy)
                {
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]] int FullyInkedRowsAtTheTail(const QImage& lyingArrow)
    {
        int rows = 0;

        for (int row = 0; row < lyingArrow.height(); ++row)
        {
            rows += qAlpha(lyingArrow.pixel(lyingArrow.width() - 1, row)) == 255 ? 1 : 0;
        }

        return rows;
    }

    [[nodiscard]] qsizetype PixelsOf(const QImage& image, const QColor& ink)
    {
        qsizetype found = 0;

        for (int row = 0; row < image.height(); ++row)
        {
            for (int column = 0; column < image.width(); ++column)
            {
                found += image.pixelColor(column, row) == ink ? 1 : 0;
            }
        }

        return found;
    }
}

void ArrowTest::TheLeftArrowIsTenByEightWithAShaftOfTwoRowsAtFullScale()
{
    const QImage ink = InkOf(Painted(ArrowHeading::Left, 1.0));

    QCOMPARE(ink.size(), QSize(10, 8));
    QCOMPARE(FullyInkedRowsAtTheTail(ink), 2);
    QVERIFY2(ink.mirrored(false, true) == ink, "the arrow is not the mirror of itself around its axis");
}

void ArrowTest::TheLeftArrowIsThirteenByTenWithAShaftOfTwoRowsAtAHundredAndTwentyFivePercent_data()
{
    QTest::addColumn<qreal>("nudge");

    QTest::newRow("on the grid") << 0.0;
    QTest::newRow("a logical fraction off the grid") << kNudge;
}

void ArrowTest::TheLeftArrowIsThirteenByTenWithAShaftOfTwoRowsAtAHundredAndTwentyFivePercent()
{
    QFETCH(const qreal, nudge);

    const QImage ink = InkOf(Painted(ArrowHeading::Left, 1.25, nudge));

    QCOMPARE(ink.size(), QSize(13, 10));
    QCOMPARE(FullyInkedRowsAtTheTail(ink), 2);
    QVERIFY2(ink.mirrored(false, true) == ink, "the arrow is not the mirror of itself around its axis");
}

void ArrowTest::UpIsTheTransposeOfLeftAndRightAndDownAreTheirMirrors_data()
{
    QTest::addColumn<qreal>("ratio");

    QTest::newRow("100%") << 1.0;
    QTest::newRow("125%") << 1.25;
}

void ArrowTest::UpIsTheTransposeOfLeftAndRightAndDownAreTheirMirrors()
{
    QFETCH(const qreal, ratio);

    const QImage left = InkOf(Painted(ArrowHeading::Left, ratio));
    const QImage up = InkOf(Painted(ArrowHeading::Up, ratio));

    QVERIFY2(Alike(up, Transposed(left)), "Up is not Left turned a quarter");
    QVERIFY2(Alike(InkOf(Painted(ArrowHeading::Right, ratio)), left.mirrored(true, false)),
             "Right is not Left looked at in a mirror");
    QVERIFY2(Alike(InkOf(Painted(ArrowHeading::Down, ratio)), up.mirrored(false, true)),
             "Down is not Up looked at in a mirror");
}

void ArrowTest::TheArrowPointingBothWaysIsTheSameFromEitherEnd()
{
    for (const qreal ratio : {1.0, 1.25})
    {
        const QImage ink = InkOf(Painted(ArrowHeading::LeftAndRight, ratio));

        QVERIFY(ink.mirrored(true, false) == ink);
        QVERIFY(ink.mirrored(false, true) == ink);
    }
}

void ArrowTest::TheExtentOfAnUprightArrowIsTheExtentOfALyingOneTurned()
{
    const QSizeF left = ArrowExtent(ArrowHeading::Left);

    QCOMPARE(ArrowExtent(ArrowHeading::Up), left.transposed());
    QCOMPARE(ArrowExtent(ArrowHeading::Down), left.transposed());
    QCOMPARE(ArrowExtent(ArrowHeading::Right), left);
    QCOMPARE(ArrowExtent(ArrowHeading::LeftAndRight).height(), left.height());
    QVERIFY(ArrowExtent(ArrowHeading::LeftAndRight).width() > left.width());
}

void ArrowTest::ATabLeadingWithAnArrowIsSixteenWiderAndKeepsItsLabelFreeOfGlyphs()
{
    PageTab tab(QStringLiteral("Back"));
    const QSize bare = tab.sizeHint();

    tab.LeadWith(ArrowHeading::Left);

    QCOMPARE(tab.sizeHint().width(), bare.width() + 16);
    QCOMPARE(tab.sizeHint().height(), bare.height());
    QCOMPARE(tab.minimumSizeHint(), tab.sizeHint());
    QCOMPARE(tab.Label(), QStringLiteral("Back"));
    QVERIFY(!tab.text().contains(QChar(0x2190)));
}

void ArrowTest::WithTextTheArrowWidensTheButtonByItselfAndItsGap()
{
    const QString text = QStringLiteral("v0.58.0");
    const QPushButton plain(text);

    ArrowButton button;
    button.setText(text);
    button.LeadWith(ArrowHeading::Down);

    QCOMPARE(button.sizeHint().width(), plain.sizeHint().width() + 14);
    QCOMPARE(button.sizeHint().height(), plain.sizeHint().height());
    QCOMPARE(button.minimumSizeHint(), button.sizeHint());
}

void ArrowTest::WithoutTextOrWithoutAnArrowTheButtonIsAsWideAsAPlainOne()
{
    const QString text = QStringLiteral("Restart to update");
    const QPushButton plainWithText(text);
    const QPushButton plainBare;

    ArrowButton noText;
    noText.LeadWith(ArrowHeading::Up);

    ArrowButton noArrow;
    noArrow.setText(text);
    noArrow.LeadWith(ArrowHeading::Up);
    noArrow.LeadWith(std::nullopt);

    QCOMPARE(noText.sizeHint(), plainBare.sizeHint());
    QCOMPARE(noText.minimumSizeHint(), noText.sizeHint());
    QCOMPARE(noArrow.sizeHint(), plainWithText.sizeHint());
    QCOMPARE(noArrow.minimumSizeHint(), noArrow.sizeHint());
}

void ArrowTest::ADisabledButtonDrawsItsArrowInTheDisabledInk()
{
    const QPalette palette = ModernistPalette(Qt::ColorScheme::Dark);
    const QColor enabledInk = palette.color(QPalette::Active, QPalette::ButtonText);
    const QColor disabledInk = palette.color(QPalette::Disabled, QPalette::ButtonText);

    QVERIFY2(enabledInk != disabledInk, "the theme gives a disabled button the ink of an enabled one");

    ArrowButton button;
    button.setPalette(palette);
    button.LeadWith(ArrowHeading::Left);
    button.resize(40, 30);

    QImage ready(button.size(), QImage::Format_ARGB32_Premultiplied);
    ready.fill(Qt::transparent);
    button.render(&ready);

    button.setEnabled(false);

    QImage greyed(button.size(), QImage::Format_ARGB32_Premultiplied);
    greyed.fill(Qt::transparent);
    button.render(&greyed);

    QVERIFY2(PixelsOf(ready, enabledInk) > 0, "the arrow of an enabled button is not in the ink of its text");
    QVERIFY2(PixelsOf(greyed, disabledInk) > 0, "the arrow of a disabled button is not in the disabled ink");
    QCOMPARE(PixelsOf(greyed, enabledInk), 0);
}

void ArrowTest::TheGearIsPaintedAtTheRatioOfTheScreenAndNotStretchedToIt()
{
    constexpr int kSide = 14;
    constexpr qreal kRatio = 1.25;

    const int pixels = qCeil(kSide * kRatio);

    QCOMPARE(GearIcon(kSide, kRatio).availableSizes(), QList<QSize>{QSize(pixels, pixels)});
    QCOMPARE(GearIcon(kSide, 1.0).availableSizes(), QList<QSize>{QSize(kSide, kSide)});
    QCOMPARE(GearIcon(kSide, kRatio).pixmap(QSize(kSide, kSide), kRatio).devicePixelRatio(), kRatio);
}

void ArrowTest::TheGearIsTheSameOnEitherSideOfItsCentre()
{
    constexpr int kSide = 14;

    for (const qreal ratio : {1.0, 1.25})
    {
        const QIcon gear = GearIcon(kSide, ratio);
        const QImage painted = gear.pixmap(gear.availableSizes().constFirst()).toImage();

        QVERIFY2(Alike(painted, painted.mirrored(true, false)), "the left half of the gear is not its right half");
        QVERIFY2(Alike(painted, painted.mirrored(false, true)), "the upper half of the gear is not its lower half");
    }
}

QTEST_MAIN(ArrowTest)

#include "tst_arrow.moc"
