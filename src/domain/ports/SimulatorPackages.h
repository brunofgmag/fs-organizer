#ifndef FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_PACKAGES_H
#define FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_PACKAGES_H

#include <chrono>
#include <optional>
#include <string_view>

#include "domain/model/PackagePresence.h"

class SimulatorPackages
{
public:
    virtual ~SimulatorPackages() = default;

    [[nodiscard]] virtual PackagePresence PresenceOf(std::string_view packageName) const = 0;

    [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point> ListTakenAt() const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_SIMULATOR_PACKAGES_H
