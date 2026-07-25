#ifndef FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
#define FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <vector>

class InMemoryFileSystem
{
public:
    void AddDirectory(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{NodeKind::Directory};
    }

    void AddFile(const std::filesystem::path& path, const std::uintmax_t size = 0)
    {
        nodes_[Key(path)] = Node{NodeKind::File, {}, true, size};
    }

    void AddLink(const std::filesystem::path& path, const std::filesystem::path& target)
    {
        nodes_[Key(path)] = Node{NodeKind::Link, target, true};
    }

    void AddLinkWithUnreadableTarget(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{NodeKind::Link, {}, false};
    }

    void MarkVolumeUnavailable(const std::filesystem::path& path)
    {
        unavailableVolumes_.insert(path.root_name().generic_string());
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const
    {
        return !unavailableVolumes_.contains(path.root_name().generic_string());
    }

    void MarkReadOnly(const std::filesystem::path& path)
    {
        readOnlyPaths_.insert(Key(path));
    }

    [[nodiscard]] bool IsWritable(const std::filesystem::path& path) const
    {
        return IsDirectory(path) && !readOnlyPaths_.contains(Key(path));
    }

    [[nodiscard]] bool Exists(const std::filesystem::path& path) const
    {
        return nodes_.contains(Key(path));
    }

    [[nodiscard]] bool IsDirectory(const std::filesystem::path& path) const
    {
        return KindOf(path) == NodeKind::Directory;
    }

    [[nodiscard]] bool IsFile(const std::filesystem::path& path) const
    {
        return KindOf(path) == NodeKind::File;
    }

    [[nodiscard]] bool IsLink(const std::filesystem::path& path) const
    {
        return KindOf(path) == NodeKind::Link;
    }

    [[nodiscard]] std::optional<std::filesystem::path>
    LinkTarget(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        if (node == nodes_.end() || node->second.kind != NodeKind::Link || !node->second.readable)
        {
            return std::nullopt;
        }
        return node->second.target;
    }

    [[nodiscard]] std::uintmax_t FileSize(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        return node == nodes_.end() ? 0 : node->second.size;
    }

    [[nodiscard]] std::vector<std::filesystem::path>
    ChildDirectoriesOf(const std::filesystem::path& path) const
    {
        const std::string parent = Key(path);
        std::vector<std::filesystem::path> children;
        for (const auto& [key, node] : nodes_)
        {
            if (node.kind == NodeKind::File)
            {
                continue;
            }
            const std::filesystem::path candidate(key);
            if (candidate.parent_path().generic_string() == parent)
            {
                children.push_back(candidate);
            }
        }
        return children;
    }

    [[nodiscard]] std::vector<std::filesystem::path>
    FilesUnder(const std::filesystem::path& path) const
    {
        std::vector<std::filesystem::path> files;
        for (const auto& [key, node] : nodes_)
        {
            if (node.kind == NodeKind::File && IsUnder(key, Key(path)))
            {
                files.emplace_back(key);
            }
        }
        return files;
    }

    bool RemoveNode(const std::filesystem::path& path)
    {
        const std::string key = Key(path);
        const auto node = nodes_.find(key);
        if (node == nodes_.end())
        {
            return false;
        }

        if (node->second.kind == NodeKind::Directory && HasDescendants(key))
        {
            return false;
        }

        nodes_.erase(node);
        return true;
    }

    bool RemoveTree(const std::filesystem::path& path)
    {
        const std::string root = Key(path);
        if (!nodes_.contains(root))
        {
            return false;
        }

        std::erase_if(nodes_, [&root](const auto& entry)
        {
            return entry.first == root || IsUnder(entry.first, root);
        });
        return true;
    }

private:
    enum class NodeKind
    {
        Directory,
        File,
        Link
    };

    struct Node
    {
        NodeKind kind = NodeKind::Directory;
        std::filesystem::path target;
        bool readable = true;
        std::uintmax_t size = 0;
    };

    [[nodiscard]] static std::string Key(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    [[nodiscard]] static bool IsUnder(const std::string& candidate, const std::string& root)
    {
        return candidate.size() > root.size() && candidate.compare(0, root.size(), root) == 0
            && candidate[root.size()] == '/';
    }

    [[nodiscard]] std::optional<NodeKind> KindOf(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        return node == nodes_.end() ? std::nullopt : std::optional(node->second.kind);
    }

    [[nodiscard]] bool HasDescendants(const std::string& root) const
    {
        return std::ranges::any_of(nodes_ | std::views::keys,
                                   [&root](const std::string& key) { return IsUnder(key, root); });
    }

    std::map<std::string, Node> nodes_;
    std::set<std::string> unavailableVolumes_;
    std::set<std::string> readOnlyPaths_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
