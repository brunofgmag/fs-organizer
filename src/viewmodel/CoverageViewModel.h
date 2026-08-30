#ifndef FS_ORGANIZER_VIEWMODEL_COVERAGE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COVERAGE_VIEW_MODEL_H

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "application/CoverageService.h"
#include "application/ports/BackgroundRunner.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "domain/model/TreeNode.h"
#include "domain/ports/Clock.h"

struct CoverageLine
{
    QString code{};
    QString covered{};
    QString andBy{};
    bool againstTheSimulator = false;
    std::string packageName{};
    AddonId one{};
    AddonId other{};
};

struct SharedAirportsLine
{
    QString turningOn{};
    QString alreadyOn{};
    QStringList codes{};
    AddonId one{};
    AddonId other{};
};

struct TurnedOffLine
{
    QString name{};
    QString code{};
};

struct WhatTurningThemOnFound
{
    std::vector<CoverageLine> alsoCovered{};
    std::vector<SharedAirportsLine> shared{};
};

Q_DECLARE_METATYPE(WhatTurningThemOnFound)

class CoverageViewModel final : public QObject
{
    Q_OBJECT

public:
    CoverageViewModel(CoverageService& service,
                      SceneryService& scenery,
                      Session& session,
                      const Clock& clock,
                      BackgroundRunner& runner,
                      QObject* parent = nullptr);

    void Show();

    void CheckWhatWasTurnedOn(const std::vector<const TreeNode*>& nodes);

    void StopChecking();

    [[nodiscard]] bool Checking() const;

    [[nodiscard]] bool Managing() const;

    void Manage(bool managing);

    [[nodiscard]] const std::vector<CoverageLine>& Conflicts() const;

    [[nodiscard]] const std::vector<TurnedOffLine>& TurnedOff() const;

    [[nodiscard]] std::size_t AddonsWhoseSceneryWasRead() const;

    [[nodiscard]] std::size_t AddonsInTheLibraries() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ReadAt() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] FileResult Switch(const std::string& packageName, bool activated);

    [[nodiscard]] FileResult SwitchAll(const std::vector<std::string>& packageNames, bool activated);

    void TheyCanCoexist(const AddonId& one, const AddonId& other);

    void TheyCanAllCoexist(const std::vector<CoexistingPair>& pairs);

signals:
    void Changed();

    void SettingsCouldNotBeSaved();

    void CheckProgressed(int read, int outOf);

    void TurningThemOnWasChecked(const WhatTurningThemOnFound& found);

private:
    void Read();

    void Check(const std::vector<AddonToRead>& turningOn);

    [[nodiscard]] SceneryProgress TellingHowFarItGot();

    void TheAnswerCameBack(const WhatTurningThemOnFound& found);

    CoverageService& service_;
    SceneryService& scenery_;
    Session& session_;
    const Clock& clock_;
    BackgroundRunner& runner_;
    bool checking_ = false;
    std::atomic<bool> stopChecking_ = false;
    std::vector<AddonToRead> waiting_;
    std::vector<CoverageLine> conflicts_;
    std::vector<TurnedOffLine> turnedOff_;
    std::size_t read_ = 0;
    std::size_t addons_ = 0;
    std::optional<std::chrono::system_clock::time_point> readAt_;
};

#endif // FS_ORGANIZER_VIEWMODEL_COVERAGE_VIEW_MODEL_H
