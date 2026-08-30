#ifndef FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/BisectionService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"

enum class BisectionStage : int
{
    NotStarted = 0,
    Asking = 1,
    ItDrifted = 2,
    Finished = 3,
    TheLibraryGainedAnAddon = 4,
};

struct MemberOnScreen
{
    QString name{};
    std::size_t writesWith = 0;
};

struct UnitOnScreen
{
    QString name{};
    std::size_t addons = 0;
    Coupling coupling = Coupling::NotYetMeasured;
    std::vector<MemberOnScreen> members{};
};

class BisectionViewModel final : public QObject
{
    Q_OBJECT

public:
    BisectionViewModel(BisectionService& bisection,
                       Session& session,
                       BackgroundRunner& runner,
                       QObject* parent = nullptr);

    void Show();

    void Begin();

    void Answer(BisectionAnswer answer);

    void Refine();

    void CarryOn();

    void Stop();

    void Resume(ResumeChoice choice);

    [[nodiscard]] BisectionStage Stage() const;

    [[nodiscard]] const BisectionReport& Report() const;

    [[nodiscard]] bool AProcedureWasInterrupted() const;

    [[nodiscard]] bool ItIsRunning() const;

    [[nodiscard]] bool ReadingWhatIsOn() const;

    [[nodiscard]] std::size_t RoundsLeftInTheWorstCase() const;

    [[nodiscard]] std::size_t LaunchesAlreadyMade() const;

    [[nodiscard]] std::vector<UnitOnScreen> WhatIsLeft() const;

    [[nodiscard]] std::vector<UnitOnScreen> WhatToTurnOn() const;

signals:
    void Changed();

private:
    void Take(const BisectionReport& report);

    void RunTheProcedure(std::function<BisectionReport()> work);

    [[nodiscard]] BisectionReport EndedReport(BisectionReport ended, const SimulatorProfile& profile) const;

    BisectionService& bisection_;
    Session& session_;
    BackgroundRunner& runner_;
    BisectionReport report_{};
    BisectionStage stage_ = BisectionStage::NotStarted;
    std::optional<BisectionAnswer> heldAnswer_{};
    std::optional<std::vector<std::filesystem::path>> readFor_{};
    bool aSplitWasHeld_ = false;
    bool reading_ = false;
    bool mutating_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H
