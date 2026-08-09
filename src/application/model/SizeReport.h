#ifndef FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/TreeNode.h"
#include "domain/support/PathUtils.h"

struct MeasuredNode
{
    TreeNodeKind kind = TreeNodeKind::Category;
    std::filesystem::path path{};
    std::uintmax_t bytes = 0;
    bool measured = true;
    std::size_t longestEntry = 0;
    std::vector<MeasuredNode> children{};
};

struct SizeReport
{
    std::vector<MeasuredNode> libraries{};
    bool complete = false;
    std::chrono::system_clock::time_point measuredAt{};
};

struct MeasuredTree
{
    std::uintmax_t bytes = 0;
    std::size_t longestEntry = 0;
};

struct MeasuredFolder
{
    std::filesystem::path folder{};
    std::uintmax_t bytes = 0;
    std::size_t longestEntry = 0;
    bool measured = false;
};

struct FolderSizeReport
{
    std::vector<MeasuredFolder> folders{};
    std::uintmax_t bytes = 0;
    std::size_t measured = 0;
    bool complete = false;
    std::chrono::system_clock::time_point measuredAt{};
};

struct SizeProgress
{
    std::filesystem::path folder{};
    std::size_t measured = 0;
    std::size_t total = 0;
};

[[nodiscard]] inline MeasuredFolder FolderIn(const std::vector<MeasuredFolder>& weighed,
                                             const std::filesystem::path& wanted)
{
    const std::string key = ComparablePath(wanted);

    const auto found = std::ranges::find_if(weighed,
                                            [&key](const MeasuredFolder& folder)
                                            {
                                                return ComparablePath(folder.folder) == key;
                                            });

    return found == weighed.end() ? MeasuredFolder{} : *found;
}

#endif // FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H
