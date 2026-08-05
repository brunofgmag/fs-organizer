#ifndef FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H
#define FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "domain/model/TreeNode.h"

struct MeasuredNode
{
    TreeNodeKind kind = TreeNodeKind::Category;
    std::filesystem::path path{};
    std::uintmax_t bytes = 0;
    bool measured = true;
    std::vector<MeasuredNode> children{};
};

struct SizeReport
{
    std::vector<MeasuredNode> libraries{};
    bool complete = false;
    std::chrono::system_clock::time_point measuredAt{};
};

struct SizeProgress
{
    std::filesystem::path folder{};
    std::size_t measured = 0;
    std::size_t total = 0;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_SIZE_REPORT_H
