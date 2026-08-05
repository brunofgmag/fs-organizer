#include "infrastructure/sim/ContentListLocations.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace
{
    constexpr std::string_view kContentListName = "Content.xml";

    [[nodiscard]] std::vector<std::filesystem::path> FoldersToLookIn(const std::filesystem::path& base,
                                                                     const FilesystemProbe& filesystemProbe)
    {
        std::vector<std::filesystem::path> accountFolders = filesystemProbe.ChildDirectories(base);
        std::ranges::sort(accountFolders);

        std::vector<std::filesystem::path> folders{base};
        folders.insert(folders.end(), accountFolders.begin(), accountFolders.end());

        return folders;
    }
}

std::vector<ContentListLocation> ContentListLocations(const std::vector<UserCfgLocation>& userCfgLocations,
                                                      const FilesystemProbe& filesystemProbe)
{
    std::vector<ContentListLocation> found;

    for (const UserCfgLocation& location : userCfgLocations)
    {
        const std::filesystem::path base = location.configPath.parent_path();

        for (const std::filesystem::path& folder : FoldersToLookIn(base, filesystemProbe))
        {
            const std::filesystem::path candidate = folder / kContentListName;

            if (filesystemProbe.EntryExistsWithoutFollowingLinks(candidate))
            {
                found.push_back({.variant = location.variant, .listPath = candidate});
            }
        }
    }

    return found;
}

std::optional<ChosenContentList> ChooseContentList(const std::vector<ContentListLocation>& locations,
                                                   const SimulatorVariant variant)
{
    const ContentListLocation* first = nullptr;
    std::size_t howMany = 0;

    for (const ContentListLocation& location : locations)
    {
        if (location.variant != variant)
        {
            continue;
        }

        ++howMany;
        first = first == nullptr ? &location : first;
    }

    if (first == nullptr)
    {
        return std::nullopt;
    }

    return ChosenContentList{.listPath = first->listPath,
                             .accountFolder =
                                 howMany > 1 ? first->listPath.parent_path().filename().string() : std::string()};
}
