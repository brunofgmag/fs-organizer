#include "LibraryScroll.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QTextStream>
#include <QtGui/QPaintEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>

#include "application/SceneryService.h"
#include "application/Session.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/DestinationDivergence.h"
#include "domain/tree/EffectiveDestination.h"
#include "view/library/AddonTreePage.h"
#include "view/shell/MainWindow.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/CoverageViewModel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kSamples = 30;
    constexpr int kRowsInAViewport = 30;

    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    void LetTheWindowSettle()
    {
        for (int pass = 0; pass < 40; ++pass)
        {
            QApplication::processEvents();
        }
    }

    double Median(std::vector<double> samples)
    {
        std::ranges::sort(samples);

        return samples.empty() ? 0 : samples.at(samples.size() / 2);
    }

    class PaintSpy final : public QObject
    {
    public:
        int paints = 0;

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (dynamic_cast<QPaintEvent*>(event) != nullptr)
            {
                ++paints;
            }

            return QObject::eventFilter(watched, event);
        }
    };

    void ReportScroll(const QString& what, const QTreeView& view)
    {
        view.verticalScrollBar()->setValue(0);
        LetTheWindowSettle();

        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);

        std::vector<double> notches;

        for (int notch = 0; notch < kSamples; ++notch)
        {
            QWheelEvent wheel(QPointF(200, 200), view.viewport()->mapToGlobal(QPointF(200, 200)), QPoint(0, 0),
                              QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);

            QElapsedTimer timer;
            timer.start();

            QApplication::sendEvent(view.viewport(), &wheel);
            QApplication::processEvents();

            notches.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }

        view.viewport()->removeEventFilter(&spy);

        Out() << what.leftJustified(36) << QString::number(Median(notches), 'f', 2).rightJustified(8)
              << " ms per notch   paints " << QString::number(spy.paints / double{kSamples}, 'f', 1) << "\n";
        Out().flush();
    }

    struct RowUnderTheLens
    {
        QModelIndex position{};
        QString what{};
    };

    void CollectRows(const AddonTreeModel& model,
                     const QModelIndex& parent,
                     const ProfileSnapshot& snapshot,
                     std::vector<RowUnderTheLens>& found)
    {
        for (int row = 0; row < model.rowCount(parent); ++row)
        {
            const QModelIndex position = model.index(row, 0, parent);
            const TreeNode* node = AddonTreeModel::NodeAt(position);

            if (node != nullptr && node->kind == TreeNodeKind::Addon)
            {
                const bool on = snapshot.enabled.Contains(node->path);
                const QString what =
                    on ? QStringLiteral("an addon that is on") : QStringLiteral("an addon that is off");

                const auto already = std::ranges::find_if(found,
                                                          [&what](const RowUnderTheLens& seen)
                                                          {
                                                              return seen.what == what;
                                                          });

                if (already == found.end())
                {
                    found.push_back({.position = position, .what = what});
                }
            }

            if (node != nullptr && node->kind != TreeNodeKind::Addon)
            {
                const auto already = std::ranges::find_if(found,
                                                          [](const RowUnderTheLens& seen)
                                                          {
                                                              return seen.what == QStringLiteral("a category");
                                                          });

                if (already == found.end())
                {
                    found.push_back({.position = position, .what = QStringLiteral("a category")});
                }
            }

            CollectRows(model, position, snapshot, found);
        }
    }

    double MicrosecondsAsking(const AddonTreeModel& model, const QModelIndex& position, const int role, const int times)
    {
        QElapsedTimer timer;
        timer.start();

        for (int pass = 0; pass < times; ++pass)
        {
            for (int column = 0; column < AddonTreeModel::Columns; ++column)
            {
                static_cast<void>(model.data(position.sibling(position.row(), column), role));
            }
        }

        return static_cast<double>(timer.nsecsElapsed()) / 1e3 / times;
    }

    void ReportTheModelCost(const AddonTreeModel& model, const std::vector<RowUnderTheLens>& rows)
    {
        constexpr int kTimes = 200;

        const std::pair<const char*, int> roles[] = {
            {"Qt::DisplayRole", Qt::DisplayRole},
            {"Qt::CheckStateRole", Qt::CheckStateRole},
            {"Qt::ToolTipRole", Qt::ToolTipRole},
            {"EnabledRole", AddonTreeModel::EnabledRole},
            {"ConflictRole", AddonTreeModel::ConflictRole},
            {"DivergentRole", AddonTreeModel::DivergentRole},
            {"BrokenRole", AddonTreeModel::BrokenRole},
            {"AlarmingRole", AlarmingRole},
            {"TagTextRole", TagTextRole},
            {"TagToneRole", TagToneRole},
            {"EmphasisRole", EmphasisRole},
            {"QuietSuffixRole", QuietSuffixRole},
            {"QuietRole", QuietRole},
            {"AlertRole", AlertRole},
        };

        Out() << "\nAddonTreeModel::data, one row of three columns, microseconds\n";
        Out() << QStringLiteral("role").leftJustified(24);
        for (const RowUnderTheLens& row : rows)
        {
            Out() << row.what.rightJustified(22);
        }
        Out() << "\n";

        std::vector<double> totals(rows.size(), 0);

        for (const auto& [name, role] : roles)
        {
            Out() << QString::fromLatin1(name).leftJustified(24);

            for (std::size_t at = 0; at < rows.size(); ++at)
            {
                const double spent = MicrosecondsAsking(model, rows[at].position, role, kTimes);
                totals[at] += spent;

                Out() << QString::number(spent, 'f', 1).rightJustified(22);
            }

            Out() << "\n";
        }

        Out() << QStringLiteral("the fourteen together").leftJustified(24);
        for (const double total : totals)
        {
            Out() << QString::number(total, 'f', 1).rightJustified(22);
        }
        Out() << "\n";

        Out() << QStringLiteral("a viewport of 30 rows, ms").leftJustified(24);
        for (const double total : totals)
        {
            Out() << QString::number(total * kRowsInAViewport / 1000.0, 'f', 1).rightJustified(22);
        }
        Out() << "\n";
        Out().flush();
    }

    std::vector<const TreeNode*> EveryAddon(const ProfileSnapshot& snapshot)
    {
        std::vector<const TreeNode*> addons;

        for (const TreeNode& library : snapshot.libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                addons.push_back(addon);
            }
        }

        return addons;
    }

    void Report(const QString& what, const double milliseconds)
    {
        Out() << what.leftJustified(44) << QString::number(milliseconds, 'f', 1).rightJustified(10) << " ms\n";
        Out().flush();
    }

    void ReportMicroseconds(const QString& what, const double microseconds)
    {
        Out() << what.leftJustified(44) << QString::number(microseconds, 'f', 2).rightJustified(10) << " us\n";
        Out().flush();
    }

    template<typename Work>
    double Milliseconds(Work&& work)
    {
        QElapsedTimer timer;
        timer.start();

        std::forward<Work>(work)();

        return static_cast<double>(timer.nsecsElapsed()) / 1e6;
    }
}

