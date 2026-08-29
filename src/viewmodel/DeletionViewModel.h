#ifndef FS_ORGANIZER_VIEWMODEL_DELETION_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DELETION_VIEW_MODEL_H

#include <filesystem>
#include <vector>

#include <QtCore/QMetaType>
#include <QtCore/QObject>

#include "application/DeletionService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "application/model/DeletionPlan.h"
#include "application/ports/BackgroundRunner.h"
#include "domain/model/TreeNode.h"

Q_DECLARE_METATYPE(DeletionPlan)
Q_DECLARE_METATYPE(DeletionRoute)
Q_DECLARE_METATYPE(std::vector<DeletionResult>)

class DeletionViewModel final : public QObject
{
    Q_OBJECT

public:
    DeletionViewModel(Session& session,
                      ProfileService& profileService,
                      const DeletionService& service,
                      SizeService& sizes,
                      BackgroundRunner& runner,
                      QObject* parent = nullptr);

    void PlanToDelete(const std::vector<const TreeNode*>& nodes);

    void Delete(const DeletionPlan& plan, DeletionRoute route);

    [[nodiscard]] QString LabelOfProfile(const std::string& profileId) const;

signals:
    void Weighing();

    void Planned(const DeletionPlan& plan);

    void Deleting();

    void Deleted(const std::vector<DeletionResult>& results, DeletionRoute route);

private:
    [[nodiscard]] std::vector<SimulatorProfile> EveryProfile() const;

    [[nodiscard]] std::vector<const TreeNode*> NodesStillThere(const std::vector<std::filesystem::path>& chosen) const;

    Session& session_;
    ProfileService& profileService_;
    const DeletionService& service_;
    SizeService& sizes_;
    BackgroundRunner& runner_;
    MeasurementCaller caller_;
    bool deleting_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_DELETION_VIEW_MODEL_H
