#include <QtCore/QCoreApplication>
#include <QtCore/QTranslator>
#include <QtGui/QFontMetrics>
#include <QtTest/QtTest>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTableView>

#include <numeric>

#include "tests/support/CatalogueBesideTheBuild.h"
#include "view/library/SuggestionDialog.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class SuggestionDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheTableKeepsEveryCellOfASizedColumnWholeAndNeverScrollsSideways_data();
        static void TheTableKeepsEveryCellOfASizedColumnWholeAndNeverScrollsSideways();
        static void ADialogTooNarrowForItsColumnsScrollsAndTheMeasurementSeesIt();
    };
}

namespace
{
    class OpenDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::initStyleOption;
    };

    struct Row
    {
        const char* addon;
        const char* current;
        const char* suggested;
        CategoryRule rule;
    };

    constexpr Row kRows[] = {
        {"paperwing-livery-a320neo-solenne-airways", "Unsorted Downloads", "Aircraft Mods",
         CategoryRule::TheContentTypeIsLivery},
        {"orbx-airport-ybbn-brisbane-international", "Misc", "Scenery Packs", CategoryRule::TheNameSaysAirport},
        {"fsltl-traffic-package-global-airlines-pack", "Aircraft Mods", "Traffic", CategoryRule::TheNameSaysTraffic},
        {"nordic-soundworks-a320-engine-sound-pack", "Downloads", "Sound Packs", CategoryRule::TheContentTypeIsSound},
        {"cityscapes-copenhagen-photogrammetry-landmarks", "Unsorted Downloads", "Sceneries",
         CategoryRule::TheContentTypeIsScenery},
        {"fenix-a320-heavy-weather-radar-livery-set", "Aircraft Mods", "Liveries",
         CategoryRule::TheContentTypeIsLivery},
    };

    std::vector<CategorySuggestion> Realistic()
    {
        std::vector<CategorySuggestion> suggestions;

        for (const Row& row : kRows)
        {
            suggestions.push_back(
                CategorySuggestion{.addonFolder = std::filesystem::path("D:/Library") / row.current / row.addon,
                                   .currentCategory = std::filesystem::path("D:/Library") / row.current,
                                   .suggestedCategory = std::filesystem::path("D:/Library") / row.suggested,
                                   .rule = row.rule});
        }

        return suggestions;
    }

    void Settle(QWidget& dialog)
    {
        dialog.show();
        static_cast<void>(QTest::qWaitForWindowExposed(&dialog));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }

    int WhatTheSectionsAddUpTo(const QTableView& table)
    {
        const QHeaderView* header = table.horizontalHeader();
        int total = 0;

        for (int column = 0; column < header->count(); ++column)
        {
            total += header->sectionSize(column);
        }

        return total;
    }

    QString TheColumnsOf(const QTableView& table)
    {
        const QHeaderView* header = table.horizontalHeader();
        QStringList said;

        for (int column = 0; column < header->count(); ++column)
        {
            said << QStringLiteral("column %1 is %2 px").arg(column).arg(header->sectionSize(column));
        }

        return QStringLiteral("viewport %1 px, sections add up to %2 px, %3")
            .arg(table.viewport()->width())
            .arg(WhatTheSectionsAddUpTo(table))
            .arg(said.join(QStringLiteral(", ")));
    }

    QString WhatTheCellCannotShow(const QTableView& table, const QModelIndex& index)
    {
        const OpenDelegate delegate;
        QStyleOptionViewItem option;
        option.initFrom(&table);
        option.widget = &table;
        delegate.initStyleOption(&option, index);
        option.rect = QRect(0, 0, table.columnWidth(index.column()), table.rowHeight(index.row()));

        const QStyle* style = table.style();
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, &table) + 1;
        const QRect box =
            style->subElementRect(QStyle::SE_ItemViewItemText, &option, &table).adjusted(margin, 0, -margin, 0);
        const QFontMetrics metrics(option.font);

        if (metrics.elidedText(option.text, option.textElideMode, box.width()) == option.text)
        {
            return {};
        }

        return QStringLiteral("\"%1\" asks %2 px and column %3 leaves %4")
            .arg(option.text)
            .arg(metrics.horizontalAdvance(option.text))
            .arg(index.column())
            .arg(box.width());
    }

    QString WhatTheContentSizedCellsCannotShow(const QTableView& table)
    {
        QStringList elided;
        const int slack = table.horizontalHeader()->count() - 1;

        for (int column = 0; column < slack; ++column)
        {
            for (int row = 0; row < table.model()->rowCount({}); ++row)
            {
                if (const QString cut = WhatTheCellCannotShow(table, table.model()->index(row, column)); !cut.isEmpty())
                {
                    elided << cut;
                }
            }
        }

        return elided.join(QStringLiteral("; "));
    }

    QString WhatTheTitlesCannotShow(const QTableView& table)
    {
        const QHeaderView* header = table.horizontalHeader();
        QStringList cut;

        for (int column = 0; column < header->count(); ++column)
        {
            if (header->sectionSize(column) < header->sectionSizeHint(column))
            {
                cut << QStringLiteral("title %1 has %2 px of the %3 it asks")
                           .arg(column)
                           .arg(header->sectionSize(column))
                           .arg(header->sectionSizeHint(column));
            }
        }

        return cut.join(QStringLiteral("; "));
    }
}

