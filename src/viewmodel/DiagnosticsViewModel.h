#ifndef FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
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

class DiagnosticsViewModel final : public QObject
{
    Q_OBJECT

public:
    DiagnosticsViewModel(const ImportService& imports,
                         SizeService& sizes,
                         SceneryService& scenery,
                         Session& session,
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

private:
    void Count();

    void WeighTheQuarantine();

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
    bool measuring_ = false;
    bool reading_ = false;
    std::atomic<bool> cancelling_ = false;
    std::atomic<bool> stopReading_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_DIAGNOSTICS_VIEW_MODEL_H