int MeasureTheAppLibrary(MainWindow& window,
                         AddonTreePage& page,
                         AddonTreeModel& model,
                         CoverageViewModel& coverage,
                         SceneryService& scenery,
                         Session& session)
{
    window.showMaximized();
    LetTheWindowSettle();

    auto* view = page.findChild<QTreeView*>();
    if (view == nullptr)
    {
        Out() << "could not find the library tree\n";
        Out().flush();
        return 2;
    }

    const ProfileSnapshot& snapshot = session.Snapshot();
    const std::vector<const TreeNode*> addons = EveryAddon(snapshot);

    std::size_t on = 0;
    for (const TreeNode* addon : addons)
    {
        on += snapshot.enabled.Contains(addon->path) ? 1 : 0;
    }

    Out() << "libraries: " << snapshot.libraries.size() << "  addons: " << addons.size() << "  on: " << on
          << "  destination entries: " << snapshot.entries.size() << "\n";

    const double expanding = Milliseconds(
        [&view]
        {
            view->expandAll();
            LetTheWindowSettle();
        });

    Out() << "viewport " << view->viewport()->width() << "x" << view->viewport()->height() << "  expandAll "
          << QString::number(expanding, 'f', 0) << " ms\n\n";

    Out() << "scrolling the library, everything expanded\n";
    ReportScroll("  the RowDelegate", *view);

    QAbstractItemDelegate* ours = view->itemDelegate();
    view->setItemDelegate(new QStyledItemDelegate(view));
    LetTheWindowSettle();
    ReportScroll("  the plain Qt delegate", *view);
    view->setItemDelegate(ours);
    LetTheWindowSettle();

    std::vector<RowUnderTheLens> rows;
    CollectRows(model, {}, snapshot, rows);
    ReportTheModelCost(model, rows);

    if (addons.empty())
    {
        return 0;
    }

    Out() << "\nwhat each call under AddonTreeModel::data costs, microseconds\n";

    constexpr int kTimes = 2000;
    const std::filesystem::path& folder = addons.front()->path;

    const auto Micros = [](auto&& work)
    {
        return Milliseconds(work) * 1000 / kTimes;
    };

    ReportMicroseconds("ComparablePath, one path",
                       Micros(
                           [&folder]
                           {
                               for (int pass = 0; pass < kTimes; ++pass)
                               {
                                   static_cast<void>(ComparablePath(folder));
                               }
                           }));

    ReportMicroseconds("EffectiveDestination",
                       Micros(
                           [&session, &folder]
                           {
                               for (int pass = 0; pass < kTimes; ++pass)
                               {
                                   static_cast<void>(EffectiveDestination(session.Profile(), folder));
                               }
                           }));

    ReportMicroseconds("LinksPointingAt, over the entries",
                       Micros(
                           [&snapshot, &folder]
                           {
                               for (int pass = 0; pass < kTimes; ++pass)
                               {
                                   static_cast<void>(LinksPointingAt(snapshot.entries, folder));
                               }
                           }));

    ReportMicroseconds("DestinationItStrayedTo",
                       Micros(
                           [&session, &snapshot, &folder]
                           {
                               for (int pass = 0; pass < kTimes; ++pass)
                               {
                                   static_cast<void>(
                                       DestinationItStrayedTo(session.Profile(), snapshot.entries, folder));
                               }
                           }));

    Out() << "\nwhat one toggle still costs the main thread\n";

    Report("Session::RefreshEntries",
           Milliseconds(
               [&session]
               {
                   session.RefreshEntries();
               }));

    Report("AddonTreeModel::Refresh, with the tree shown",
           Milliseconds(
               [&model, &session]
               {
                   model.Refresh(session.Snapshot(), session.Profile());
                   LetTheWindowSettle();
               }));

    Out() << "\nwhat the check costs the main thread now that it runs on a worker\n";

    QObject::disconnect(&coverage, &CoverageViewModel::TurningThemOnWasChecked, &page, nullptr);

    const std::vector<const TreeNode*> one{addons.front()};

    for (const auto& [what, batch] :
         {std::pair{QStringLiteral("1 addon"), one}, std::pair{QStringLiteral("every addon"), addons}})
    {
        bool answered = false;
        double longestPause = 0;

        const auto watching = QObject::connect(&coverage, &CoverageViewModel::TurningThemOnWasChecked, &coverage,
                                               [&answered](const WhatTurningThemOnFound&)
                                               {
                                                   answered = true;
                                               });

        QElapsedTimer whole;
        whole.start();

        const double handedOver = Milliseconds(
            [&coverage, &batch]
            {
                coverage.CheckWhatWasTurnedOn(batch);
            });

        while (!answered && whole.elapsed() < 120000)
        {
            longestPause = std::max(longestPause,
                                    Milliseconds(
                                        []
                                        {
                                            QApplication::processEvents();
                                        }));

            QThread::msleep(1);
        }

        QObject::disconnect(watching);

        Report(QStringLiteral("CheckWhatWasTurnedOn, %1, blocks the caller").arg(what), handedOver);
        Report(QStringLiteral("  longest pause of the main thread while it ran"), longestPause);
        Report(QStringLiteral("  wall clock until the answer came back"), static_cast<double>(whole.elapsed()));
    }

    Out() << "\nwhat the duplicate check spends before it compares anything\n";

    const std::vector<AddonToRead> known = SceneryService::AddonsOf(session.Profile(), snapshot);

    Report("SceneryService::WhatIsAlreadyKnown, every addon",
           Milliseconds(
               [&scenery, &known]
               {
                   static_cast<void>(scenery.WhatIsAlreadyKnown(known));
               }));

    const std::vector<SceneryOfAnAddon> read = scenery.WhatIsAlreadyKnown(known);

    Report("AirportsOfEachAddon over them",
           Milliseconds(
               [&read]
               {
                   static_cast<void>(AirportsOfEachAddon(read));
               }));

    const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon(read);

    Report("PairsWithWhatIsAlreadyOn, 1 against the rest",
           Milliseconds(
               [&airports, &session]
               {
                   static_cast<void>(
                       PairsWithWhatIsAlreadyOn({airports.front()}, airports, session.Settings().coexistingAirports));
               }));

    return 0;
}
