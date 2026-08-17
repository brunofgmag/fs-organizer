#ifndef FS_ORGANIZER_DOMAIN_SCENERY_AIRPORT_COVERAGE_H
#define FS_ORGANIZER_DOMAIN_SCENERY_AIRPORT_COVERAGE_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/AddonId.h"
#include "domain/model/Manifest.h"
#include "domain/model/SceneryCodes.h"

struct SceneryOfAnAddon
{
    AddonId addon{};
    std::filesystem::path resolvedPath{};
    std::vector<SceneryCodes> files{};
    bool itIsNavigationData = false;
};

enum class AirportEvidence : int
{
    ItCarriesNoAirportRecord = 0,
    ARecordWasNotRead = 1,
    TheCodeWasRead = 2,
    ItIsNavigationData = 3,
};

struct AirportsOfAnAddon
{
    AddonId addon{};
    AirportEvidence evidence = AirportEvidence::ItCarriesNoAirportRecord;
    std::vector<std::string> codes{};
};

struct AirportGroup
{
    std::string code{};
    std::vector<AddonId> addons{};
};

struct CoexistingPair
{
    AddonId one{};
    AddonId other{};
};

struct AirportPair
{
    std::string code{};
    AddonId one{};
    AddonId other{};
};

struct SharedAirports
{
    AddonId turningOn{};
    AddonId alreadyOn{};
    std::vector<std::string> codes{};
};

struct SimulatorAirport
{
    std::string packageName{};
    std::string code{};
    bool activated = true;
};

struct AirportTheSimulatorAlsoCovers
{
    std::string code{};
    AddonId addon{};
    std::string packageName{};
};

[[nodiscard]] bool ItDeclaresNavigationData(const Manifest& manifest);

[[nodiscard]] std::vector<AirportsOfAnAddon> AirportsOfEachAddon(const std::vector<SceneryOfAnAddon>& scenery);

[[nodiscard]] std::vector<AirportGroup> GroupsOfTheSameAirport(const std::vector<AirportsOfAnAddon>& addons);

[[nodiscard]] bool ItIsTheSamePair(const CoexistingPair& left, const CoexistingPair& right);

[[nodiscard]] std::vector<AirportPair> PairsOfTheSameAirport(const std::vector<AirportsOfAnAddon>& addons,
                                                             const std::vector<CoexistingPair>& coexisting);

[[nodiscard]] std::vector<SharedAirports> PairsWithWhatIsAlreadyOn(const std::vector<AirportsOfAnAddon>& turningOn,
                                                                   const std::vector<AirportsOfAnAddon>& alreadyOn,
                                                                   const std::vector<CoexistingPair>& coexisting);

[[nodiscard]] std::vector<AirportTheSimulatorAlsoCovers>
AirportsTheSimulatorAlsoCovers(const std::vector<AirportsOfAnAddon>& addons,
                               const std::vector<SimulatorAirport>& simulator);

#endif // FS_ORGANIZER_DOMAIN_SCENERY_AIRPORT_COVERAGE_H
