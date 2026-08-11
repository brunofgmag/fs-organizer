#include "domain/scenery/AirportCoverage.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>

#include "domain/support/PathUtils.h"

namespace
{
    struct Gathering
    {
        AddonId addon{};
        std::vector<std::string> codes{};
        bool somethingWasNotRead = false;
    };

    void GatherFrom(Gathering& into, const SceneryCodes& file)
    {
        if (file.reading != SceneryReading::Read || file.anIdentifierDidNotDecode)
        {
            into.somethingWasNotRead = true;
        }

        for (const std::string& code : file.codes)
        {
            if (std::ranges::find(into.codes, code) == into.codes.end())
            {
                into.codes.push_back(code);
            }
        }
    }

    [[nodiscard]] AirportEvidence EvidenceOf(const Gathering& gathered)
    {
        if (!gathered.codes.empty())
        {
            return AirportEvidence::TheCodeWasRead;
        }

        return gathered.somethingWasNotRead ? AirportEvidence::ARecordWasNotRead
                                            : AirportEvidence::ItCarriesNoAirportRecord;
    }
}

std::vector<AirportsOfAnAddon> AirportsOfEachAddon(const std::vector<SceneryOfAnAddon>& scenery)
{
    std::vector<Gathering> gathered;
    std::unordered_map<std::string, std::size_t> reachedAt;

    for (const SceneryOfAnAddon& addon : scenery)
    {
        const auto [reached, isTheFirstReach] = reachedAt.emplace(ComparablePath(addon.resolvedPath), gathered.size());

        if (isTheFirstReach)
        {
            gathered.push_back({.addon = addon.addon});
        }

        Gathering& into = gathered[reached->second];

        for (const SceneryCodes& file : addon.files)
        {
            GatherFrom(into, file);
        }
    }

    std::vector<AirportsOfAnAddon> airports;
    airports.reserve(gathered.size());

    for (const Gathering& one : gathered)
    {
        airports.push_back({.addon = one.addon, .evidence = EvidenceOf(one), .codes = one.codes});
    }

    return airports;
}

std::vector<AirportGroup> GroupsOfTheSameAirport(const std::vector<AirportsOfAnAddon>& addons)
{
    std::vector<AirportGroup> groups;
    std::unordered_map<std::string, std::size_t> groupOf;

    for (const AirportsOfAnAddon& addon : addons)
    {
        for (const std::string& code : addon.codes)
        {
            const auto known = groupOf.find(code);

            if (known == groupOf.end())
            {
                groupOf.emplace(code, groups.size());
                groups.push_back({.code = code, .addons = {addon.addon}});
                continue;
            }

            groups[known->second].addons.push_back(addon.addon);
        }
    }

    std::erase_if(groups,
                  [](const AirportGroup& group)
                  {
                      return group.addons.size() < 2;
                  });

    return groups;
}

bool ItIsTheSamePair(const CoexistingPair& left, const CoexistingPair& right)
{
    if (left.one == right.one && left.other == right.other)
    {
        return true;
    }

    return left.one == right.other && left.other == right.one;
}

std::vector<AirportPair> PairsOfTheSameAirport(const std::vector<AirportsOfAnAddon>& addons,
                                               const std::vector<CoexistingPair>& coexisting)
{
    const auto theUserSaidTheyCanCoexist = [&coexisting](const AddonId& one, const AddonId& other)
    {
        return std::ranges::any_of(coexisting,
                                   [&one, &other](const CoexistingPair& marked)
                                   {
                                       return ItIsTheSamePair(marked, {.one = one, .other = other});
                                   });
    };

    std::vector<AirportPair> pairs;

    for (const AirportGroup& group : GroupsOfTheSameAirport(addons))
    {
        for (std::size_t one = 0; one < group.addons.size(); ++one)
        {
            for (std::size_t other = one + 1; other < group.addons.size(); ++other)
            {
                if (theUserSaidTheyCanCoexist(group.addons[one], group.addons[other]))
                {
                    continue;
                }

                pairs.push_back({.code = group.code, .one = group.addons[one], .other = group.addons[other]});
            }
        }
    }

    return pairs;
}

std::vector<AirportTheSimulatorAlsoCovers>
AirportsTheSimulatorAlsoCovers(const std::vector<AirportsOfAnAddon>& addons,
                               const std::vector<SimulatorAirport>& simulator)
{
    std::vector<AirportTheSimulatorAlsoCovers> covered;

    for (const AirportsOfAnAddon& addon : addons)
    {
        for (const std::string& code : addon.codes)
        {
            for (const SimulatorAirport& package : simulator)
            {
                if (package.activated && package.code == code)
                {
                    covered.push_back({.code = code, .addon = addon.addon, .packageName = package.packageName});
                }
            }
        }
    }

    return covered;
}
