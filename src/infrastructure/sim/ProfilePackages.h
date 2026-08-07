#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_PACKAGES_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_PACKAGES_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/SimulatorPackages.h"
#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ContentXmlPackages.h"

class ProfilePackages final : public SimulatorPackages
{
public:
    ProfilePackages(const FilesystemProbe& filesystemProbe, std::vector<ContentListLocation> locations);

    void Reload(SimulatorVariant variant);

    [[nodiscard]] PackagePresence PresenceOf(std::string_view packageName) const override;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ListTakenAt() const override;

    [[nodiscard]] std::string ListAccountFolder() const override;

private:
    const FilesystemProbe& filesystemProbe_;
    std::vector<ContentListLocation> locations_;
    std::string accountFolder_;
    ContentXmlPackages read_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_PROFILE_PACKAGES_H
