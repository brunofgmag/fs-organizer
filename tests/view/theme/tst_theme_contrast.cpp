#include <cmath>
#include <cstring>

#include <QtTest/QtTest>

#include "view/theme/ModernistTones.h"

namespace
{
    constexpr double kBodyTextMinimum = 4.5;

    struct Tone
    {
        const char* name;
        QColor ModernistTones::* value;
    };

    constexpr Tone kWindow{.name = "window", .value = &ModernistTones::window};
    constexpr Tone kChrome{.name = "chrome", .value = &ModernistTones::chrome};
    constexpr Tone kRaised{.name = "raised", .value = &ModernistTones::raised};
    constexpr Tone kAlarming{.name = "alarming", .value = &ModernistTones::alarming};
    constexpr Tone kAccent{.name = "accent", .value = &ModernistTones::accent};
    constexpr Tone kAccentWarm{.name = "accentWarm", .value = &ModernistTones::accentWarm};
    constexpr Tone kText{.name = "text", .value = &ModernistTones::text};
    constexpr Tone kSecondary{.name = "secondary", .value = &ModernistTones::secondary};
    constexpr Tone kFaint{.name = "faint", .value = &ModernistTones::faint};
    constexpr Tone kAccentInk{.name = "accentInk", .value = &ModernistTones::accentInk};
    constexpr Tone kOnAccent{.name = "onAccent", .value = &ModernistTones::onAccent};

    struct DeclaredPair
    {
        Tone ink;
        Tone ground;
    };

    QList<DeclaredPair> Wherever(const Tone& ink, const QList<Tone>& grounds)
    {
        QList<DeclaredPair> pairs;

        for (const Tone& ground : grounds)
        {
            pairs.append({.ink = ink, .ground = ground});
        }

        return pairs;
    }

    QList<DeclaredPair> DeclaredPairs()
    {
        return Wherever(kText, {kWindow, kChrome, kRaised, kAlarming})
            + Wherever(kSecondary, {kWindow, kChrome, kRaised, kAlarming}) + Wherever(kFaint, {kWindow})
            + Wherever(kAccentInk, {kWindow, kChrome, kRaised, kAlarming})
            + Wherever(kOnAccent, {kAccent, kAccentWarm});
    }

    struct NamedScheme
    {
        const char* name;
        Qt::ColorScheme scheme;
    };

    QList<NamedScheme> SchemesThatShip()
    {
        return {
            {.name = "dark", .scheme = Qt::ColorScheme::Dark},
            {.name = "light", .scheme = Qt::ColorScheme::Light},
        };
    }

    double Linearised(const double channel)
    {
        return channel <= 0.03928 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    }

    double RelativeLuminance(const QColor& tone)
    {
        return 0.2126 * Linearised(tone.redF()) + 0.7152 * Linearised(tone.greenF())
            + 0.0722 * Linearised(tone.blueF());
    }

    double ContrastRatio(const QColor& ink, const QColor& ground)
    {
        const double one = RelativeLuminance(ink);
        const double other = RelativeLuminance(ground);

        return (std::max(one, other) + 0.05) / (std::min(one, other) + 0.05);
    }

    QColor ValueOf(const Tone& tone, const ModernistTones& tones)
    {
        return tones.*tone.value;
    }

    double RatioOf(const DeclaredPair& pair, const ModernistTones& tones)
    {
        return ContrastRatio(ValueOf(pair.ink, tones), ValueOf(pair.ground, tones));
    }

    QString Complaint(const char* scheme, const DeclaredPair& pair, const double ratio)
    {
        return QStringLiteral("%1: %2 on %3 measures %4:1, and body text needs %5:1")
            .arg(QString::fromLatin1(scheme))
            .arg(QString::fromLatin1(pair.ink.name))
            .arg(QString::fromLatin1(pair.ground.name))
            .arg(QString::number(ratio, 'f', 2))
            .arg(QString::number(kBodyTextMinimum, 'f', 2));
    }

