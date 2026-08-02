#include <QtCore/QCoreApplication>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>

#include <QtWidgets/QTreeView>

#include "view/LanguageSwitch.h"
#include "viewmodel/AddonTreeFilterModel.h"
#include "viewmodel/AddonTreeModel.h"
#include "view/shell/TriageStrip.h"

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

    QString ActionLabelled(const QWidget& strip, const QString& text)
    {
        for (const QPushButton* button : strip.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                return button->text();
            }
        }

        return {};
    }
}

namespace
{
    class LanguageChangeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen();
        static void TheStoredLanguageIsHonouredAndAnUnknownOneFallsBack();
        static void ATreeViewOverAProxySurvivesTheModelBeingRetranslated();
    };
}

void LanguageChangeTest::InstallingATranslatorRewritesWhatIsAlreadyOnTheScreen()
{
    TriageStrip strip;
    strip.ShowBreakdown(1, 0, 0, 0);

    QCOMPARE(ActionLabelled(strip, QStringLiteral("Repair the broken ones…")),
             QStringLiteral("Repair the broken ones…"));

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY2(!ActionLabelled(strip, QStringLiteral("<Repair the broken ones…>")).isEmpty(),
             "the strip was built before the switch and kept the old text");

    QCoreApplication::removeTranslator(&marking);
    QCoreApplication::processEvents();

    QCOMPARE(ActionLabelled(strip, QStringLiteral("Repair the broken ones…")),
             QStringLiteral("Repair the broken ones…"));
}

void LanguageChangeTest::TheStoredLanguageIsHonouredAndAnUnknownOneFallsBack()
{
    QCOMPARE(LanguageSwitch::Stored("pt_BR"), QStringLiteral("pt_BR"));
    QCOMPARE(LanguageSwitch::Stored("en"), QStringLiteral("en"));
    QVERIFY2(LanguageSwitch::Stored("kl_GL") == QStringLiteral("en")
                 || LanguageSwitch::Stored("kl_GL") == QStringLiteral("pt_BR"),
             "an unknown language should fall back to one of the two that ship");
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

    model.Retranslated();
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
