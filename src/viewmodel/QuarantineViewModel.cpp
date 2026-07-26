#include "viewmodel/QuarantineViewModel.h"

#include <algorithm>

QuarantineViewModel::QuarantineViewModel(const ImportService& service,
                                         ProfileService& profileService,
                                         const AddonTreeModel& treeModel,
                                         QuarantineModel& model,
                                         QObject* parent)
    : QObject(parent), service_(service), profileService_(profileService), treeModel_(treeModel),
      model_(model)
{
}

void QuarantineViewModel::Show()
{
    model_.ShowItems(service_.Quarantined(treeModel_.Profile()));
}

void QuarantineViewModel::Restore(const std::vector<QuarantinedItem>& items)
{
    const std::vector<FileOperationResult> results = service_.Restore(treeModel_.Profile(), items);

    Show();

    if (std::ranges::any_of(results, [](const FileOperationResult& result)
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
    const std::vector<FileOperationResult> results = service_.Discard(treeModel_.Profile(), items);

    Show();

    emit Discarded(results);
}
