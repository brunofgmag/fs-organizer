#ifndef FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H

#include <cstddef>
#include <optional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/BisectionService.h"
#include "application/Session.h"

enum class BisectionStage : int
{
    NotStarted = 0,
    Asking = 1,
    ItDrifted = 2,
    Finished = 3,
};

struct UnitOnScreen
{
    QString name{};
    std::size_t addons = 0;
    Coupling coupling = Coupling::NotYetMeasured;
};

class BisectionViewModel final : public QObject
{
    Q_OBJECT

public:
    BisectionViewModel(BisectionService& bisection, Session& session, QObject* parent = nullptr);

    void Show();

    void Begin();

    void Answer(BisectionAnswer answer);

    void Refine();

    void Stop();

    void Resume(ResumeChoice choice);

    [[nodiscard]] BisectionStage Stage() const;

    [[nodiscard]] const BisectionReport& Report() const;

    [[nodiscard]] bool AProcedureWasInterrupted() const;

    [[nodiscard]] bool ItIsRunning() const;

    [[nodiscard]] std::size_t RoundsLeftInTheWorstCase() const;

    [[nodiscard]] std::vector<UnitOnScreen> WhatIsLeft() const;

    [[nodiscard]] std::vector<UnitOnScreen> WhatToTurnOn() const;

signals:
    void Changed();

private:
    void Take(const BisectionReport& report);

    void TakeTheEndOf(const BisectionReport& ended);

    BisectionService& bisection_;
    Session& session_;
    BisectionReport report_{};
    BisectionStage stage_ = BisectionStage::NotStarted;
};

#endif // FS_ORGANIZER_VIEWMODEL_BISECTION_VIEW_MODEL_H
