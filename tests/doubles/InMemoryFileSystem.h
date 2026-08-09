#ifndef FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
#define FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <vector>

#include "domain/model/WriteAccess.h"

class InMemoryFileSystem
{
public:
    void AddDirectory(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{.kind = NodeKind::Directory};
    }

    void AddFile(const std::filesystem::path& path, const std::uintmax_t size = 0)
    {
        AddTheFoldersAbove(path);

        nodes_[Key(path)] = Node{.kind = NodeKind::File, .target = {}, .readable = true, .size = size};
    }

    void AddFileWithContents(const std::filesystem::path& path, std::string contents)
    {
        AddTheFoldersAbove(path);

        nodes_[Key(path)] = Node{.kind = NodeKind::File,
                                 .target = {},
                                 .readable = true,
                                 .size = contents.size(),
                                 .contents = std::move(contents)};
    }

    [[nodiscard]] std::optional<std::string> ContentsOf(const std::filesystem::path& path) const
    {
        const auto node = nodes_.find(Key(path));

        return node == nodes_.end() || node->second.kind != NodeKind::File ? std::nullopt
                                                                           : std::optional(node->second.contents);
    }

    void AddLink(const std::filesystem::path& path, const std::filesystem::path& target)
    {
        nodes_[Key(path)] = Node{.kind = NodeKind::Link, .target = target, .readable = true};
    }

    void AddLinkWithUnreadableTarget(const std::filesystem::path& path)
    {
        nodes_[Key(path)] = Node{.kind = NodeKind::Link, .target = {}, .readable = false};
    }

    void MarkVolumeUnavailable(const std::filesystem::path& path)
    {
        unavailableVolumes_.insert(VolumeOf(path));
    }

    [[nodiscard]] bool VolumeIsAvailable(const std::filesystem::path& path) const
    {
        return !unavailableVolumes_.contains(VolumeOf(path));
    }

    [[nodiscard]] static bool SameVolume(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        return VolumeOf(left) == VolumeOf(right);
    }

    void MarkReadOnly(const std::filesystem::path& path)
    {
        refusedPaths_[Key(path)] = WriteAccess::TheVolumeIsReadOnly;
    }

    void DenyPermissionOn(const std::filesystem::path& path)
    {
        refusedPaths_[Key(path)] = WriteAccess::PermissionIsDenied;
    }

    [[nodiscard]] WriteAccess WriteAccessOn(const std::filesystem::path& path) const
    {
        if (!IsDirectory(path))
        {
            return WriteAccess::TheFolderIsNotThere;
        }

        const auto refused = refusedPaths_.find(Key(path));

        return refused == refusedPaths_.end() ? WriteAccess::ItAccepts : refused->second;
    }

    void SetFreeSpace(const std::filesystem::path& path, const std::uintmax_t bytes)
    {
        freeSpace_[VolumeOf(path)] = bytes;
    }

    void MarkFreeSpaceUnknown(const std::filesystem::path& path)
    {
        unmeasurableVolumes_.insert(VolumeOf(path));
    }

    [[nodiscard]] std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const
    {
        const std::string volume = VolumeOf(path);
        if (unmeasurableVolumes_.contains(volume))
        {
            return std::nullopt;
        }

        const auto measured = freeSpace_.find(volume);
        return measured == freeSpace_.end() ? std::numeric_limits<std::uintmax_t>::max() : measured->second;
    }

    void SetRecycleBinQuota(const std::filesystem::path& path, const std::uintmax_t bytes)
    {
        recycleQuota_[VolumeOf(path)] = bytes;
    }

    void MakeTheVolumeDeletePermanently(const std::filesystem::path& path)
    {
        nukingVolumes_.insert(VolumeOf(path));
    }

    [[nodiscard]] std::optional<std::uintmax_t> RecycleBinQuotaOn(const std::filesystem::path& path) const
    {
        const auto quota = recycleQuota_.find(VolumeOf(path));

        return quota == recycleQuota_.end() ? std::nullopt : std::optional(quota->second);
    }

    [[nodiscard]] bool VolumeRecycles(const std::filesystem::path& path) const
    {
        return !nukingVolumes_.contains(VolumeOf(path));
    }

    [[nodiscard]] std::optional<std::size_t> LongestEntryUnder(const std::filesystem::path& path) const
    {
        const std::string root = Key(path);
        if (!nodes_.contains(root))
        {
            return std::nullopt;
        }

        std::size_t longest = root.size();
        for (const std::string& key : nodes_ | std::views::keys)
        {
            if (IsUnder(key, root))
            {
                longest = std::max(longest, key.size());
            }
        }

        return longest;
    }