    const char* WorstGroundFor(const char* ink, const ModernistTones& tones)
    {
        const char* worst = nullptr;
        double lowest = 0.0;

        for (const DeclaredPair& pair : DeclaredPairs())
        {
            if (std::strcmp(pair.ink.name, ink) != 0)
            {
                continue;
            }

            if (const double ratio = RatioOf(pair, tones); worst == nullptr || ratio < lowest)
            {
                worst = pair.ground.name;
                lowest = ratio;
            }
        }

        return worst;
    }

    class ThemeContrastTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryDeclaredPairReachesTheMinimumInBothSchemes();
        static void TheWorstGroundIsTheOneThatDecides();
        static void TheHoverFillCarriesItsInkLikeTheRestingFill();
        static void TheDisabledInkKeepsItsValueAndStaysOutOfTheJudgement();
        static void TheComplaintNamesThePairTheMeasurementAndTheMinimum();
    };
}

void ThemeContrastTest::EveryDeclaredPairReachesTheMinimumInBothSchemes()
{
    QStringList complaints;

    for (const auto& [name, scheme] : SchemesThatShip())
    {
        const ModernistTones tones = TonesOf(scheme);

        for (const DeclaredPair& pair : DeclaredPairs())
        {
            if (const double ratio = RatioOf(pair, tones); ratio < kBodyTextMinimum)
            {
                complaints.append(Complaint(name, pair, ratio));
            }
        }
    }

    QVERIFY2(complaints.isEmpty(), qPrintable(QStringLiteral("\n") + complaints.join(QLatin1Char('\n'))));
}

void ThemeContrastTest::TheWorstGroundIsTheOneThatDecides()
{
    const ModernistTones light = TonesOf(Qt::ColorScheme::Light);

    QCOMPARE(WorstGroundFor(kSecondary.name, light), kAlarming.name);
    QVERIFY(ContrastRatio(light.secondary, light.alarming) < ContrastRatio(light.secondary, light.window));

    for (const auto& [name, scheme] : SchemesThatShip())
    {
        const ModernistTones tones = TonesOf(scheme);

        QVERIFY(tones.alarming.isValid());
        QVERIFY(tones.alarming != tones.window);
    }
}

void ThemeContrastTest::TheHoverFillCarriesItsInkLikeTheRestingFill()
{
    for (const auto& [name, scheme] : SchemesThatShip())
    {
        const ModernistTones tones = TonesOf(scheme);

        QVERIFY(tones.accentWarm != tones.accent);
        QVERIFY(ContrastRatio(tones.onAccent, tones.accent) >= kBodyTextMinimum);
        QVERIFY(ContrastRatio(tones.onAccent, tones.accentWarm) >= kBodyTextMinimum);
    }
}

void ThemeContrastTest::TheDisabledInkKeepsItsValueAndStaysOutOfTheJudgement()
{
    QCOMPARE(TonesOf(Qt::ColorScheme::Dark).disabled, QColor("#6b6560"));
    QCOMPARE(TonesOf(Qt::ColorScheme::Light).disabled, QColor("#9b9797"));

    for (const DeclaredPair& pair : DeclaredPairs())
    {
        QVERIFY(pair.ink.value != &ModernistTones::disabled);
        QVERIFY(pair.ground.value != &ModernistTones::disabled);
    }

    for (const auto& [name, scheme] : SchemesThatShip())
    {
        const ModernistTones tones = TonesOf(scheme);

        QVERIFY(ContrastRatio(tones.disabled, tones.window) < kBodyTextMinimum);
        QVERIFY(ContrastRatio(tones.faint, tones.window) > ContrastRatio(tones.disabled, tones.window));
    }
}

void ThemeContrastTest::TheComplaintNamesThePairTheMeasurementAndTheMinimum()
{
    const DeclaredPair broken{.ink = kAccentInk, .ground = kAlarming};
    const QString said = Complaint("light", broken, 3.18);

    QVERIFY(said.contains(QStringLiteral("light")));
    QVERIFY(said.contains(QString::fromLatin1(kAccentInk.name)));
    QVERIFY(said.contains(QString::fromLatin1(kAlarming.name)));
    QVERIFY(said.contains(QString::number(3.18, 'f', 2)));
    QVERIFY(said.contains(QString::number(kBodyTextMinimum, 'f', 2)));
}

QTEST_MAIN(ThemeContrastTest)

#include "tst_theme_contrast.moc"
