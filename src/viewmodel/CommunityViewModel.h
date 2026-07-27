#ifndef FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "viewmodel/CommunityModel.h"
#include "viewmodel/SessionNotifier.h"

class CommunityViewModel final : public QObject
{
    Q_OBJECT

public:
    CommunityViewModel(ProfileService& service,
                       Session& session,
                       const SessionNotifier& notifier,
                       CommunityModel& model,
                       QObject* parent = nullptr);

    void Show();

    [[nodiscard]] std::vector<RepairCandidate> PlanRepairs() const;

    void Repair(const std::vector<RepairRequest>& requests);

    [[nodiscard]] std::size_t NeedsAttention() const;

    [[nodiscard]] const ProfileSnapshot& Snapshot() const;

signals:
    void RepairFinished(const std::vector<LinkOperationResult>& results);

    void AttentionChanged(std::size_t count);

private:
    void Refresh();

    ProfileService& service_;
    Session& session_;
    CommunityModel& model_;
    std::size_t attention_ = 0;
};

#endif // FS_ORGANIZER_VIEWMODEL_COMMUNITY_VIEW_MODEL_H
