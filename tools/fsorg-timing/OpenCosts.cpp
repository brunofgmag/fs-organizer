#include "OpenCosts.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QAbstractProxyModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeView>

#include "SessionForMeasuring.h"
#include "application/DocumentService.h"
#include "application/LoadReport.h"
#include "application/ProfileService.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/model/EnabledAddons.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/DestinationDivergence.h"
#include "domain/tree/EffectiveDestination.h"
#include "infrastructure/sim/LoadingReportLocations.h"
#include "infrastructure/sim/LoadingReportText.h"
#include "infrastructure/sim/ProfileLoadingReport.h"
#include "infrastructure/sim/WindowsUserCfgLocations.h"
#include "support/PathText.h"
#include "view/library/AddonTreePage.h"
#include "view/shell/MainWindow.h"
#include "viewmodel/AddonDocumentsViewModel.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/AddonTreeViewModel.h"

namespace
{
    constexpr int kRuns = 3;
    constexpr double kFrame = 16.7;
    constexpr qint64 kGiveUpAfter = 120000;
    constexpr qsizetype kWhatColumn = 30;

    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    template<typename Work>
    double Milliseconds(Work&& work)
    {
        QElapsedTimer timer;
        timer.start();

        std::forward<Work>(work)();

        return static_cast<double>(timer.nsecsElapsed()) / 1e6;
    }

    double Median(std::vector<double> samples)
    {
        std::ranges::sort(samples);

        return samples.empty() ? 0 : samples.at(samples.size() / 2);
    }

    QString Fixed(const double milliseconds, const int places = 2)
    {
        return QString::number(milliseconds, 'f', places);
    }

    QString ArgumentValue(const QString& name)
    {
        const QString prefix = name + QLatin1Char('=');

        for (const QString& argument : QCoreApplication::arguments())
        {
            if (argument.startsWith(prefix))
            {
                return argument.mid(prefix.size());
            }
        }

        return {};
    }

    void MeasureTheLoadSection(const FilesystemProbe& filesystemProbe, const Session& session)
    {
        Out() << "\nDiagnostics, the Load section: what ShowTheLoad does on the main thread, ms\n";

        const std::filesystem::path path = LoadingReportOf(
            LoadingReportLocations(WindowsUserCfgLocations(), filesystemProbe), session.Profile().variant);
        const std::optional<std::string> contents = path.empty() ? std::nullopt : filesystemProbe.ContentsOf(path);

        if (!contents.has_value())
        {
            Out() << "  no Report-loading.toml found for this variant\n";
            Out().flush();
            return;
        }

        const ProfileLoadingReport source(filesystemProbe, path);
        const ProfileSnapshot& snapshot = session.Snapshot();
        const std::optional<LoadingReport> report = source.LastReport();

        std::size_t addons = 0;
        for (const TreeNode& library : snapshot.libraries)
        {
            addons += CountAddons(library);
        }

        Out() << "  " << AsText(path) << "\n  " << contents->size() << " bytes, " << std::ranges::count(*contents, '\n')
              << " lines, " << (report.has_value() ? report->modules.size() : 0) << " modules, " << addons
              << " addons in the libraries\n";

        for (int run = 1; run <= kRuns; ++run)
        {
            std::optional<std::string> read;
            LoadingReport parsed;
            std::optional<LoadingReport> last;
            LoadDiagnostics load;

            const double reading = Milliseconds(
                [&]
                {
                    read = filesystemProbe.ContentsOf(path);
                });
            const double parsing = Milliseconds(
                [&]
                {
                    parsed = LoadingReportFrom(*read);
                });
            const double lastReport = Milliseconds(
                [&]
                {
                    last = source.LastReport();
                });
            const double reporting = Milliseconds(
                [&]
                {
                    load = ReportTheLoad(last, snapshot);
                });

            const double whole = lastReport + reporting;

            Out() << "  run " << run << "  read the file " << Fixed(reading) << "  parse " << Fixed(parsing)
                  << "  LastReport (read and parse) " << Fixed(lastReport) << "  ReportTheLoad " << Fixed(reporting)
                  << "  together " << Fixed(whole) << "  under one " << Fixed(kFrame, 1)
                  << " ms frame: " << (whole < kFrame ? "yes" : "NO") << "  (modules " << load.modules.size() << ")\n";
        }

        Out().flush();
    }

    struct CategoryCost
    {
        const TreeNode* category = nullptr;
        std::size_t addons = 0;
        std::vector<double> asInstalled{};
        std::vector<double> unanimous{};
    };

