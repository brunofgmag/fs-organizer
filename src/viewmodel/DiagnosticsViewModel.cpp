#include "viewmodel/DiagnosticsViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "domain/importing/ImportPaths.h"
#include "domain/support/PathUtils.h"
#include "support/PathText.h"

namespace
{
    std::vector<std::string> QuarantineFoldersBeside(const SimulatorProfile& profile)
    {
        std::vector<std::string> folders;
        folders.reserve(profile.destinations.size());

        for (const std::filesystem::path& destination : profile.destinations)
        {
            folders.push_back(ComparablePath(QuarantineFolderBeside(destination)));
        }

        return folders;
    }

    std::vector<std::string> QuarantineFoldersInside(const SimulatorProfile& profile)
    {
        std::vector<std::string> folders;
        folders.reserve(profile.libraries.size());

        for (const Library& library : profile.libraries)
        {
            folders.push_back(ComparablePath(QuarantineFolderInside(library.path)));
        }

        return folders;
    }

    SceneryCensus CensusOf(const std::vector<SceneryOfAnAddon>& walked, const std::size_t addons)
    {
        SceneryCensus census{.carryingACode = {},
                             .whoseRecordWasNotRead = {},
                             .carryingNavigationData = {},
                             .carryingNoAirportRecord = 0,
                             .addons = addons};

        for (const AirportsOfAnAddon& addon : AirportsOfEachAddon(walked))
        {
            switch (addon.evidence)
            {
            case AirportEvidence::TheCodeWasRead:
                census.carryingACode.push_back(QString::fromStdString(addon.addon.folderName));
                break;
            case AirportEvidence::ARecordWasNotRead:
                census.whoseRecordWasNotRead.push_back(QString::fromStdString(addon.addon.folderName));
                break;
            case AirportEvidence::ItIsNavigationData:
                census.carryingNavigationData.push_back(QString::fromStdString(addon.addon.folderName));
                break;
            case AirportEvidence::ItCarriesNoAirportRecord: ++census.carryingNoAirportRecord; break;
            }
        }

        return census;
    }
}

DiagnosticsViewModel::DiagnosticsViewModel(const ImportService& imports,
                                           SizeService& sizes,
                                           SceneryService& scenery,
                                           Session& session,
                                           const LoadingReportSource& loading,
                                           const Clock& clock,
                                           BackgroundRunner& runner,
                                           QObject* parent)
    : QObject(parent),
      imports_(imports),
      sizes_(sizes),
      scenery_(scenery),
      session_(session),
      loading_(loading),
      clock_(clock),
      runner_(runner),
      caller_(sizes.NewCaller())
{
}

void DiagnosticsViewModel::Show()
{
    session_.RefreshEntries();
    Count();
}

void DiagnosticsViewModel::ShowSize()
{
    if (measuredAt_.has_value() || measuring_)
    {
        return;
    }

    Ask(Freshness::ReuseWhatIsKnown);
}

void DiagnosticsViewModel::MeasureSizeAgain()
{
    Ask(Freshness::MeasureAgain);
}

void DiagnosticsViewModel::CancelSize()
{
    cancelling_ = true;
}

void DiagnosticsViewModel::ShowTheLoad()
{
    load_ = ReportTheLoad(loading_.LastReport(), session_.Snapshot());

    emit LoadRead();
}

void DiagnosticsViewModel::ShowScenery()
{
    if (sceneryReadAt_.has_value() || reading_)
    {
        return;
    }

    Walk(SceneryService::AddonsOf(session_.Profile(), session_.Snapshot()), SceneryFreshness::ReuseWhatIsKnown);
}

void DiagnosticsViewModel::ReadTheSceneryAgain()
{
    if (reading_)
    {
        return;
    }

    Walk(SceneryService::AddonsOf(session_.Profile(), session_.Snapshot()), SceneryFreshness::ReadAgain);
}

void DiagnosticsViewModel::CancelScenery()
{
    stopReading_ = true;
}

const LoadDiagnostics& DiagnosticsViewModel::Load() const
{
    return load_;
}

const SceneryCensus& DiagnosticsViewModel::Scenery() const
{
    return census_;
}

bool DiagnosticsViewModel::ReadingTheScenery() const
{
    return reading_;
}

std::optional<std::chrono::system_clock::time_point> DiagnosticsViewModel::SceneryReadAt() const
{
    return sceneryReadAt_;
}

