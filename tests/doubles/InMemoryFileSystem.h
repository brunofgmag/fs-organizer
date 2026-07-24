#ifndef FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
#define FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

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
        nodes_[Key(path)] = Node{NodeKind::Link, target};
    }

    [[nodiscard]] bool Exists(const std::filesystem::path& path) const
    {
        return nodes_.find(Key(path)) != nodes_.end();
    }

    [[nodiscard]] bool IsLink(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        return node != nodes_.end() && node->second.Kind == NodeKind::Link;
    }

    [[nodiscard]] std::optional<std::filesystem::path>
    LinkTarget(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));
        if (node == nodes_.end() || node->second.Kind != NodeKind::Link)
        {
            return std::nullopt;
        }
        return node->second.Target;
    }

    void RemoveNode(const std::filesystem::path& path)
    {
        nodes_.erase(Key(path));
    }

    [[nodiscard]] std::vector<std::string> Paths() const
    {
        std::vector<std::string> paths;
        paths.reserve(nodes_.size());
        for (const auto& entry: nodes_)
        {
            paths.push_back(entry.first);
        }
        return paths;
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
        NodeKind Kind;
        std::filesystem::path Target;
    };

    [[nodiscard]] static std::string Key(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    std::map<std::string, Node> nodes_;
};

#endif
