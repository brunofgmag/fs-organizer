#include "domain/bisection/CoupledUnits.h"

#include <algorithm>
#include <map>
#include <numeric>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    class Merger
    {
    public:
        explicit Merger(const std::size_t members) : owner_(members)
        {
            std::iota(owner_.begin(), owner_.end(), std::size_t{0});
        }

        [[nodiscard]] std::size_t OwnerOf(const std::size_t member)
        {
            if (owner_[member] == member)
            {
                return member;
            }

            owner_[member] = OwnerOf(owner_[member]);

            return owner_[member];
        }

        void Join(const std::size_t one, const std::size_t other)
        {
            const std::size_t left = OwnerOf(one);
            const std::size_t right = OwnerOf(other);

            if (left != right)
            {
                owner_[std::max(left, right)] = std::min(left, right);
            }
        }

    private:
        std::vector<std::size_t> owner_;
    };

    void JoinEveryoneSharing(Merger& merger, const std::map<std::string, std::vector<std::size_t>>& byKey)
    {
        for (const auto& [key, members] : byKey)
        {
            for (std::size_t next = 1; next < members.size(); ++next)
            {
                merger.Join(members.front(), members[next]);
            }
        }
    }

    [[nodiscard]] std::optional<std::filesystem::path> BaseOf(const std::vector<CouplingFacts>& facts,
                                                              const std::vector<std::size_t>& members)
    {
        if (members.size() < 2)
        {
            return std::nullopt;
        }

        std::optional<std::filesystem::path> found;

        for (const std::size_t member : members)
        {
            if (facts[member].declaresABaseContainer)
            {
                continue;
            }

            if (found.has_value())
            {
                return std::nullopt;
            }

            found = facts[member].folder;
        }

        return found;
    }
}

std::vector<SearchUnit> UnitsFrom(const std::vector<CouplingFacts>& facts)
{
    Merger merger(facts.size());
    std::map<std::string, std::size_t> byFolderName;
    std::map<std::string, std::vector<std::size_t>> byModelFolder;
    std::map<std::string, std::vector<std::size_t>> byWrittenPath;

    for (std::size_t member = 0; member < facts.size(); ++member)
    {
        byFolderName.emplace(ComparableFileName(facts[member].folder), member);

        for (const std::string& modelFolder : facts[member].modelFolders)
        {
            byModelFolder[LoweredForComparison(modelFolder)].push_back(member);
        }

        for (const std::filesystem::path& written : facts[member].writesInside)
        {
            byWrittenPath[ComparablePath(written)].push_back(member);
        }
    }

    JoinEveryoneSharing(merger, byModelFolder);
    JoinEveryoneSharing(merger, byWrittenPath);

    for (std::size_t member = 0; member < facts.size(); ++member)
    {
        for (const std::string& declared : facts[member].declaredPackages)
        {
            const auto named = byFolderName.find(LoweredForComparison(declared));

            if (named != byFolderName.end())
            {
                merger.Join(member, named->second);
            }
        }
    }

    std::map<std::size_t, std::vector<std::size_t>> grouped;

    for (std::size_t member = 0; member < facts.size(); ++member)
    {
        grouped[merger.OwnerOf(member)].push_back(member);
    }

    std::vector<SearchUnit> units;
    units.reserve(grouped.size());

    for (auto& [owner, members] : grouped)
    {
        std::ranges::sort(members,
                          [&facts](const std::size_t one, const std::size_t other)
                          {
                              return ComparablePath(facts[one].folder) < ComparablePath(facts[other].folder);
                          });

        SearchUnit unit;
        unit.base = BaseOf(facts, members);
        unit.addons.reserve(members.size());

        for (const std::size_t member : members)
        {
            unit.addons.push_back(facts[member].folder);
        }

        units.push_back(unit);
    }

    std::ranges::sort(units,
                      [](const SearchUnit& one, const SearchUnit& other)
                      {
                          return ComparablePath(one.addons.front()) < ComparablePath(other.addons.front());
                      });

    return units;
}
