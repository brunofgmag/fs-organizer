#include "viewmodel/QuarantineViewModel.h"

#include <algorithm>
#include <memory>

QuarantineViewModel::QuarantineViewModel(const ImportService& service,
                                         ProfileService& profileService,
                                         const Session& session,
                                         const SessionNotifier& notifier,
                                         QuarantineModel& model,
                                         SizeService& sizes,
                                         BackgroundRunner& runner,
                                         QObject* parent)
    : QObject(parent),
      service_(service),
      profileService_(profileService),
      session_(session),
      model_(model),
      sizes_(sizes),
      runner_(runner),
      caller_(sizes.NewCaller())
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

    const std::vector<QuarantinedItem> items = service_.Quarantined(session_.Profile());

    model_.ShowItems(items);

    Describe(items);
    Weigh(items);
}

void QuarantineViewModel::Describe(const std::vector<QuarantinedItem>& items)
{
    if (items.empty())
    {
        return;
    }

    const int mine = ++listed_;
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const auto described = std::make_shared<std::vector<QuarantineDetail>>();

    runner_.Run(
        [this, entries, items, described]
        {
            *described = service_.Describe(entries, items);
        },
        [this, mine, described]
        {
            if (mine != listed_)
            {
                return;
            }

            model_.ShowDetails(*described);
        });
}

void QuarantineViewModel::Weigh(const std::vector<QuarantinedItem>& items)
{
    if (items.empty())
    {
        return;
    }

    std::vector<std::filesystem::path> folders;
    folders.reserve(items.size());

    for (const QuarantinedItem& item : items)
    {
        folders.push_back(item.path);
    }

    sizes_.MeasureFolders(folders, caller_, Freshness::ReuseWhatIsKnown, {},
                          [this](const FolderSizeReport& report)
                          {
                              model_.ShowSizes(report.folders);
                          });
}

void QuarantineViewModel::Restore(const std::vector<QuarantinedItem>& items)
{
    const std::vector<FileOperationResult> results = service_.Restore(session_.Profile(), items);

    Show();

    if (std::ranges::any_of(results,
                            [](const FileOperationResult& result)
                            {
                                return Succeeded(result.result);
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