    std::vector<double> TimesOf(const TreeNode& category, const std::vector<DestinationEntry>& entries)
    {
        std::vector<double> runs;
        runs.reserve(kRuns);

        for (int run = 0; run < kRuns; ++run)
        {
            runs.push_back(Milliseconds(
                [&]
                {
                    static_cast<void>(WhereTheEnabledAddonsPoint(category, entries));
                }));
        }

        return runs;
    }

    std::vector<DestinationEntry> AllInOneDestination(const std::vector<DestinationEntry>& entries)
    {
        std::vector<DestinationEntry> together = entries;

        if (together.empty())
        {
            return together;
        }

        const std::filesystem::path destination = together.front().path.parent_path();

        for (DestinationEntry& entry : together)
        {
            entry.path = destination / entry.path.filename();
        }

        return together;
    }

    QString TimesText(const std::vector<double>& runs)
    {
        QStringList texts;

        for (const double run : runs)
        {
            texts.append(Fixed(run, 1));
        }

        return texts.join(QLatin1Char(' '));
    }

    QString TheAnswerOf(const TreeNode& category, const std::vector<DestinationEntry>& entries)
    {
        const DestinationAgreement agreement = WhereTheEnabledAddonsPoint(category, entries);

        if (!agreement.unanimous)
        {
            return QStringLiteral("divergent, it stops at the first addon that disagrees");
        }

        return agreement.destination.empty() ? QStringLiteral("no enabled addon") : QStringLiteral("unanimous");
    }

    void MeasureTheDestinationDivergence(const Session& session)
    {
        Out() << "\nWhereTheEnabledAddonsPoint, over every category of the install, " << kRuns << " runs each\n";

        const ProfileSnapshot& snapshot = session.Snapshot();
        const std::vector<DestinationEntry> unanimousEntries = AllInOneDestination(snapshot.entries);

        std::vector<CategoryCost> costs;
        for (const TreeNode& library : snapshot.libraries)
        {
            for (const TreeNode* category : CategoriesUnder(library))
            {
                costs.push_back({.category = category,
                                 .addons = CountAddons(*category),
                                 .asInstalled = TimesOf(*category, snapshot.entries),
                                 .unanimous = TimesOf(*category, unanimousEntries)});
            }
        }

        if (costs.empty())
        {
            Out() << "  no category\n";
            Out().flush();
            return;
        }

        double installedTogether = 0;
        double unanimousTogether = 0;
        for (const CategoryCost& cost : costs)
        {
            installedTogether += Median(cost.asInstalled);
            unanimousTogether += Median(cost.unanimous);
        }

        Out() << "  categories: " << costs.size() << "  destination entries: " << snapshot.entries.size()
              << "\n  every category one after the other, median of each: as the install is "
              << Fixed(installedTogether, 1) << " ms, with every link in one destination "
              << Fixed(unanimousTogether, 1) << " ms\n";

        std::ranges::sort(costs,
                          [](const CategoryCost& left, const CategoryCost& right)
                          {
                              return left.addons > right.addons;
                          });

        Out() << QStringLiteral("  category").leftJustified(24) << QStringLiteral("kind").leftJustified(10)
              << QStringLiteral("addons").leftJustified(8) << QStringLiteral("as installed, ms").leftJustified(34)
              << QStringLiteral("one destination, ms").leftJustified(34) << "the answer as installed\n";

        for (const CategoryCost& cost : costs)
        {
            Out() << ("  " + AsText(cost.category->path.filename())).leftJustified(24)
                  << QString(cost.category->kind == TreeNodeKind::Library ? "library" : "category").leftJustified(10)
                  << QString::number(cost.addons).leftJustified(8) << TimesText(cost.asInstalled).leftJustified(34)
                  << TimesText(cost.unanimous).leftJustified(34) << TheAnswerOf(*cost.category, snapshot.entries)
                  << "\n";
        }

        Out().flush();
    }

    struct Landings
    {
        bool documents = false;
        bool size = false;
    };

    struct Watched
    {
        double firstPass = 0;
        bool landedInTheFirstPass = false;
        double longestPause = 0;
        QString landing{};
        double longestQuietPause = 0;
        double wallClock = 0;
        bool landed = false;
    };

    QString NameWhatLanded(const Landings& before, const Landings& after)
    {
        QStringList names;

        if (!before.documents && after.documents)
        {
            names.append(QStringLiteral("documents"));
        }

        if (!before.size && after.size)
        {
            names.append(QStringLiteral("size"));
        }

        return names.join(QStringLiteral(" + "));
    }

