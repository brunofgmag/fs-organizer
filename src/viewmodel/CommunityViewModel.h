#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "application/model/LinkBatchReport.h"
#include "application/ports/BackgroundRunner.h"
#include "viewmodel/AttentionBreakdown.h"
#include "viewmodel/CommunityModel.h"
#include "viewmodel/GuardedRunner.h"
#include "viewmodel/SelectionSize.h"
#include "viewmodel/SessionNotifier.h"

class CommunityViewModel final : public QObject
{
    Q_OBJECT

public:
    CommunityViewModel(ProfileService& service,
                       Session& session,
                       const SessionNotifier& notifier,
                       CommunityModel& model,
                       SizeService& sizes,
                       BackgroundRunner& runner,
                       QObject* parent = nullptr);

    void Show();

    void ReadTheDestinationsAgain();

    void TheListIsOnScreen(bool onScreen);

    void MeasureTheSelection(const std::vector<DestinationEntry>& entries);

    void WeighTheFolders(const std::vector<std::filesystem::path>& folders,
                         std::function<void(std::uintmax_t bytes)> onWeighed);

    [[nodiscard]] std::vector<RepairCandidate> PlanRepairs() const;

    void Repair(const std::vector<RepairRequest>& requests);

    [[nodiscard]] AttentionBreakdown Breakdown() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

signals:
    void RepairFinished(const LinkBatchReport& report);

    void BreakdownChanged(const AttentionBreakdown& breakdown);

    void SizeMeasuring();

    void SizeMeasured(const SelectionSize& size);

private:
    struct RepairWork
    {
        EntriesStamp stamp{};
        std::vector<TreeNode> libraries{};
        std::vector<RepairRequest> requests{};
        LinkBatchOutcome outcome{};
        bool simulatorRunning = false;
    };

    void Refresh();

    void ApplyTheRepair(RepairWork& work);

    void HandTheEntriesToTheModel();

    void CountTheBreakdown();

    ProfileService& service_;
    Session& session_;
    CommunityModel& model_;
    SizeService& sizes_;
    MeasurementCaller caller_;
    MeasurementCaller foldersCaller_;
    GuardedRunner repairing_;
    AttentionBreakdown breakdown_;
    bool onScreen_ = true;
    bool modelIsStale_ = true;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
