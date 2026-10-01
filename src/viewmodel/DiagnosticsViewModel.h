#ifndef FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/ImportService.h"
#include "application/LoadReport.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/LoadingReportSource.h"
#include "domain/ports/Clock.h"
#include "viewmodel/SessionNotifier.h"

struct ClassificationCount
{
    EntryClassification classification = EntryClassification::Managed;
    std::size_t count = 0;
};

struct QuarantineWeight
{
    std::uintmax_t bytes = 0;
    std::size_t besideDestinations = 0;
    std::size_t insideLibraries = 0;
};

struct SceneryCensus
{
    std::vector<QString> carryingACode{};
    std::vector<QString> whoseRecordWasNotRead{};
    std::vector<QString> carryingNavigationData{};
    std::size_t carryingNoAirportRecord = 0;
    std::size_t addons = 0;
};

struct LibrarySet
{
    std::string profileId{};
    std::vector<std::string> roots{};

    [[nodiscard]] bool operator==(const LibrarySet& other) const = default;
};

class DiagnosticsViewModel final : public QObject
{
    Q_OBJECT

public:
    DiagnosticsViewModel(const ImportService& imports,
                         SizeService& sizes,
                         SceneryService& scenery,
                         Session& session,
                         const SessionNotifier& notifier,
                         const LoadingReportSource& loading,
                         const Clock& clock,
                         BackgroundRunner& runner,
                         QObject* parent = nullptr);

    void Show();

    void ShowSize();

    void MeasureSizeAgain();

    void CancelSize();

    void ShowTheLoad();

    void ShowScenery();

    void ReadTheSceneryAgain();

    void CancelScenery();

    [[nodiscard]] const LoadDiagnostics& Load() const;

    [[nodiscard]] const SceneryCensus& Scenery() const;

    [[nodiscard]] bool ReadingTheScenery() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> SceneryReadAt() const;

    [[nodiscard]] const std::vector<ClassificationCount>& Counts() const;

    [[nodiscard]] const std::vector<DestinationEntry>& Broken() const;

    [[nodiscard]] const std::vector<DestinationEntry>& Unavailable() const;

    [[nodiscard]] const QuarantineWeight& Quarantine() const;

    [[nodiscard]] const SizeReport& Size() const;

    [[nodiscard]] bool Measuring() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> CountedAt() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> MeasuredAt() const;

signals:
    void Counted();

    void SizeProgressed(const QString& folder, int measured, int total);

    void SizeMeasured();

    void LoadRead();

    void SceneryProgressed(int read, int total);

    void SceneryRead();

    void TheLibrariesChanged();

private:
    using StopToken = std::shared_ptr<std::atomic<bool>>;

    void FollowTheLibraries();

    void ForgetWhatBelongedToTheOldLibraries();

    void Count();

    void WeighTheQuarantine();

    void LandTheQuarantine(const SimulatorProfile& profile, const std::vector<QuarantinedItem>& items);

    void Ask(Freshness freshness);

    void Walk(const std::vector<AddonToRead>& addons, SceneryFreshness freshness);

    const ImportService& imports_;
    SizeService& sizes_;
    SceneryService& scenery_;
    Session& session_;
    const LoadingReportSource& loading_;
    const Clock& clock_;
    BackgroundRunner& runner_;
    MeasurementCaller caller_;
    MeasurementCaller quarantineCaller_;
    std::vector<ClassificationCount> counts_;
    std::vector<DestinationEntry> broken_;
    std::vector<DestinationEntry> unavailable_;
    QuarantineWeight quarantine_;
    SizeReport size_;
    LoadDiagnostics load_{};
    SceneryCensus census_;
    std::optional<std::chrono::system_clock::time_point> countedAt_;
    std::optional<std::chrono::system_clock::time_point> measuredAt_;
    std::optional<std::chrono::system_clock::time_point> sceneryReadAt_;
    LibrarySet libraries_;
    bool measuring_ = false;
    bool reading_ = false;
    int weighing_ = 0;
    StopToken sizeStop_ = std::make_shared<std::atomic<bool>>(false);
    StopToken sceneryStop_ = std::make_shared<std::atomic<bool>>(false);
};

#endif // FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H
