#include "viewmodel/AddonTreeViewModel.h"

#include <algorithm>
#include <set>
#include <string>

#include <QtCore/QStringList>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/DestinationDivergence.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/ToggleDirection.h"
#include "viewmodel/FailureText.h"

namespace
{
    std::filesystem::path CategoryHolding(const TreeNode& node)
    {
        return node.kind == TreeNodeKind::Addon ? node.path.parent_path() : node.path;
    }

    bool WasAgreedTo(const std::vector<TakenPlace>& agreed, const TakenPlace& swap)
    {
        return std::ranges::any_of(agreed,
                                   [&swap](const TakenPlace& candidate)
                                   {
                                       return ComparablePath(candidate.addonFolder) == ComparablePath(swap.addonFolder)
                                           && ComparablePath(candidate.occupant) == ComparablePath(swap.occupant);
                                   });
    }

    std::vector<const TreeNode*> AddonsToEnable(const std::vector<const TreeNode*>& nodes,
                                                const std::set<std::string>& heldBack)
    {
        std::vector<const TreeNode*> wanted;

        for (const TreeNode* node : nodes)
        {
            for (const TreeNode* addon : AddonsUnder(*node))
            {
                if (!heldBack.contains(ComparablePath(addon->path)))
                {
                    wanted.push_back(addon);
                }
            }
        }

        return wanted;
    }
}

AddonTreeViewModel::AddonTreeViewModel(Session& session,
                                       ProfileService& service,
                                       AddonTreeModel& model,
                                       const SimulatorPackages& packages,
                                       SizeService& sizes,
                                       BackgroundRunner& runner,
                                       const SessionNotifier& notifier,
                                       QObject* parent)
    : QObject(parent),
      session_(session),
      service_(service),
      model_(model),
      packages_(packages),
      sizes_(sizes),
      runner_(runner),
      caller_(sizes.NewCaller())
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &AddonTreeViewModel::AdoptScan);

    connect(&notifier, &SessionNotifier::Refreshed, this,
            [this]
            {
                model_.Refresh(session_.Snapshot(), session_.Profile());
            });
}

void AddonTreeViewModel::MeasureTheSelection(const std::vector<std::filesystem::path>& addonFolders)
{
    if (addonFolders.empty())
    {
        emit SizeMeasured(SelectionSize{});
        return;
    }

    emit SizeMeasuring();

    sizes_.MeasureFolders(addonFolders, caller_, Freshness::ReuseWhatIsKnown, {},
                          [this](const FolderSizeReport& report)
                          {
                              emit SizeMeasured(SelectionSize{.bytes = report.bytes,
                                                              .measured = report.measured,
                                                              .selected = report.folders.size()});
                          });
}

void AddonTreeViewModel::WeighTheSwaps(const std::vector<TakenPlace>& swaps,
                                       std::function<void(const std::vector<WeighedSwap>&)> onWeighed)
{
    std::vector<std::filesystem::path> folders;
    folders.reserve(swaps.size() * 2);

    for (const TakenPlace& swap : swaps)
    {
        folders.push_back(swap.occupant);
        folders.push_back(swap.addonFolder);
    }

    sizes_.MeasureFolders(folders, caller_, Freshness::ReuseWhatIsKnown, {},
                          [swaps, weighed = std::move(onWeighed)](const FolderSizeReport& report)
                          {
                              std::vector<WeighedSwap> sides;
                              sides.reserve(swaps.size());

                              for (const TakenPlace& swap : swaps)
                              {
                                  sides.push_back(WeighedSwap{.goesOff = FolderIn(report.folders, swap.occupant),
                                                              .goesOn = FolderIn(report.folders, swap.addonFolder)});
                              }

                              weighed(sides);
                          });
}

void AddonTreeViewModel::ShowActiveProfile() const
{
    session_.ShowActiveProfile();
}

void AddonTreeViewModel::ChooseProfile(const std::string& profileId) const
{
    session_.ChooseProfile(profileId);
}

void AddonTreeViewModel::CancelScan() const
{
    session_.CancelScan();
}

void AddonTreeViewModel::AdoptScan()
{
    model_.Show(session_.Snapshot(), session_.Profile());

    emit Shown();
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes)
{
    Toggle(nodes, WouldEnable(nodes));
}

bool AddonTreeViewModel::WouldEnable(const std::vector<const TreeNode*>& nodes) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();

    return ShouldEnable(session_.Profile(), snapshot.entries, snapshot.enabled, nodes);
}

std::size_t AddonTreeViewModel::AddonsThatWouldChange(const std::vector<const TreeNode*>& nodes,
                                                      const bool enable) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    std::set<std::string> counted;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (snapshot.enabled.Contains(addon->path) != enable)
            {
                counted.insert(ComparablePath(addon->path));
            }
        }
    }

    return counted.size();
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    Toggle(nodes, enable, {});
}

