#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PACKAGE_NAMING_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PACKAGE_NAMING_H

#include <string>
#include <string_view>

[[nodiscard]] bool ItIsContentTheSimulatorShips(std::string_view packageName);

[[nodiscard]] std::string AirportCodeInAPackageName(std::string_view packageName);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PACKAGE_NAMING_H
