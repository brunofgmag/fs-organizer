#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H

#include <filesystem>
#include <string_view>
#include <vector>

#include "application/ports/PackageList.h"

class ContentXmlPackageList final : public PackageList
{
public:
    explicit ContentXmlPackageList(std::filesystem::path listPath);

    void Use(std::filesystem::path listPath);

    [[nodiscard]] std::vector<PackageEntry> Entries() const override;

    [[nodiscard]] std::vector<SimulatorAirport> AirportsTheSimulatorShips() const override;

    [[nodiscard]] FileResult Switch(std::string_view packageName, bool activated) override;

private:
    std::filesystem::path listPath_;
};

[[nodiscard]] std::filesystem::path BackupOfPackageList(const std::filesystem::path& listPath);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H
