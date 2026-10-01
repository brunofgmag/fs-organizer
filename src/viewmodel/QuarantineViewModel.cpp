#include "viewmodel/QuarantineViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

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
      caller_(sizes.NewCaller()),
      collisionCaller_(sizes.NewCaller()),
      working_(runner)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this,
            [this]
            {
                ListWhatIsHeld();
            });
}

void QuarantineViewModel::ListWhatIsHeld()
{
    const int mine = ++listed_;
    const SimulatorProfile profile = session_.Profile();
    const auto items = std::make_shared<std::vector<QuarantinedItem>>();

    runner_.Run(
        [this, profile, items]
        {
            *items = service_.Quarantined(profile);
        },
        [this, mine, items]
        {
            if (mine != listed_)
            {
                return;
            }

            model_.ShowItems(*items);

            if (shown_)
            {
                Describe(*items);
                Weigh(*items);
            }
        });
}

void QuarantineViewModel::Show()
{
    shown_ = true;

    ListWhatIsHeld();
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

std::vector<RestoreOffer> QuarantineViewModel::WhatRestoringWouldDo(const std::vector<QuarantinedItem>& items) const
{
    return service_.OffersFor(session_.Profile(), items);
}

void QuarantineViewModel::PrepareRestore(const std::vector<QuarantinedItem>& items)
{
    if (items.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const auto offers = std::make_shared<std::vector<RestoreOffer>>();

    working_.Run(
        [this, profile, items, offers]
        {
            *offers = service_.OffersFor(profile, items);
        },
        [this, offers]
        {
            emit RestoreOffersReady(*offers);
        });
}

void QuarantineViewModel::WeighBothSidesOf(const RestoreCheck& check, std::function<void(const TwoSides&)> onWeighed)
{
    sizes_.MeasureFolders(
        {check.item.path, check.occupant}, collisionCaller_, Freshness::MeasureAgain, {},
        [held = check.item.path, occupant = check.occupant,
         weighed = std::move(onWeighed)](const FolderSizeReport& report)
        {
            weighed(TwoSides{.held = FolderIn(report.folders, held), .occupant = FolderIn(report.folders, occupant)});
        });
}

void QuarantineViewModel::Restore(const std::vector<QuarantinedItem>& items)
{
    Restore(items, {});
}

void QuarantineViewModel::Swap(const std::vector<QuarantinedItem>& items)
{
    Restore({}, items);
}

void QuarantineViewModel::Restore(const std::vector<QuarantinedItem>& going,
                                  const std::vector<QuarantinedItem>& replacing)
{
    if (going.empty() && replacing.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const auto restored = std::make_shared<std::vector<FileOperationResult>>();
    const auto swapped = std::make_shared<std::vector<SwapResult>>();

    working_.Run(
        [this, profile, entries, going, replacing, restored, swapped]
        {
            if (!going.empty())
            {
                *restored = service_.Restore(profile, going);
            }

            swapped->reserve(replacing.size());

            for (const QuarantinedItem& item : replacing)
            {
                swapped->push_back(service_.Swap(profile, entries, item));
            }
        },
        [this, going, replacing, restored, swapped]
        {
            Show();

            const bool anythingCameBack = std::ranges::any_of(*restored,
                                                              [](const FileOperationResult& result)
                                                              {
                                                                  return Succeeded(result.result);
                                                              })
                || std::ranges::any_of(*swapped,
                                       [](const SwapResult& result)
                                       {
                                           return result.Succeeded();
                                       });

            if (anythingCameBack)
            {
                profileService_.ForgetUndo();
            }

            emit CameBack();

            if (!going.empty())
            {
                emit Restored(*restored);
            }

            if (!replacing.empty())
            {
                emit Swapped(*swapped);
            }
        });
}

void QuarantineViewModel::Discard(const std::vector<QuarantinedItem>& items)
{
    if (items.empty())
    {
        return;
    }

    const SimulatorProfile profile = session_.Profile();
    const auto results = std::make_shared<std::vector<FileOperationResult>>();

    working_.Run(
        [this, count = static_cast<int>(items.size())]
        {
            emit DiscardStarted(count);
        },
        [this, profile, items, results]
        {
            *results =
                service_.Discard(profile, items,
                                 [this](const std::size_t discarded, const std::size_t outOf)
                                 {
                                     emit DiscardProgressed(static_cast<int>(discarded), static_cast<int>(outOf));
                                 });
        },
        [this, results]
        {
            Show();

            emit Discarded(*results);
        });
}
