#include "viewmodel/QuarantineViewModel.h"

#include <algorithm>

QuarantineViewModel::QuarantineViewModel(const ImportService& service,
                                         ProfileService& profileService,
                                         const Session& session,
                                         const SessionNotifier& notifier,
                                         QuarantineModel& model,
                                         QObject* parent)
    : QObject(parent), service_(service), profileService_(profileService), session_(session), model_(model)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this,
            [this]
            {
                if (shown_)
                {
                    Show();
                }
            });
}

void QuarantineViewModel::Show()
{
    shown_ = true;

    model_.ShowItems(service_.Quarantined(session_.Profile()));
}

void QuarantineViewModel::Restore(const std::vector<QuarantinedItem>& items)
{
    const std::vector<FileOperationResult> results = service_.Restore(session_.Profile(), items);

    Show();

    if (std::ranges::any_of(results,
                            [](const FileOperationResult& result)
                            {
                                return result.result == ImportResult::Completed;
                            }))
    {
        profileService_.ForgetUndo();
    }

    emit Restored(results);
}

void QuarantineViewModel::Discard(const std::vector<QuarantinedItem>& items)
{
    const std::vector<FileOperationResult> results = service_.Discard(session_.Profile(), items);

    Show();

    emit Discarded(results);
}
