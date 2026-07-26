#ifndef FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ImportService.h"
#include "application/ProfileService.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/QuarantineModel.h"

class QuarantineViewModel final : public QObject
{
    Q_OBJECT

public:
    QuarantineViewModel(const ImportService& service,
                        ProfileService& profileService,
                        const AddonTreeModel& treeModel,
                        QuarantineModel& model,
                        QObject* parent = nullptr);

    void Show();

    void Restore(const std::vector<QuarantinedItem>& items);

    void Discard(const std::vector<QuarantinedItem>& items);

signals:
    void Restored(const std::vector<FileOperationResult>& results);

    void Discarded(const std::vector<FileOperationResult>& results);

private:
    const ImportService& service_;
    ProfileService& profileService_;
    const AddonTreeModel& treeModel_;
    QuarantineModel& model_;
};

#endif // FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
