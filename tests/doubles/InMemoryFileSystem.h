#ifndef FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
#define FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>

class InMemoryFileSystem
{
public:
    void AddDirectory(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{NodeKind::Directory, {}};
    }

    void AddFile(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{NodeKind::File, {}};
    }

    void AddLink(const std::filesystem::path& path, const std::filesystem::path& target)
    {
        nodes_[Key(path)] = Node{NodeKind::Link, target, true};
    }

    void AddLinkWithUnreadableTarget(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{NodeKind::Link, {}, false};
    }

    [[nodiscard]] bool Exists(const std::filesystem::path& path) const
    {
        return nodes_.find(Key(path)) != nodes_.end();
    }

    [[nodiscard]] bool IsDirectory(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        return node != nodes_.end() && node->second.kind == NodeKind::Directory;
    }

    [[nodiscard]] bool IsLink(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        return node != nodes_.end() && node->second.kind == NodeKind::Link;
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

    void RemoveNode(const std::filesystem::path& path)
    {
        nodes_.erase(Key(path));
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
    };

    [[nodiscard]] static std::string Key(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    std::map<std::string, Node> nodes_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
