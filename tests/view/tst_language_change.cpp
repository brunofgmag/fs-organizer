#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include "view/panels/FoldersOutsideNotice.h"
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
        const QList<QPushButton*> buttons = strip.findChildren<QPushButton*>();

        return std::ranges::any_of(buttons,
                                   [&](const QPushButton* button)
                                   {
                                       return button->text() == text;
                                   });
    }

    bool Ships(const QString& language)
    {
        return std::ranges::any_of(LanguageSwitch::Offered(),
                                   [&](const LanguageSwitch::Offer& offer)
                                   {
                                       return language == QLatin1String(offer.code);
                                   });
    }
}

namespace
{
    class LanguageChangeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen();
        static void InstallingATranslatorRewritesTheNoticeOfFoldersOutsideTheLibrary();
        static void AStoredLanguageIsHonouredAndAnythingElseFallsBackTheSameWay();
        static void ALanguageThatCouldNotBeInstalledIsNotReportedAsInUse();
        static void ATreeViewOverAProxySurvivesTheModelBeingRetranslated();
    };
}

void LanguageChangeTest::InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen()
{
    TriageStrip strip;
    strip.ShowBreakdown({.broken = 1});

    QVERIFY(HasActionLabelled(strip, QStringLiteral("Repair broken links…")));

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY2(HasActionLabelled(strip, QStringLiteral("<Repair broken links…>")),
             "the strip was built before the switch and kept the old text");

    QCoreApplication::removeTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY(HasActionLabelled(strip, QStringLiteral("Repair broken links…")));
}

void LanguageChangeTest::InstallingATranslatorRewritesTheNoticeOfFoldersOutsideTheLibrary()
{
    FoldersOutsideNotice notice;
    notice.ShowFolders(3);

    const QLabel* said = notice.findChild<QLabel*>(QStringLiteral("TriageQuiet"));

    QVERIFY(said != nullptr);
    const QString before = said->text();
    QVERIFY(before.startsWith(QLatin1Char('3')));
    QVERIFY(HasActionLabelled(notice, QStringLiteral("Import into the library…")));

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY2(said->text().startsWith(QLatin1Char('<')), "the notice kept the old text after the switch");
    QVERIFY2(HasActionLabelled(notice, QStringLiteral("<Import into the library…>")),
             "the notice button kept the old text after the switch");

    QCoreApplication::removeTranslator(&marking);
    QCoreApplication::processEvents();

    QCOMPARE(said->text(), before);
    QVERIFY(HasActionLabelled(notice, QStringLiteral("Import into the library…")));
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