    QModelIndex IndexOf(const AddonTreeModel& model, const QModelIndex& parent, const TreeNode& wanted)
    {
        for (int row = 0; row < model.rowCount(parent); ++row)
        {
            const QModelIndex position = model.index(row, 0, parent);
            const TreeNode* node = AddonTreeModel::NodeAt(position);

            if (node != nullptr && ComparablePath(node->path) == ComparablePath(wanted.path))
            {
                return position;
            }

            if (const QModelIndex deeper = IndexOf(model, position, wanted); deeper.isValid())
            {
                return deeper;
            }
        }

        return {};
    }

    EntriesRead WithoutTheAddon(const EntriesRead& base, const TreeNode& addon)
    {
        EntriesRead without = base;

        std::erase_if(without.entries,
                      [&addon](const DestinationEntry& entry)
                      {
                          return ComparablePath(entry.target) == ComparablePath(addon.path);
                      });
        without.enabled = EnabledAddons{EnabledAddonFolders(without.entries)};

        return without;
    }

    EntriesRead WithTheAddon(const EntriesRead& base, const TreeNode& addon, const SimulatorProfile& profile)
    {
        EntriesRead with = base;

        DestinationEntry stand;
        stand.path = EffectiveDestination(profile, addon.path) / addon.path.filename();
        stand.target = addon.path;
        stand.classification = EntryClassification::Managed;

        with.entries.push_back(stand);
        with.enabled = EnabledAddons{EnabledAddonFolders(with.entries)};

        return with;
    }

    class SelectionBench
    {
    public:
        SelectionBench(QTreeView& view,
                       AddonTreeModel& model,
                       AddonTreeViewModel& treeViewModel,
                       AddonDocumentsViewModel& documentsViewModel,
                       const ProfileService& profileService,
                       ColdableSceneryCache& sceneryCache,
                       Session& session)
            : view_(view),
              model_(model),
              profileService_(profileService),
              sceneryCache_(sceneryCache),
              session_(session)
        {
            const auto stall =
                static_cast<unsigned long>(ArgumentValue(QStringLiteral("--stall-documents-ms")).toInt());

            QObject::connect(&documentsViewModel, &AddonDocumentsViewModel::Indexed, &documentsViewModel,
                             [this, stall]
                             {
                                 QThread::msleep(stall);
                                 landings_.documents = true;
                             });
            QObject::connect(&treeViewModel, &AddonTreeViewModel::SizeMeasured, &treeViewModel,
                             [this](const SelectionSize&)
                             {
                                 landings_.size = true;
                             });
        }

        void Run(const TreeNode& addon, const QString& what, const bool cold)
        {
            sceneryCache_.Forget(cold);

            const QString cache = cold ? QStringLiteral("cold") : QStringLiteral("warm");

            if (!cold)
            {
                Select(addon);
                static_cast<void>(Watch());
            }

            for (int run = 1; run <= kRuns; ++run)
            {
                const QString tag = QStringLiteral("%1, cache %2").arg(what, cache);

                Clear();

                const double selecting = Select(addon);
                Row(tag, QStringLiteral("select"), run, selecting, Watch());

                for (const auto& [phase, read] : TransitionsOf(addon))
                {
                    const double adopting = Adopt(read);
                    Row(tag, phase, run, adopting, Watch());
                }
            }
        }

        void Header() const
        {
            Out() << "\n"
                  << QStringLiteral("selection").leftJustified(kWhatColumn) << QStringLiteral("phase").leftJustified(9)
                  << QStringLiteral("run").leftJustified(4) << QStringLiteral("call ms").leftJustified(10)
                  << QStringLiteral("first pass").leftJustified(12) << QStringLiteral("landing pause").leftJustified(15)
                  << QStringLiteral("coincided with").leftJustified(18)
                  << QStringLiteral("quiet pause").leftJustified(13) << "wall until both landed\n";
            Out().flush();
        }

    private:
        struct Transition
        {
            QString phase{};
            EntriesRead read{};
        };

        [[nodiscard]] std::vector<Transition> TransitionsOf(const TreeNode& addon) const
        {
            const EntriesRead base = ReadBase();
            const EntriesRead without = WithoutTheAddon(base, addon);

            if (without.entries.size() != base.entries.size())
            {
                return {{.phase = QStringLiteral("disable"), .read = without},
                        {.phase = QStringLiteral("enable"), .read = base}};
            }

            return {{.phase = QStringLiteral("enable"), .read = WithTheAddon(base, addon, session_.Profile())},
                    {.phase = QStringLiteral("disable"), .read = base}};
        }

