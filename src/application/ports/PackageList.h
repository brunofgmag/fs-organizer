#ifndef FS_ORGANIZER_APPLICATION_PORTS_PACKAGE_LIST_H
#define FS_ORGANIZER_APPLICATION_PORTS_PACKAGE_LIST_H

#include <string>
#include <string_view>
#include <vector>

#include "domain/model/FileResult.h"
#include "domain/scenery/AirportCoverage.h"

enum class PackageActivation : int
{
    Activated = 0,
    UserDisabled = 1,
    SystemDisabled = 2,
    ItSaysSomethingElse = 3,
};

struct PackageEntry
{
    std::string name{};
    PackageActivation activation = PackageActivation::Activated;
};

class PackageList
{
public:
    virtual ~PackageList() = default;

    [[nodiscard]] virtual std::vector<PackageEntry> Entries() const = 0;

    [[nodiscard]] virtual std::vector<SimulatorAirport> AirportsTheSimulatorShips() const = 0;

    [[nodiscard]] virtual FileResult Switch(std::string_view packageName, bool activated) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_PACKAGE_LIST_H
