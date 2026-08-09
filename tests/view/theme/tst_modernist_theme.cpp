#include <QtGui/QFontDatabase>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtTest/QtTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleOption>

#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistStyle.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class ModernistThemeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheArchivoFamilyResolvesFromTheBinary();
        static void TheBrandIconsResolveFromTheBinary();
        static void EachSchemeGetsItsOwnGroundAndTheSameAccent();
        static void TheStyleSheetLeavesNoPlaceholderBehind();
        static void ApplyingTheThemeSetsTheApplicationFont();
        static void ADisabledDefaultButtonTakesOffTheAccent();
        static void AnOutlineStaysInsideItsBoxAtEveryScale();
        static void ATagPaintsItsWordInsideItsOwnPadding();
        static void ATagKeepsRoomAroundItsWord();
        static void TheFocusRingTakesTheBorderOfAButtonAndTheBoxOfAnItem();
        static void AButtonWearingTheStyleSheetStillShowsWhereTheKeyboardIs();
        static void TheDefaultButtonKeepsItsHoverTone();
    };

    QImage Painted(QWidget& widget)
    {
        QImage surface(widget.size(), QImage::Format_ARGB32_Premultiplied);
        surface.fill(Qt::magenta);

        widget.render(&surface);

        return surface;
    }
}

void ModernistThemeTest::TheArchivoFamilyResolvesFromTheBinary()
{
    ApplyModernistTheme(*qApp);

    QVERIFY(QFontDatabase::families().contains(QStringLiteral("Archivo")));
}

void ModernistThemeTest::TheBrandIconsResolveFromTheBinary()
{
    const QIcon icon = BrandIcon();

    QVERIFY(!icon.isNull());
    QVERIFY(!icon.pixmap(16).isNull());
    QVERIFY(!icon.pixmap(256).isNull());
}

void ModernistThemeTest::EachSchemeGetsItsOwnGroundAndTheSameAccent()
{
    const QPalette dark = ModernistPalette(Qt::ColorScheme::Dark);
    const QPalette light = ModernistPalette(Qt::ColorScheme::Light);

    QVERIFY(dark.color(QPalette::Window) != light.color(QPalette::Window));
    QVERIFY(dark.color(QPalette::WindowText) != light.color(QPalette::WindowText));

    QCOMPARE(dark.color(QPalette::Accent), light.color(QPalette::Accent));
    QVERIFY(dark.color(QPalette::Highlight) != dark.color(QPalette::Accent));

    QVERIFY(dark.color(QPalette::Window).lightness() < dark.color(QPalette::WindowText).lightness());
    QVERIFY(light.color(QPalette::Window).lightness() > light.color(QPalette::WindowText).lightness());

    QVERIFY(dark.color(QPalette::Disabled, QPalette::WindowText) != dark.color(QPalette::Active, QPalette::WindowText));
}

void ModernistThemeTest::TheStyleSheetLeavesNoPlaceholderBehind()
{
    for (const Qt::ColorScheme scheme : {Qt::ColorScheme::Dark, Qt::ColorScheme::Light})
    {
        const QString sheet = ModernistStyleSheet(scheme);

        QVERIFY2(!sheet.contains(QLatin1Char('%')), qPrintable(sheet));
    }
}

void ModernistThemeTest::ApplyingTheThemeSetsTheApplicationFont()
{
    ApplyModernistTheme(*qApp);

    QCOMPARE(QApplication::font().family(), QStringLiteral("Archivo"));
    QCOMPARE(QApplication::font().featureValue("tnum"), 1u);
}

void ModernistThemeTest::ADisabledDefaultButtonTakesOffTheAccent()
{
    QPushButton button(QStringLiteral("Apply"));
    button.setStyleSheet(ModernistStyleSheet(Qt::ColorScheme::Dark));
    button.setDefault(true);
    button.resize(160, 30);

    const QPoint ground(8, button.height() / 2);
    const QColor accent = ModernistPalette(Qt::ColorScheme::Dark).color(QPalette::Accent);

    QPixmap ready(button.size());
    button.render(&ready);
    QCOMPARE(ready.toImage().pixelColor(ground), accent);

    button.setEnabled(false);

    QPixmap greyed(button.size());
    button.render(&greyed);
    QVERIFY(greyed.toImage().pixelColor(ground) != accent);
}

void ModernistThemeTest::AnOutlineStaysInsideItsBoxAtEveryScale()
{
    const QRectF box(0, 0, 40, 20);

    for (const qreal scale : {1.0, 1.25, 1.5, 2.0})
    {
        QImage surface(80, 40, QImage::Format_ARGB32_Premultiplied);
        surface.setDevicePixelRatio(scale);

        QPainter painter(&surface);
        const qreal thin = OneDevicePixel(painter);
        const QRectF outline = OutlineInside(painter, box);

        QCOMPARE(thin, 1.0 / scale);
        QCOMPARE(outline.center(), box.center());
        QVERIFY(outline.top() - box.top() > 0.0);
        QCOMPARE(outline.top() - box.top(), thin / 2.0);
        QCOMPARE(box.bottom() - outline.bottom(), thin / 2.0);
    }
}