std::vector<TakenPlace> AddonTreeViewModel::SwapsNeededTo(const std::vector<const TreeNode*>& nodes) const
{
    const std::vector<TreeNode>& libraries = session_.Snapshot().libraries;

    std::vector<TakenPlace> swaps;

    for (const TakenPlace& taken : service_.PlacesTaken(session_.Profile(), nodes))
    {
        if (AddonAt(libraries, taken.occupant) != nullptr)
        {
            swaps.push_back(taken);
        }
    }

    return swaps;
}

QString AddonTreeViewModel::VersionOf(const std::filesystem::path& addonFolder) const
{
    const TreeNode* addon = AddonAt(session_.Snapshot().libraries, addonFolder);

    if (addon == nullptr || !addon->addon.has_value())
    {
        return {};
    }

    return QString::fromStdString(addon->addon->manifest.packageVersion);
}

std::vector<StartupLine> AddonTreeViewModel::StartupEntriesAtRisk(const std::vector<const TreeNode*>& nodes) const
{
    return service_.StartupEntriesCarriedBy(session_.Profile(), session_.Snapshot(), nodes);
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes,
                                const bool enable,
                                const std::vector<TakenPlace>& agreedSwaps)
{
    Toggle(nodes, enable, agreedSwaps, {});
}

TogglePlan AddonTreeViewModel::PlanToggle(const std::vector<const TreeNode*>& nodes, const bool enable) const
{
    TogglePlan plan;
    plan.onDisk = service_.ReadLinksNow(session_.Profile());

    if (!enable)
    {
        return plan;
    }

    const std::vector<TreeNode>& libraries = session_.Snapshot().libraries;

    for (const TakenPlace& taken : service_.PlacesTaken(session_.Profile(), nodes, plan.onDisk))
    {
        if (AddonAt(libraries, taken.occupant) != nullptr)
        {
            plan.swapsNeeded.push_back(taken);
        }
    }

    return plan;
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes,
                                const bool enable,
                                const std::vector<TakenPlace>& agreedSwaps,
                                const std::vector<StartupLine>& agreedEntries)
{
    Toggle(nodes, enable, PlanToggle(nodes, enable), agreedSwaps, agreedEntries);
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes,
                                const bool enable,
                                TogglePlan plan,
                                const std::vector<TakenPlace>& agreedSwaps,
                                const std::vector<StartupLine>& agreedEntries)
{
    auto work = std::make_shared<ToggleWork>();
    work->profile = session_.Profile();
    work->shown.enabled = session_.Snapshot().enabled;
    work->onDisk = std::move(plan.onDisk);

    if (!enable)
    {
        work->startupEntriesToTurnOff = agreedEntries;

        for (const TreeNode* node : nodes)
        {
            for (const TreeNode* addon : AddonsUnder(*node))
            {
                work->toDisable.push_back(*addon);
            }
        }

        RunTheBatch(std::move(work));
        return;
    }

    const std::vector<TreeNode>& libraries = session_.Snapshot().libraries;

    std::vector<const TreeNode*> occupants;
    std::set<std::string> heldBack;

    for (const TakenPlace& swap : plan.swapsNeeded)
    {
        if (WasAgreedTo(agreedSwaps, swap))
        {
            occupants.push_back(AddonAt(libraries, swap.occupant));
            continue;
        }

        heldBack.insert(ComparablePath(swap.addonFolder));
    }

    for (const TreeNode* occupant : occupants)
    {
        work->toDisable.push_back(*occupant);
    }

    for (const TreeNode* addon : AddonsToEnable(nodes, heldBack))
    {
        work->toEnable.push_back(*addon);
    }

    work->leftAlone = heldBack.size();

    RunTheBatch(std::move(work));
}

void AddonTreeViewModel::RunGuarded(std::function<void()> work, std::function<void()> done)
{
    if (toggling_)
    {
        return;
    }

    toggling_ = true;

    runner_.Run(std::move(work),
                [this, done = std::move(done)]
                {
                    toggling_ = false;

                    done();
                });
}

void AddonTreeViewModel::RunTheBatch(std::shared_ptr<ToggleWork> work)
{
    RunGuarded(
        [this, work]
        {
            LinkBatch batch;
            batch.startupEntriesToTurnOff = work->startupEntriesToTurnOff;

            for (const TreeNode& addon : work->toDisable)
            {
                batch.toDisable.push_back(&addon);
            }

            for (const TreeNode& addon : work->toEnable)
            {
                batch.toEnable.push_back(&addon);
            }

            work->report = service_.SetEnabled(work->profile, work->shown, batch, work->onDisk);
            work->report.leftAlone = work->leftAlone;
        },
        [this, work]
        {
            ApplyResults(work->report);
        });
}

