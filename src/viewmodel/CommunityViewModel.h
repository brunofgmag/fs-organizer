#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "viewmodel/AttentionBreakdown.h"
#include "viewmodel/CommunityModel.h"
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
                       QObject* parent = nullptr);

    void Show();

    void MeasureTheSelection(const std::vector<DestinationEntry>& entries);

    [[nodiscard]] std::vector<RepairCandidate> PlanRepairs() const;

    void Repair(const std::vector<RepairRequest>& requests);

    [[nodiscard]] AttentionBreakdown Breakdown() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

signals:
    void RepairFinished(const std::vector<LinkOperationResult>& results);

    void BreakdownChanged(const AttentionBreakdown& breakdown);

    void SizeMeasuring();

    void SizeMeasured(const SelectionSize& size);

private:
    void Refresh();

    ProfileService& service_;
    Session& session_;
    CommunityModel& model_;
    SizeService& sizes_;
    MeasurementCaller caller_;
    AttentionBreakdown breakdown_;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
