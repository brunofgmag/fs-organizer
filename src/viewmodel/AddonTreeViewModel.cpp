#include "viewmodel/AddonTreeViewModel.h"

#include <algorithm>

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
}

AddonTreeViewModel::AddonTreeViewModel(Session& session,
                                       ProfileService& service,
                                       const ProcessProbe& probe,
                                       AddonTreeModel& model,
                                       const SessionNotifier& notifier,
                                       QObject* parent)
    : QObject(parent), session_(session), service_(service), probe_(probe), model_(model)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &AddonTreeViewModel::AdoptScan);

    connect(&notifier, &SessionNotifier::Refreshed, this,
            [this]
            {
                model_.Refresh(session_.Snapshot(), session_.Profile());
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

void AddonTreeViewModel::AdoptScan()
{
    model_.Show(session_.Snapshot(), session_.Profile());

    if (!probe_.SimulatorIsRunning() && restartPending_)
    {
        restartPending_ = false;
        emit RestartPendingChanged(false);
    }

    emit Shown();
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes)
{
    const ProfileSnapshot& snapshot = session_.Snapshot();

    Toggle(nodes, ShouldEnable(session_.Profile(), snapshot.entries, snapshot.enabled, nodes));
}

void AddonTreeViewModel::Toggle(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    const std::vector<LinkOperationResult> results =
        service_.SetEnabled(session_.Profile(), session_.Snapshot(), nodes, enable);

    ApplyResults(results);
}

void AddonTreeViewModel::UndoLastBatch()
{
    const std::vector<LinkOperationResult> results = service_.UndoLastBatch();

    ApplyResults(results);
}

void AddonTreeViewModel::ApplyResults(const std::vector<LinkOperationResult>& results)
{
    session_.RefreshEntries();

    NoteSimulatorState(results);

    emit BatchFinished(results);
}

void AddonTreeViewModel::NoteSimulatorState(const std::vector<LinkOperationResult>& results)
{
    const bool changed = std::ranges::any_of(results,
                                             [](const LinkOperationResult& result)
                                             {
                                                 return result.outcome.Succeeded();
                                             });

    if (!changed || !probe_.SimulatorIsRunning())
    {
        return;
    }

    if (!warnedAboutSimulator_)
    {
        warnedAboutSimulator_ = true;
        emit SimulatorIsRunning();
    }

    if (!restartPending_)
    {
        restartPending_ = true;
        emit RestartPendingChanged(true);
    }
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
        emit Refused(tr("Dê um nome à categoria."));
        return;
    }

    const FileOperationResult result = session_.CreateCategory(CategoryHolding(*node), wanted.toStdString());

    if (result.result != FileResult::Completed)
    {
        emit Refused(Describe(result));
    }
}

void AddonTreeViewModel::RenameCategory(const TreeNode* node, const QString& name)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
    {
        emit Refused(tr("Dê um nome à categoria."));
        return;
    }

    if (wanted.toStdString() == node->path.filename().string())
    {
        return;
    }

    const FileOperationResult result = session_.RenameCategory(node->path, wanted.toStdString());

    if (result.result != FileResult::Completed)
    {
        emit Refused(Describe(result));
    }
}

bool AddonTreeViewModel::CanRemoveCategory(const TreeNode* node) const
{
    return node->kind == TreeNodeKind::Category && CountAddons(*node) == 0;
}

void AddonTreeViewModel::RemoveCategory(const TreeNode* node)
{
    const FileOperationResult result = session_.RemoveCategory(node->path);

    if (result.result != FileResult::Completed)
    {
        emit Refused(Describe(result));
    }
}

void AddonTreeViewModel::MoveTo(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& category)
{
    std::vector<AddonMove> moves;
    for (const TreeNode* node : nodes)
    {
        if (node->kind == TreeNodeKind::Addon)
        {
            moves.push_back(AddonMove{node->path, category});
        }
    }

    if (moves.empty())
    {
        emit Refused(tr("Selecione ao menos um addon para mover."));
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
        moves.push_back(AddonMove{suggestion.addonFolder, suggestion.suggestedCategory});
    }

    if (!moves.empty())
    {
        Perform(moves);
    }
}

void AddonTreeViewModel::Perform(const std::vector<AddonMove>& moves)
{
    QStringList refusals;

    for (const FileOperationResult& result : session_.MoveAddons(moves))
    {
        if (result.result != FileResult::Completed)
        {
            refusals.append(Describe(result));
        }
    }

    if (!refusals.isEmpty())
    {
        emit Refused(refusals.join('\n'));
    }
}

void AddonTreeViewModel::AdoptDestination(const TreeNode* category)
{
    const DestinationAgreement agreement = WhereTheEnabledAddonsPoint(*category, session_.Snapshot().entries);

    if (!agreement.unanimous)
    {
        emit Refused(tr("Os addons habilitados desta categoria estão ligados em destinos diferentes. Escolha um "
                        "destino para a categoria em vez de adotar o que está no disco."));
        return;
    }

    if (agreement.destination.empty())
    {
        emit Refused(tr("Nenhum addon desta categoria está habilitado, então não há destino para adotar."));
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
    const std::vector<const TreeNode*> strayed = StrayedUnder(nodes);

    if (strayed.empty())
    {
        emit Refused(tr("Nenhum addon daqui está ligado fora do destino que o perfil manda usar."));
        return;
    }

    Toggle(strayed, false);
    Toggle(strayed, true);
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

std::vector<std::filesystem::path> AddonTreeViewModel::CategoriesFor(const TreeNode* node) const
{
    const TreeNode* tree = LibraryTreeHolding(*node);
    if (tree == nullptr)
    {
        return {};
    }

    const std::string holding = ComparablePath(CategoryHolding(*node));

    std::vector<std::filesystem::path> offered;
    for (const TreeNode* candidate : CategoriesUnder(*tree))
    {
        if (candidate->kind == TreeNodeKind::Category && ComparablePath(candidate->path) != holding
            && HoldsAddonsOrWasDeclared(*candidate))
        {
            offered.push_back(candidate->path);
        }
    }

    return offered;
}

LibraryReport AddonTreeViewModel::AddLibrary(const std::filesystem::path& path) const
{
    return session_.RegisterLibrary(path);
}

bool AddonTreeViewModel::CanUndo() const
{
    return service_.CanUndo();
}

const SimulatorProfile& AddonTreeViewModel::Profile() const
{
    return session_.Profile();
}
