#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H

#include <filesystem>
#include <mutex>
#include <optional>
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

    [[nodiscard]] FileResult SwitchAll(const std::vector<std::string>& packageNames, bool activated) override;

private:
    std::filesystem::path listPath_;
    mutable std::mutex guard_;
    mutable std::optional<std::filesystem::file_time_type> parsedAt_;
    mutable std::vector<PackageEntry> parsed_;
};

[[nodiscard]] std::filesystem::path BackupOfPackageList(const std::filesystem::path& listPath);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGE_LIST_H