void DiagnosticsViewModel::Walk(const std::vector<AddonToRead>& addons, const SceneryFreshness freshness)
{
    reading_ = true;
    stopReading_ = false;

    auto walked = std::make_shared<std::vector<SceneryOfAnAddon>>();

    runner_.Run(
        [this, addons, walked, freshness]
        {
            *walked = scenery_.SceneryOfEach(
                addons,
                [this](const std::size_t read, const std::size_t outOf)
                {
                    QMetaObject::invokeMethod(this,
                                              [this, read, outOf]
                                              {
                                                  emit SceneryProgressed(static_cast<int>(read),
                                                                         static_cast<int>(outOf));
                                              });

                    return !stopReading_;
                },
                freshness);
        },
        [this, addons, walked]
        {
            census_ = CensusOf(*walked, addons.size());
            reading_ = false;
            sceneryReadAt_ = clock_.Now();

            emit SceneryRead();
        });
}

const std::vector<ClassificationCount>& DiagnosticsViewModel::Counts() const
{
    return counts_;
}

const std::vector<DestinationEntry>& DiagnosticsViewModel::Broken() const
{
    return broken_;
}

const std::vector<DestinationEntry>& DiagnosticsViewModel::Unavailable() const
{
    return unavailable_;
}

const QuarantineWeight& DiagnosticsViewModel::Quarantine() const
{
    return quarantine_;
}

const SizeReport& DiagnosticsViewModel::Size() const
{
    return size_;
}

bool DiagnosticsViewModel::Measuring() const
{
    return measuring_;
}

std::optional<std::chrono::system_clock::time_point> DiagnosticsViewModel::CountedAt() const
{
    return countedAt_;
}

std::optional<std::chrono::system_clock::time_point> DiagnosticsViewModel::MeasuredAt() const
{
    return measuredAt_;
}

void DiagnosticsViewModel::Count()
{
    const std::vector<DestinationEntry>& entries = session_.Snapshot().entries;

    counts_.clear();
    for (const EntryClassification classification : kEveryClassification)
    {
        counts_.push_back(ClassificationCount{.classification = classification, .count = 0});
    }

    broken_.clear();
    unavailable_.clear();

    for (const DestinationEntry& entry : entries)
    {
        ++counts_[OrderOf(entry.classification)].count;

        if (entry.classification == EntryClassification::Broken)
        {
            broken_.push_back(entry);
        }

        if (entry.classification == EntryClassification::Unavailable)
        {
            unavailable_.push_back(entry);
        }
    }

    WeighTheQuarantine();

    countedAt_ = clock_.Now();

    emit Counted();
}

void DiagnosticsViewModel::WeighTheQuarantine()
{
    const SimulatorProfile& profile = session_.Profile();
    const std::vector<QuarantinedItem> items = imports_.Quarantined(profile);
    const std::vector<std::string> beside = QuarantineFoldersBeside(profile);
    const std::vector<std::string> inside = QuarantineFoldersInside(profile);

    std::vector<std::filesystem::path> folders;
    folders.reserve(items.size());

    quarantine_ = QuarantineWeight{};

    for (const QuarantinedItem& item : items)
    {
        folders.push_back(item.path);

        const std::string holder = ComparablePath(item.path.parent_path());
        if (std::ranges::find(beside, holder) != beside.end())
        {
            ++quarantine_.besideDestinations;
        }

        if (std::ranges::find(inside, holder) != inside.end())
        {
            ++quarantine_.insideLibraries;
        }
    }

    if (folders.empty())
    {
        return;
    }

    sizes_.MeasureFolders(folders, caller_, Freshness::ReuseWhatIsKnown, {},
                          [this](const FolderSizeReport& report)
                          {
                              quarantine_.bytes = report.bytes;

                              emit Counted();
                          });
}

void DiagnosticsViewModel::Ask(const Freshness freshness)
{
    std::vector<std::filesystem::path> roots;
    roots.reserve(session_.Profile().libraries.size());

    for (const Library& library : session_.Profile().libraries)
    {
        roots.push_back(library.path);
    }

    measuring_ = true;
    cancelling_ = false;

    sizes_.Measure(
        roots, caller_, freshness,
        [this](const SizeProgress& progress)
        {
            QMetaObject::invokeMethod(
                this,
                [this, folder = AsText(progress.folder), measured = progress.measured, total = progress.total]
                {
                    emit SizeProgressed(folder, static_cast<int>(measured), static_cast<int>(total));
                });

            return !cancelling_;
        },
        [this](const SizeReport& report)
        {
            size_ = report;
            measuredAt_ = report.measuredAt;
            measuring_ = false;

            emit SizeMeasured();
        });
}
