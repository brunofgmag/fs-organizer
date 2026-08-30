#ifndef FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H

#include <functional>
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

    [[nodiscard]] std::vector<RestoreOffer> WhatRestoringWouldDo(const std::vector<QuarantinedItem>& items) const;

    void PrepareRestore(const std::vector<QuarantinedItem>& items);

    void WeighBothSidesOf(const RestoreCheck& check, std::function<void(const TwoSides&)> onWeighed);

    void Restore(const std::vector<QuarantinedItem>& items);

    void Restore(const std::vector<QuarantinedItem>& going, const std::vector<QuarantinedItem>& replacing);

    void Swap(const std::vector<QuarantinedItem>& items);

    void Discard(const std::vector<QuarantinedItem>& items);

signals:
    void RestoreOffersReady(const std::vector<RestoreOffer>& offers);

    void Restored(const std::vector<FileOperationResult>& results);

    void DiscardStarted(int items);

    void DiscardProgressed(int discarded, int outOf);

    void Swapped(const std::vector<SwapResult>& results);

    void Discarded(const std::vector<FileOperationResult>& results);

private:
    [[nodiscard]] std::vector<QuarantinedItem> ListWhatIsHeld();

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
    bool working_ = false;
    bool shown_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_QUARANTINE_VIEW_MODEL_H
