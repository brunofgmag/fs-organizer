#ifndef FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ImportService.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/SizeService.h"
#include "application/ports/BackgroundRunner.h"
#include "viewmodel/QuarantineModel.h"
#include "viewmodel/SessionNotifier.h"

class QuarantineViewModel final : public QObject
{
    Q_OBJECT

public:
    QuarantineViewModel(const ImportService& service,
                        ProfileService& profileService,
                        const Session& session,
                        const SessionNotifier& notifier,
                        QuarantineModel& model,
                        SizeService& sizes,
                        BackgroundRunner& runner,
                        QObject* parent = nullptr);

    void Show();

    void Restore(const std::vector<QuarantinedItem>& items);

    void Discard(const std::vector<QuarantinedItem>& items);

signals:
    void Restored(const std::vector<FileOperationResult>& results);

    void Discarded(const std::vector<FileOperationResult>& results);

private:
    void Describe(const std::vector<QuarantinedItem>& items);

    void Weigh(const std::vector<QuarantinedItem>& items);

    const ImportService& service_;
    ProfileService& profileService_;
    const Session& session_;
    QuarantineModel& model_;
    SizeService& sizes_;
    BackgroundRunner& runner_;
    MeasurementCaller caller_;
    int listed_ = 0;
    bool shown_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