void AddonTreeViewModel::UndoLastBatch()
{
    const auto results = std::make_shared<std::vector<LinkOperationResult>>();

    RunGuarded(
        [this, results]
        {
            *results = service_.UndoLastBatch();
        },
        [this, results]
        {
            ApplyResults({.results = *results, .drifted = 0});
        });
}

void AddonTreeViewModel::ApplyResults(const LinkBatchReport& report)
{
    session_.RefreshEntries();

    session_.NoteLinkResults(report.results);

    emit BatchFinished(report);
}

void AddonTreeViewModel::OverrideDestination(const std::vector<const TreeNode*>& nodes,
                                             const std::filesystem::path& destination) const
{
    session_.OverrideDestination(nodes, destination);
}

void AddonTreeViewModel::CreateCategory(const TreeNode* node, const QString& name)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
    {
        emit Refused(tr("Give the category a name."));
        return;
    }

    const FileOperationResult result = session_.CreateCategory(CategoryHolding(*node), wanted.toStdString());

    if (!Succeeded(result.result))
    {
        emit Refused(Describe(result));
    }
}

std::filesystem::path AddonTreeViewModel::RenameCategory(const TreeNode* node, const QString& name)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
    {
        emit Refused(tr("Give the category a name."));
        return {};
    }

    if (wanted.toStdString() == node->path.filename().string())
    {
        return node->path;
    }

    if (toggling_)
    {
        return {};
    }

    const FileOperationResult check = session_.CheckRenameCategory(node->path, wanted.toStdString());

    if (!Succeeded(check.result))
    {
        emit Refused(Describe(check));

        return {};
    }

    auto reorganized = std::make_shared<Session::ReorganizedLibrary>();
    reorganized->profile = session_.Profile();

    RunGuarded(
        [this, reorganized, category = node->path, chosen = wanted.toStdString()]
        {
            *reorganized = session_.RenameCategoryOn(std::move(reorganized->profile), category, chosen);
        },
        [this, reorganized]
        {
            const FileOperationResult& result = reorganized->results.front();

            session_.AdoptTheReorganization(std::move(reorganized->profile), TheFolderLanded(result.result));

            if (!Succeeded(result.result))
            {
                emit Refused(Describe(result));
            }
        });

    return check.path;
}

bool AddonTreeViewModel::CanRemoveCategory(const TreeNode* node)
{
    return node->kind == TreeNodeKind::Category && CountAddons(*node) == 0;
}

void AddonTreeViewModel::RemoveCategory(const TreeNode* node)
{
    auto reorganized = std::make_shared<Session::ReorganizedLibrary>();
    reorganized->profile = session_.Profile();

    RunGuarded(
        [this, reorganized, category = node->path]
        {
            *reorganized = session_.RemoveCategoryOn(std::move(reorganized->profile), category);
        },
        [this, reorganized]
        {
            const FileOperationResult& result = reorganized->results.front();

            session_.AdoptTheReorganization(std::move(reorganized->profile), Succeeded(result.result));

            if (!Succeeded(result.result))
            {
                emit Refused(Describe(result));
            }
        });
}

void AddonTreeViewModel::MoveTo(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& category)
{
    std::vector<AddonMove> moves;
    for (const TreeNode* node : nodes)
    {
        if (node->kind == TreeNodeKind::Addon)
        {
            moves.push_back(AddonMove{.addonFolder = node->path, .category = category});
        }
    }

    if (moves.empty())
    {
        emit Refused(tr("Select at least one addon to move."));
        return;
    }

    Perform(moves);
}

void AddonTreeViewModel::ApplySuggestions(const std::vector<CategorySuggestion>& chosen)
{
    std::vector<AddonMove> moves;
    moves.reserve(chosen.size());

    for (const CategorySuggestion& suggestion : chosen)
    {
        moves.push_back(AddonMove{.addonFolder = suggestion.addonFolder, .category = suggestion.suggestedCategory});
    }

    if (!moves.empty())
    {
        Perform(moves);
    }
}

void AddonTreeViewModel::Perform(const std::vector<AddonMove>& moves)
{
    auto reorganized = std::make_shared<Session::ReorganizedLibrary>();
    reorganized->profile = session_.Profile();

    RunGuarded(
        [this, reorganized, moves]
        {
            *reorganized = session_.MoveAddonsOn(std::move(reorganized->profile), moves);
        },
        [this, reorganized]
        {
            const bool landed = std::ranges::any_of(reorganized->results,
                                                    [](const FileOperationResult& result)
                                                    {
                                                        return TheFolderLanded(result.result);
                                                    });

            session_.AdoptTheReorganization(std::move(reorganized->profile), landed);

            QStringList refusals;

            for (const FileOperationResult& result : reorganized->results)
            {
                if (!Succeeded(result.result))
                {
                    refusals.append(Describe(result));
                }
            }

            if (!refusals.isEmpty())
            {
                emit Refused(refusals.join('\n'));
            }
        });
}

