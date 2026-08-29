#include "viewmodel/DeletionViewModel.h"

#include <algorithm>
#include <memory>

#include "domain/tree/AddonTree.h"
#include "viewmodel/SimulatorText.h"

DeletionViewModel::DeletionViewModel(Session& session,
                                     ProfileService& profileService,
                                     const DeletionService& service,
                                     SizeService& sizes,
                                     BackgroundRunner& runner,
                                     QObject* parent)
    : QObject(parent),
      session_(session),
      profileService_(profileService),
      service_(service),
      sizes_(sizes),
      runner_(runner),
      caller_(sizes.NewCaller())
{
}

std::vector<SimulatorProfile> DeletionViewModel::EveryProfile() const
{
    const std::vector<SimulatorProfile>& stored = session_.Settings().profiles;

    return stored.empty() ? std::vector<SimulatorProfile>{session_.Profile()} : stored;
}

std::vector<const TreeNode*> DeletionViewModel::NodesStillThere(const std::vector<std::filesystem::path>& chosen) const
{
    std::vector<const TreeNode*> nodes;
    nodes.reserve(chosen.size());

    for (const std::filesystem::path& folder : chosen)
    {
        if (const TreeNode* node = NodeAt(session_.Snapshot().libraries, folder))
        {
            nodes.push_back(node);
        }
    }

    return nodes;
}

void DeletionViewModel::PlanToDelete(const std::vector<const TreeNode*>& nodes)
{
    std::vector<std::filesystem::path> chosen;
    std::vector<std::filesystem::path> addonFolders;

    for (const TreeNode* node : nodes)
    {
        if (node == nullptr)
        {
            continue;
        }

        chosen.push_back(node->path);

        if (node->kind == TreeNodeKind::Addon)
        {
            addonFolders.push_back(node->path);
        }
    }

    emit Weighing();

    sizes_.MeasureFolders(addonFolders, caller_, Freshness::MeasureAgain, {},
                          [this, chosen](const FolderSizeReport&)
                          {
                              const std::vector<const TreeNode*> stillThere = NodesStillThere(chosen);

                              emit Planned(service_.Plan(session_.Profile(), EveryProfile(), stillThere));
                          });
}

void DeletionViewModel::Delete(const DeletionPlan& plan, const DeletionRoute route)
{
    if (deleting_)
    {
        return;
    }

    deleting_ = true;

    emit Deleting();

    const std::vector<SimulatorProfile> everyProfile = EveryProfile();
    const auto results = std::make_shared<std::vector<DeletionResult>>();

    runner_.Run(
        [this, everyProfile, plan, route, results]
        {
            *results = service_.Delete(everyProfile, plan, route);
        },
        [this, route, results]
        {
            deleting_ = false;

            if (std::ranges::any_of(*results,
                                    [](const DeletionResult& result)
                                    {
                                        return Succeeded(result.result);
                                    }))
            {
                profileService_.ForgetUndo();
            }

            emit Deleted(*results, route);
        });
}

QString DeletionViewModel::LabelOfProfile(const std::string& profileId) const
{
    for (const SimulatorProfile& profile : EveryProfile())
    {
        if (profile.id == profileId)
        {
            return NameOf(profile.variant);
        }
    }

    return QString::fromStdString(profileId);
}
