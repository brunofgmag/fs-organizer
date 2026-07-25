#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/CommunityModel.h"

class CommunityViewModel final : public QObject
{
    Q_OBJECT

public:
    CommunityViewModel(ProfileService& service,
                       AddonTreeModel& treeModel,
                       CommunityModel& model,
                       QObject* parent = nullptr);

    void Show();

    [[nodiscard]] std::vector<RepairCandidate> PlanRepairs() const;

    void Repair(const std::vector<RepairRequest>& requests);

    [[nodiscard]] std::size_t NeedsAttention() const;

signals:
    void RepairFinished(const std::vector<LinkOperationResult>& results);

    void AttentionChanged(std::size_t count);

private:
    void Refresh();

    ProfileService& service_;
    AddonTreeModel& treeModel_;
    CommunityModel& model_;
    std::size_t attention_ = 0;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