        [[nodiscard]] EntriesRead ReadBase() const
        {
            return profileService_.ReadEntries(session_.StampForAnEntriesRead(), session_.Snapshot().libraries);
        }

        double Adopt(EntriesRead read)
        {
            read.stamp = session_.StampForAnEntriesRead();
            landings_ = {};

            return Milliseconds(
                [&]
                {
                    session_.AdoptTheEntriesRead(std::move(read));
                });
        }

        void Clear() const
        {
            view_.selectionModel()->clear();

            for (int pass = 0; pass < 20; ++pass)
            {
                QApplication::processEvents();
            }
        }

        double Select(const TreeNode& addon)
        {
            const QModelIndex source = IndexOf(model_, {}, addon);
            const auto* proxy = qobject_cast<const QAbstractProxyModel*>(view_.model());
            const QModelIndex shown = proxy != nullptr ? proxy->mapFromSource(source) : source;

            landings_ = {};

            return Milliseconds(
                [&]
                {
                    view_.scrollTo(shown);
                    view_.selectionModel()->setCurrentIndex(
                        shown, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                });
        }

        Watched Watch()
        {
            Watched watched;

            QElapsedTimer whole;
            whole.start();

            bool first = true;

            while (!(landings_.documents && landings_.size) && whole.elapsed() < kGiveUpAfter)
            {
                const Landings before = landings_;
                const double pause = Milliseconds(
                    []
                    {
                        QApplication::processEvents();
                    });
                const QString landing = NameWhatLanded(before, landings_);

                if (first)
                {
                    watched.firstPass = pause;
                    watched.landedInTheFirstPass = !landing.isEmpty();
                    first = false;
                }
                else if (landing.isEmpty())
                {
                    watched.longestQuietPause = std::max(watched.longestQuietPause, pause);
                }
                else if (pause > watched.longestPause)
                {
                    watched.longestPause = pause;
                    watched.landing = landing;
                }

                QThread::msleep(1);
            }

            watched.wallClock = static_cast<double>(whole.elapsed());
            watched.landed = landings_.documents && landings_.size;

            return watched;
        }

        static void
        Row(const QString& what, const QString& phase, const int run, const double call, const Watched& watched)
        {
            Out() << what.leftJustified(kWhatColumn) << phase.leftJustified(9) << QString::number(run).leftJustified(4)
                  << Fixed(call).leftJustified(10)
                  << (Fixed(watched.firstPass) + (watched.landedInTheFirstPass ? "*" : "")).leftJustified(12)
                  << Fixed(watched.longestPause).leftJustified(15)
                  << (watched.landing.isEmpty() ? QStringLiteral("-") : watched.landing).leftJustified(18)
                  << Fixed(watched.longestQuietPause).leftJustified(13) << Fixed(watched.wallClock, 0)
                  << (watched.landed ? "" : "  NOT BOTH LANDED") << "\n";
            Out().flush();
        }

        QTreeView& view_;
        AddonTreeModel& model_;
        const ProfileService& profileService_;
        ColdableSceneryCache& sceneryCache_;
        Session& session_;
        Landings landings_{};
    };

    const TreeNode* AddonWhoseFolderHolds(const std::vector<const TreeNode*>& addons, const QString& text)
    {
        const auto match =
            std::ranges::find_if(addons,
                                 [&text](const TreeNode* addon)
                                 {
                                     return AsText(addon->path.filename()).contains(text, Qt::CaseInsensitive);
                                 });

        return match == addons.end() ? nullptr : *match;
    }

    std::size_t WhatTheyHold(const DocumentsOfAnAddon& indexed)
    {
        return indexed.documents.size() + indexed.airports.size();
    }

    const TreeNode* TheAirportWithTheMostDocuments(const Session& session,
                                                   SceneryService& scenery,
                                                   const DocumentService& documentService)
    {
        const TreeNode* best = nullptr;
        std::size_t most = 0;

        for (const AddonToRead& toRead : SceneryService::AddonsOf(session.Profile(), session.Snapshot()))
        {
            if (toRead.itIsNavigationData)
            {
                continue;
            }

            const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon({scenery.SceneryOf(toRead)});

            if (airports.empty() || airports.front().codes.empty())
            {
                continue;
            }

            const std::size_t holds =
                WhatTheyHold(documentService.DocumentsOf(toRead.addon, toRead.folder, airports.front().codes));

            if (holds > most)
            {
                most = holds;
                best = AddonAt(session.Snapshot().libraries, toRead.folder);
            }
        }

        return best;
    }