void SuggestionDialogTest::TheTableKeepsEveryCellOfASizedColumnWholeAndNeverScrollsSideways_data()
{
    QTest::addColumn<QString>("language");

    QTest::newRow("English") << QStringLiteral("en");
    QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR");
}

void SuggestionDialogTest::TheTableKeepsEveryCellOfASizedColumnWholeAndNeverScrollsSideways()
{
    QFETCH(const QString, language);

    QTranslator catalogue;

    if (language != QLatin1String("en"))
    {
        const QString file = TheCatalogueBesideTheBuild(language);

        QVERIFY2(!file.isEmpty(), "app_pt_BR.qm is not beside the build: build the release_translations target");
        QVERIFY(catalogue.load(file));
        QVERIFY(QCoreApplication::installTranslator(&catalogue));
    }

    ApplyModernistTheme(*qApp);

    SuggestionDialog dialog(Realistic());
    Settle(dialog);

    const auto* table = dialog.findChild<QTableView*>();

    QVERIFY(table != nullptr);
    QCOMPARE(table->model()->rowCount({}), static_cast<int>(std::size(kRows)));

    const QString said = TheColumnsOf(*table);

    QVERIFY2(!table->horizontalScrollBar()->isVisible(), qPrintable(said));
    QCOMPARE(WhatTheSectionsAddUpTo(*table), table->viewport()->width());
    QVERIFY2(WhatTheContentSizedCellsCannotShow(*table).isEmpty(),
             qPrintable(WhatTheContentSizedCellsCannotShow(*table) + QStringLiteral(" in ") + said));
    QVERIFY2(WhatTheTitlesCannotShow(*table).isEmpty(),
             qPrintable(WhatTheTitlesCannotShow(*table) + QStringLiteral(" in ") + said));

    QCoreApplication::removeTranslator(&catalogue);
}

void SuggestionDialogTest::ADialogTooNarrowForItsColumnsScrollsAndTheMeasurementSeesIt()
{
    ApplyModernistTheme(*qApp);

    SuggestionDialog dialog(Realistic());
    dialog.resize(420, dialog.height());
    Settle(dialog);

    const auto* table = dialog.findChild<QTableView*>();

    QVERIFY(table != nullptr);
    QVERIFY2(table->horizontalScrollBar()->isVisible(), qPrintable(TheColumnsOf(*table)));
    QVERIFY(WhatTheSectionsAddUpTo(*table) > table->viewport()->width());
}

QTEST_MAIN(SuggestionDialogTest)

#include "tst_suggestion_dialog.moc"