void ModernistThemeTest::ATagPaintsItsWordInsideItsOwnPadding()
{
    const QFont base = QApplication::font();
    const QString word = QStringLiteral("GERENCIADA");
    const QSize wanted = TagSizeOf(word, base);

    QImage surface(wanted.width() + 40, wanted.height() + 40, QImage::Format_ARGB32_Premultiplied);
    surface.fill(Qt::black);

    const QRect box(20, 20, wanted.width(), wanted.height());

    QPainter painter(&surface);
    painter.setFont(base);
    PaintTag(painter, box, word, TagTone::Muted, base);
    painter.end();

    const QRgb ground = surface.pixel(box.left() + 2, box.center().y());

    int leftmost = surface.width();
    int rightmost = -1;

    for (int y = box.top() + 1; y < box.bottom(); ++y)
    {
        for (int x = box.left() + 1; x < box.right(); ++x)
        {
            if (surface.pixel(x, y) != ground)
            {
                leftmost = std::min(leftmost, x);
                rightmost = std::max(rightmost, x);
            }
        }
    }

    QVERIFY(rightmost > 0);
    QVERIFY(leftmost - box.left() >= 5);
    QVERIFY(box.right() - rightmost >= 5);
}

void ModernistThemeTest::ATagKeepsRoomAroundItsWord()
{
    const QFont base = QApplication::font();
    const QString word = QStringLiteral("GERENCIADA");

    const QSize box = TagSizeOf(word, base);
    const QFontMetrics measured(TagFont(base));

    const int slackX = box.width() - measured.horizontalAdvance(word.toUpper());
    const int slackY = box.height() - measured.height();

    QVERIFY(slackX >= 16);
    QVERIFY(slackY >= 6);
}

void ModernistThemeTest::TheFocusRingTakesTheBorderOfAButtonAndTheBoxOfAnItem()
{
    const ModernistStyle style(Qt::ColorScheme::Dark);
    const QColor accent = ModernistPalette(Qt::ColorScheme::Dark).color(QPalette::Accent);

    QStyleOptionFocusRect asked;
    asked.rect = QRect(6, 6, 108, 18);
    asked.state = QStyle::State_KeyboardFocusChange;

    QListWidget list;
    list.resize(120, 90);

    QImage onList(list.size(), QImage::Format_ARGB32_Premultiplied);
    onList.fill(Qt::black);

    QPainter overList(&onList);
    style.drawPrimitive(QStyle::PE_FrameFocusRect, &asked, &overList, &list);
    overList.end();

    QCOMPARE(onList.pixelColor(asked.rect.center().x(), asked.rect.top()), accent);
    QCOMPARE(onList.pixelColor(asked.rect.topLeft()), accent);
    QCOMPARE(onList.pixelColor(asked.rect.bottomRight()), accent);
    QVERIFY(onList.pixelColor(list.rect().center().x(), list.rect().bottom()) != accent);

    QPushButton button;
    button.resize(120, 30);

    QImage onButton(button.size(), QImage::Format_ARGB32_Premultiplied);
    onButton.fill(Qt::black);

    QPainter overButton(&onButton);
    style.drawPrimitive(QStyle::PE_FrameFocusRect, &asked, &overButton, &button);
    overButton.end();

    QCOMPARE(onButton.pixelColor(button.rect().center().x(), button.rect().top()), accent);
}

void ModernistThemeTest::AButtonWearingTheStyleSheetStillShowsWhereTheKeyboardIs()
{
    QPushButton button(QStringLiteral("Restore"));
    button.setStyleSheet(ModernistStyleSheet(Qt::ColorScheme::Dark));
    button.resize(160, 30);
    button.show();
    QVERIFY(QTest::qWaitForWindowExposed(&button));

    const QPoint onTheBorder(button.width() / 2, 0);

    button.clearFocus();
    QVERIFY2(!button.hasFocus(), "showing a lone button hands it the focus, so the resting shot has to take it away");

    const QImage resting = Painted(button);

    button.activateWindow();
    button.setFocus();
    QVERIFY2(button.hasFocus(), "without focus the measurement below proves nothing");

    QVERIFY2(Painted(button).pixelColor(onTheBorder) != resting.pixelColor(onTheBorder),
             "the style sheet takes the native focus ring away, so it has to put one back");
}

void ModernistThemeTest::TheDefaultButtonKeepsItsHoverTone()
{
    const QString sheet = ModernistStyleSheet(Qt::ColorScheme::Dark);

    const qsizetype hover = sheet.indexOf(QStringLiteral("QPushButton:default:hover"));
    const qsizetype plain = sheet.indexOf(QStringLiteral("QPushButton:default {"));

    QVERIFY2(hover > plain,
             "with the same specificity the later rule wins, so the default button loses its hover to the rule above");
}

QTEST_MAIN(ModernistThemeTest)

#include "tst_modernist_theme.moc"
