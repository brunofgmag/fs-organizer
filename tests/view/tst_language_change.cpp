#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include "view/shell/LanguageSwitch.h"
#include "view/shell/TriageStrip.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/ModelRetranslation.h"

namespace
{
    class MarkingTranslator final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString translate(const char*, const char* source, const char*, int) const override
        {
            return QStringLiteral("<%1>").arg(QString::fromUtf8(source));
        }
    };

    bool HasActionLabelled(const QWidget& strip, const QString& text)
    {
        for (const QPushButton* button : strip.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                return true;
            }
        }

        return false;
    }

    bool Ships(const QString& language)
    {
        for (const LanguageSwitch::Offer& offer : LanguageSwitch::Offered())
        {
            if (language == QLatin1String(offer.code))
            {
                return true;
            }
        }

        return false;
    }
}

namespace
{
    class LanguageChangeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen();
        static void AStoredLanguageIsHonouredAndAnythingElseFallsBackTheSameWay();
        static void ALanguageThatCouldNotBeInstalledIsNotReportedAsInUse();
        static void ATreeViewOverAProxySurvivesTheModelBeingRetranslated();
    };
}

void LanguageChangeTest::InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen()
{
    TriageStrip strip;
    strip.ShowBreakdown({.broken = 1});

    QVERIFY(HasActionLabelled(strip, QStringLiteral("Repair the broken ones…")));

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY2(HasActionLabelled(strip, QStringLiteral("<Repair the broken ones…>")),
             "the strip was built before the switch and kept the old text");

    QCoreApplication::removeTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY(HasActionLabelled(strip, QStringLiteral("Repair the broken ones…")));
}

void LanguageChangeTest::AStoredLanguageIsHonouredAndAnythingElseFallsBackTheSameWay()
{
    QCOMPARE(LanguageSwitch::Resolve(QStringLiteral("pt_BR")), QStringLiteral("pt_BR"));
    QCOMPARE(LanguageSwitch::Resolve(QStringLiteral("en")), QStringLiteral("en"));

    const QString unknown = LanguageSwitch::Resolve(QStringLiteral("kl_GL"));
    const QString absent = LanguageSwitch::Resolve({});

    QVERIFY2(unknown != QStringLiteral("kl_GL"), "a language the app does not ship was echoed back");
    QVERIFY2(Ships(unknown), "the fallback landed on a language the app does not ship");
    QCOMPARE(unknown, absent);

    const bool systemSpeaksPortuguese = QLocale::system().name().startsWith(QLatin1String("pt"));
    QCOMPARE(absent, systemSpeaksPortuguese ? QStringLiteral("pt_BR") : QStringLiteral("en"));
}

void LanguageChangeTest::ALanguageThatCouldNotBeInstalledIsNotReportedAsInUse()
{
    LanguageSwitch language;

    QVERIFY(language.Use(QStringLiteral("en")));
    QCOMPARE(language.InUse(), QStringLiteral("en"));

    QVERIFY2(!language.Use(QStringLiteral("pt_BR")),
             "applying a language whose catalogue is missing must report failure");
    QCOMPARE(language.InUse(), QStringLiteral("en"));
}

void LanguageChangeTest::ATreeViewOverAProxySurvivesTheModelBeingRetranslated()
{
    TreeNode addon;
    addon.kind = TreeNodeKind::Addon;
    addon.path = "D:/Library/Aircrafts/pmdg";
    addon.addon = Addon{.folderPath = "D:/Library/Aircrafts/pmdg", .manifest = Manifest{}};

    TreeNode category;
    category.kind = TreeNodeKind::Category;
    category.path = "D:/Library/Aircrafts";
    category.children = {addon};

    TreeNode library;
    library.kind = TreeNodeKind::Library;
    library.path = "D:/Library";
    library.children = {category};

    ProfileSnapshot snapshot;
    snapshot.libraries = {library};

    SimulatorProfile profile;
    profile.id = "msfs2024";
    profile.destinations = {"E:/Community"};
    profile.defaultDestination = "E:/Community";

    AddonTreeModel model;
    model.Show(snapshot, profile);

    AddonTreeFilterModel proxy;
    proxy.setSourceModel(&model);

    QTreeView view;
    view.setModel(&proxy);
    view.expandAll();

    const QPersistentModelIndex kept(proxy.index(0, 0, proxy.index(0, 0, {})));
    view.show();
    QCoreApplication::processEvents();

    view.hide();
    QCoreApplication::processEvents();

    SayTheModelWasRetranslated(model);
    QCoreApplication::processEvents();

    view.show();
    QCoreApplication::processEvents();

    QVERIFY(kept.isValid());
    const QModelIndex inTheSource = proxy.mapToSource(kept);
    QVERIFY2(inTheSource.isValid(), "the proxy could not map back the index it had kept");
    QVERIFY(proxy.mapFromSource(inTheSource).isValid());

    view.expandAll();
    view.collapseAll();
    view.expandAll();
    QCoreApplication::processEvents();

    QVERIFY2(view.model() != nullptr, "the view lost its model after the language change");
    QCOMPARE(proxy.rowCount({}), 1);
}

QTEST_MAIN(LanguageChangeTest)
#include "tst_language_change.moc"