void AddonTreeViewModel::AdoptDestination(const TreeNode* category)
{
    const DestinationAgreement agreement = WhereTheEnabledAddonsPoint(*category, session_.Snapshot().entries);

    if (!agreement.unanimous)
    {
        emit Refused(tr("The enabled addons of this category are linked in different destinations. Choose a "
                        "destination for the category instead of adopting what is on the disk."));
        return;
    }

    if (agreement.destination.empty())
    {
        emit Refused(tr("No addon of this category is enabled, so there is no destination to adopt."));
        return;
    }

    session_.OverrideDestination({category}, agreement.destination);
}

std::vector<const TreeNode*> AddonTreeViewModel::StrayedUnder(const std::vector<const TreeNode*>& nodes) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const SimulatorProfile& profile = session_.Profile();

    std::vector<const TreeNode*> strayed;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (!DestinationItStrayedTo(profile, snapshot.entries, addon->path).empty())
            {
                strayed.push_back(addon);
            }
        }
    }

    return strayed;
}

std::size_t AddonTreeViewModel::StrayAddonsUnder(const std::vector<const TreeNode*>& nodes) const
{
    return StrayedUnder(nodes).size();
}

void AddonTreeViewModel::RelinkToTheProfileDestination(const std::vector<const TreeNode*>& nodes)
{
    if (toggling_)
    {
        return;
    }

    const std::vector<const TreeNode*> strayed = StrayedUnder(nodes);

    if (strayed.empty())
    {
        emit Refused(tr("No addon from here is linked away from the destination the profile says to use."));
        return;
    }

    auto work = std::make_shared<ToggleWork>();
    work->profile = session_.Profile();
    work->shown.enabled = session_.Snapshot().enabled;

    for (const TreeNode* addon : strayed)
    {
        work->toDisable.push_back(*addon);
    }

    RunGuarded(
        [this, work]
        {
            std::vector<const TreeNode*> relinking;
            relinking.reserve(work->toDisable.size());

            for (const TreeNode& addon : work->toDisable)
            {
                relinking.push_back(&addon);
            }

            work->report = service_.Relink(work->profile, work->shown, relinking);
        },
        [this, work]
        {
            ApplyResults(work->report);
        });
}

const TreeNode* AddonTreeViewModel::LibraryTreeHolding(const TreeNode& node) const
{
    const Library* library = LibraryContaining(session_.Profile(), node.path);

    return library == nullptr ? nullptr : LibraryTreeAt(session_.Snapshot().libraries, library->path);
}

std::vector<CategorySuggestion> AddonTreeViewModel::SuggestionsFor(const TreeNode* node) const
{
    const TreeNode* tree = LibraryTreeHolding(*node);

    return tree == nullptr ? std::vector<CategorySuggestion>{} : SuggestCategories(*tree, AddonsUnder(*node));
}

DependencyReport AddonTreeViewModel::DependenciesOf(const TreeNode* node) const
{
    if (node == nullptr || node->kind != TreeNodeKind::Addon || !node->addon.has_value())
    {
        return {};
    }

    return ReportDependencies(*node->addon, session_.Snapshot(), packages_);
}

std::vector<MoveTarget> AddonTreeViewModel::CategoriesFor(const TreeNode* node) const
{
    const TreeNode* tree = LibraryTreeHolding(*node);
    if (tree == nullptr)
    {
        return {};
    }

    const std::string holding = ComparablePath(CategoryHolding(*node));

    std::vector<MoveTarget> offered;
    for (const TreeNode* candidate : CategoriesOfferedIn(*tree, false))
    {
        if (ComparablePath(candidate->path) != holding)
        {
            offered.push_back(MoveTarget{.category = candidate->path,
                                         .relativePath = candidate->path.lexically_relative(tree->path)});
        }
    }

    return offered;
}

bool AddonTreeViewModel::WouldAcceptLibrary(const std::filesystem::path& path) const
{
    return session_.WouldAcceptLibrary(path);
}

void AddonTreeViewModel::AddLibrary(const std::filesystem::path& path)
{
    auto registration = std::make_shared<Session::LibraryRegistration>();
    registration->profile = session_.Profile();

    RunGuarded(
        [this, registration, path]
        {
            *registration = session_.RegisterLibraryOn(std::move(registration->profile), path);
        },
        [this, registration, path]
        {
            const LibraryReport report = registration->report;

            session_.AdoptTheRegistration(std::move(*registration));

            emit LibraryRegistered(path, report);
        });
}

bool AddonTreeViewModel::CanUndo() const
{
    return service_.CanUndo();
}

const SimulatorProfile& AddonTreeViewModel::Profile() const
{
    return session_.Profile();
}