    std::uintmax_t BytesIn(const FilesystemProbe& filesystemProbe, const std::filesystem::path& folder)
    {
        std::uintmax_t bytes = 0;

        if (const std::optional<TreeFingerprint> walk = filesystemProbe.FingerprintTree(folder); walk.has_value())
        {
            for (const FileFingerprint& file : walk->files)
            {
                bytes += file.size;
            }
        }

        return bytes;
    }

    const TreeNode* TheLargestAircraft(const std::vector<const TreeNode*>& addons,
                                       const FilesystemProbe& filesystemProbe)
    {
        const TreeNode* largest = nullptr;
        std::uintmax_t most = 0;

        for (const TreeNode* addon : addons)
        {
            if (!AsText(addon->path.parent_path().filename()).contains(QStringLiteral("aircraft"), Qt::CaseInsensitive))
            {
                continue;
            }

            if (const std::uintmax_t bytes = BytesIn(filesystemProbe, addon->path); bytes > most)
            {
                most = bytes;
                largest = addon;
            }
        }

        return largest;
    }

    void Describe(const QString& what, const TreeNode* addon, const FilesystemProbe& filesystemProbe)
    {
        if (addon == nullptr)
        {
            Out() << "  " << what << ": none found, pass --" << (what.startsWith("airport") ? "airport" : "aircraft")
                  << "-addon=<part of the folder name>\n";
            return;
        }

        Out() << "  " << what << ": " << AsText(addon->path) << "  " << BytesIn(filesystemProbe, addon->path) / 1048576
              << " MiB\n";
    }
}

int MeasureTheOpenCosts(MainWindow& window,
                        AddonTreePage& page,
                        AddonTreeModel& model,
                        AddonTreeViewModel& treeViewModel,
                        AddonDocumentsViewModel& documentsViewModel,
                        const ProfileService& profileService,
                        const DocumentService& documentService,
                        const FilesystemProbe& filesystemProbe,
                        SceneryService& scenery,
                        ColdableSceneryCache& sceneryCache,
                        Session& session)
{
    window.showMaximized();
    for (int pass = 0; pass < 40; ++pass)
    {
        QApplication::processEvents();
    }

    auto* view = page.findChild<QTreeView*>();
    if (view == nullptr)
    {
        Out() << "could not find the library tree\n";
        Out().flush();
        return 2;
    }

    view->expandAll();
    for (int pass = 0; pass < 40; ++pass)
    {
        QApplication::processEvents();
    }

    MeasureTheDestinationDivergence(session);
    MeasureTheLoadSection(filesystemProbe, session);

    Out() << "\nthe selection after an enable: the main thread watched until documents and size both landed\n";

    std::vector<const TreeNode*> addons;
    for (const TreeNode& library : session.Snapshot().libraries)
    {
        std::ranges::copy(AddonsUnder(library), std::back_inserter(addons));
    }

    const QString airportName = ArgumentValue(QStringLiteral("--airport-addon"));
    const QString aircraftName = ArgumentValue(QStringLiteral("--aircraft-addon"));

    const TreeNode* airport = airportName.isEmpty() ? TheAirportWithTheMostDocuments(session, scenery, documentService)
                                                    : AddonWhoseFolderHolds(addons, airportName);
    const TreeNode* aircraft = aircraftName.isEmpty() ? TheLargestAircraft(addons, filesystemProbe)
                                                      : AddonWhoseFolderHolds(addons, aircraftName);

    Describe(QStringLiteral("airport addon with documents"), airport, filesystemProbe);
    Describe(QStringLiteral("large aircraft addon"), aircraft, filesystemProbe);

    SelectionBench bench(*view, model, treeViewModel, documentsViewModel, profileService, sceneryCache, session);
    bench.Header();

    for (const auto& [what, addon] :
         {std::pair{QStringLiteral("airport"), airport}, std::pair{QStringLiteral("aircraft"), aircraft}})
    {
        if (addon == nullptr)
        {
            continue;
        }

        bench.Run(*addon, what, false);
        bench.Run(*addon, what, true);
    }

    sceneryCache.Forget(false);

    return 0;
}
