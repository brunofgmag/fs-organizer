#include "viewmodel/QuarantineViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "domain/support/PathUtils.h"

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
            });
}

std::vector<QuarantinedItem> QuarantineViewModel::ListWhatIsHeld()
{
    std::vector<QuarantinedItem> items = service_.Quarantined(session_.Profile());

    model_.ShowItems(items);

    return items;
}

void QuarantineViewModel::Show()
{
    shown_ = true;

    const std::vector<QuarantinedItem> items = ListWhatIsHeld();

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

std::vector<RestoreOffer> QuarantineViewModel::WhatRestoringWouldDo(const std::vector<QuarantinedItem>& items) const
{
    std::vector<RestoreCheck> checks = service_.CheckRestore(session_.Profile(), items);

    std::vector<RestoreOffer> offers;
    offers.reserve(checks.size());

    for (RestoreCheck& check : checks)
    {
        std::vector<RestorePlace> places =
            check.NeedsAPlace() ? service_.PlacesFor(session_.Profile(), check.item) : std::vector<RestorePlace>{};

        offers.push_back(RestoreOffer{.check = std::move(check), .places = std::move(places)});
    }

    return offers;
}

void QuarantineViewModel::PrepareRestore(const std::vector<QuarantinedItem>& items)
{
    if (working_ || items.empty())
    {
        return;
    }

    working_ = true;

    const SimulatorProfile profile = session_.Profile();
    const auto offers = std::make_shared<std::vector<RestoreOffer>>();

    runner_.Run(
        [this, profile, items, offers]
        {
            std::vector<RestoreCheck> checks = service_.CheckRestore(profile, items);

            offers->reserve(checks.size());

            for (RestoreCheck& check : checks)
            {
                std::vector<RestorePlace> places =
                    check.NeedsAPlace() ? service_.PlacesFor(profile, check.item) : std::vector<RestorePlace>{};

                offers->push_back(RestoreOffer{.check = std::move(check), .places = std::move(places)});
            }
        },
        [this, offers]
        {
            working_ = false;

            emit RestoreOffersReady(*offers);
        });
}

void QuarantineViewModel::WeighBothSidesOf(const RestoreCheck& check, std::function<void(const TwoSides&)> onWeighed)
{
    sizes_.MeasureFolders(
        {check.item.path, check.occupant}, caller_, Freshness::MeasureAgain, {},
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
    if (working_ || (going.empty() && replacing.empty()))
    {
        return;
    }

    working_ = true;

    const SimulatorProfile profile = session_.Profile();
    const std::vector<DestinationEntry> entries = session_.Snapshot().entries;
    const auto restored = std::make_shared<std::vector<FileOperationResult>>();
    const auto swapped = std::make_shared<std::vector<SwapResult>>();

    runner_.Run(
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
            working_ = false;

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
    if (working_ || items.empty())
    {
        return;
    }

    working_ = true;

    const SimulatorProfile profile = session_.Profile();
    const auto results = std::make_shared<std::vector<FileOperationResult>>();

    emit DiscardStarted(static_cast<int>(items.size()));

    runner_.Run(
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
            working_ = false;

            Show();

            emit Discarded(*results);
        });
}
