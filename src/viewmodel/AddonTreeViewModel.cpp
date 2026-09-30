#include "viewmodel/AddonTreeViewModel.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include <QtCore/QStringList>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonDestinations.h"
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
      selectionCaller_(sizes.NewCaller()),
      swapsCaller_(sizes.NewCaller()),
      toggling_(runner)
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

    sizes_.MeasureFolders(addonFolders, selectionCaller_, Freshness::ReuseWhatIsKnown, {},
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

    sizes_.MeasureFolders(folders, swapsCaller_, Freshness::ReuseWhatIsKnown, {},
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
    const ProfileSnapshot& snapshot = session_.Snapshot();

    std::vector<TakenPlace> swaps;

    for (const TakenPlace& taken : service_.PlacesTakenNow(session_.Profile(), nodes, snapshot))
    {
        if (AddonAt(snapshot.libraries, taken.occupant) != nullptr)
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
    return TogglePlan{.swapsNeeded = enable ? SwapsNeededTo(nodes) : std::vector<TakenPlace>{}};
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
                                const TogglePlan& plan,
                                const std::vector<TakenPlace>& agreedSwaps,
                                const std::vector<StartupLine>& agreedEntries)
{
    const std::shared_ptr<ToggleWork> work = WorkOnTheShownProfile();

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

        RunTheBatch(work);
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

    RunTheBatch(work);
}

std::shared_ptr<AddonTreeViewModel::ToggleWork> AddonTreeViewModel::WorkOnTheShownProfile() const
{
    auto work = std::make_shared<ToggleWork>();
    work->stamp = session_.StampForAnEntriesRead();
    work->shown.enabled = session_.Snapshot().enabled;
    work->shown.libraries = session_.Snapshot().libraries;

    return work;
}

void AddonTreeViewModel::RunTheBatch(const std::shared_ptr<ToggleWork>& work)
{
    toggling_.Run(
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

            work->outcome = service_.SetEnabled(work->stamp, work->shown, batch);
            work->outcome.report.leftAlone = work->leftAlone;
            work->simulatorRunning = session_.SimulatorIsRunningAfter(work->outcome.report.results);
        },
        [this, work]
        {
            ApplyResults(*work);
        });
}

void AddonTreeViewModel::UndoLastBatch()
{
    const std::shared_ptr<ToggleWork> work = WorkOnTheShownProfile();

    toggling_.Run(
        [this, work]
        {
            work->outcome = service_.UndoLastBatch(work->stamp, work->shown.libraries);
            work->simulatorRunning = session_.SimulatorIsRunningAfter(work->outcome.report.results);
        },
        [this, work]
        {
            ApplyResults(*work);
        });
}

void AddonTreeViewModel::ApplyResults(ToggleWork& work)
{
    session_.AdoptTheEntriesRead(std::move(work.outcome.read));

    session_.NoteLinkResults(work.outcome.report.results, work.simulatorRunning);

    emit BatchFinished(work.outcome.report);
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
        emit Refused(tr("Enter a name for the category."));
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
        emit Refused(tr("Enter a name for the category."));
        return {};
    }

    if (wanted.toStdString() == node->path.filename().string())
    {
        return node->path;
    }

    if (toggling_.Busy())
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

    toggling_.Run(
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

    toggling_.Run(
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

    toggling_.Run(
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
        emit Refused(tr("The enabled addons of this category are linked in different destinations. Choose one "
                        "destination for the category instead."));
        return;
    }

    if (agreement.destination.empty())
    {
        emit Refused(tr("No addon of this category is enabled, so there is no destination to keep."));
        return;
    }

    session_.OverrideDestination({category}, agreement.destination);
}

std::vector<const TreeNode*> AddonTreeViewModel::StrayedUnder(const std::vector<const TreeNode*>& nodes) const
{
    const AddonDestinations destinations(session_.Profile(), session_.Snapshot().entries);

    std::vector<const TreeNode*> strayed;
    std::set<std::string> asked;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (asked.insert(ComparablePath(addon->path)).second && !destinations.Of(addon->path).strayedTo.empty())
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
    if (toggling_.Busy())
    {
        return;
    }

    RelinkStrayed(StrayedUnder(nodes));
}

void AddonTreeViewModel::RelinkStrayed(const std::vector<const TreeNode*>& strayed)
{
    if (toggling_.Busy())
    {
        return;
    }

    if (strayed.empty())
    {
        emit Refused(tr("Every addon here is already linked in the profile destination."));
        return;
    }

    const std::shared_ptr<ToggleWork> work = WorkOnTheShownProfile();

    for (const TreeNode* addon : strayed)
    {
        work->toDisable.push_back(*addon);
    }

    toggling_.Run(
        [this, work]
        {
            std::vector<const TreeNode*> relinking;
            relinking.reserve(work->toDisable.size());

            for (const TreeNode& addon : work->toDisable)
            {
                relinking.push_back(&addon);
            }

            work->outcome = service_.Relink(work->stamp, work->shown, relinking);
            work->simulatorRunning = session_.SimulatorIsRunningAfter(work->outcome.report.results);
        },
        [this, work]
        {
            ApplyResults(*work);
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

std::size_t AddonTreeViewModel::MovableAmong(const std::vector<const TreeNode*>& addons) const
{
    std::map<const TreeNode*, std::set<std::string>> offeredByTree;
    std::size_t movable = 0;

    for (const TreeNode* node : addons)
    {
        const TreeNode* tree = LibraryTreeHolding(*node);
        if (tree == nullptr)
        {
            continue;
        }

        auto offered = offeredByTree.find(tree);
        if (offered == offeredByTree.end())
        {
            std::set<std::string> categories;
            for (const TreeNode* candidate : CategoriesOfferedIn(*tree, false))
            {
                categories.insert(ComparablePath(candidate->path));
            }

            offered = offeredByTree.emplace(tree, std::move(categories)).first;
        }

        const std::set<std::string>& categories = offered->second;
        const std::size_t holdingIt = categories.contains(ComparablePath(CategoryHolding(*node))) ? 1 : 0;

        movable += categories.size() > holdingIt ? 1 : 0;
    }

    return movable;
}

bool AddonTreeViewModel::WouldAcceptLibrary(const std::filesystem::path& path) const
{
    return session_.WouldAcceptLibrary(path);
}

void AddonTreeViewModel::AddLibrary(const std::filesystem::path& path)
{
    auto registration = std::make_shared<Session::LibraryRegistration>();
    registration->profile = session_.Profile();

    toggling_.Run(
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