    bool RecycleTree(const std::filesystem::path& path)
    {
        if (!RemoveTree(path))
        {
            return false;
        }

        recycled_.push_back(Key(path));

        return true;
    }

    [[nodiscard]] bool WasRecycled(const std::filesystem::path& path) const
    {
        return std::ranges::find(recycled_, Key(path)) != recycled_.end();
    }

    void SetLastWriteTime(const std::filesystem::path& path, const std::chrono::system_clock::time_point when)
    {
        written_[Key(path)] = when;
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const
    {
        const auto when = written_.find(Key(path));

        return when == written_.end() ? std::nullopt : std::optional(when->second);
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

    [[nodiscard]] std::optional<std::filesystem::path> LinkTarget(const std::filesystem::path& path) const
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

    [[nodiscard]] std::vector<std::filesystem::path> ChildDirectoriesOf(const std::filesystem::path& path) const
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

    [[nodiscard]] std::vector<std::filesystem::path> ChildFilesOf(const std::filesystem::path& path) const
    {
        const std::string parent = Key(path);
        std::vector<std::filesystem::path> children;
        for (const auto& [key, node] : nodes_)
        {
            if (node.kind != NodeKind::File)
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

    [[nodiscard]] std::vector<std::filesystem::path> FilesUnder(const std::filesystem::path& path) const
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

    bool MoveTree(const std::filesystem::path& source, const std::filesystem::path& destination)
    {
        const std::string from = Key(source);
        if (!nodes_.contains(from) || nodes_.contains(Key(destination)))
        {
            return false;
        }

        const std::string to = Key(destination);
        std::map<std::string, Node> moved;
        for (const auto& [key, node] : nodes_)
        {
            if (key == from)
            {
                moved.emplace(to, node);
            }
            else if (IsUnder(key, from))
            {
                moved.emplace(to + key.substr(from.size()), node);
            }
            else
            {
                moved.emplace(key, node);
            }
        }

        nodes_ = std::move(moved);

        return true;
    }

    bool RemoveEmptyDirectory(const std::filesystem::path& path)
    {
        const std::string root = Key(path);
        if (KindOf(path) != NodeKind::Directory)
        {
            return false;
        }

        const bool occupied = std::ranges::any_of(nodes_,
                                                  [&root](const auto& entry)
                                                  {
                                                      return IsUnder(entry.first, root);
                                                  });
        if (occupied)
        {
            return false;
        }

        nodes_.erase(root);

        return true;
    }

    bool RemoveTree(const std::filesystem::path& path)
    {
        const std::string root = Key(path);
        if (!nodes_.contains(root))
        {
            return false;
        }

        std::erase_if(nodes_,
                      [&root](const auto& entry)
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
        std::filesystem::path target{};
        bool readable = true;
        std::uintmax_t size = 0;
        std::string contents{};
    };

    [[nodiscard]] static std::string VolumeOf(const std::filesystem::path& path)
    {
        const std::string text = path.generic_string();
        const std::size_t slash = text.find('/');
        std::string head = slash == std::string::npos ? text : text.substr(0, slash);
        std::ranges::transform(head, head.begin(),
                               [](const unsigned char letter)
                               {
                                   return static_cast<char>(std::tolower(letter));
                               });

        return head.ends_with(':') ? head : std::string{};
    }

    void AddTheFoldersAbove(const std::filesystem::path& path)
    {
        for (std::filesystem::path folder = path.parent_path(); !folder.empty() && folder != folder.root_path();
             folder = folder.parent_path())
        {
            if (nodes_.contains(Key(folder)))
            {
                return;
            }

            nodes_[Key(folder)] = Node{.kind = NodeKind::Directory};
        }
    }

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
                                   [&root](const std::string& key)
                                   {
                                       return IsUnder(key, root);
                                   });
    }

    std::map<std::string, Node> nodes_;
    std::map<std::string, std::chrono::system_clock::time_point> written_;
    std::map<std::string, std::uintmax_t> freeSpace_;
    std::map<std::string, std::uintmax_t> recycleQuota_;
    std::set<std::string> nukingVolumes_;
    std::vector<std::string> recycled_;
    std::set<std::string> unmeasurableVolumes_;
    std::set<std::string> unavailableVolumes_;
    std::map<std::string, WriteAccess> refusedPaths_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_IN_MEMORY_FILE_SYSTEM_H
