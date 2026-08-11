#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGES_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGES_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>

#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/SimulatorPackages.h"

class ContentXmlPackages final : public SimulatorPackages
{
public:
    ContentXmlPackages(const FilesystemProbe& filesystemProbe, std::filesystem::path listPath);

    void ReadAgain(std::filesystem::path listPath);

    [[nodiscard]] PackagePresence PresenceOf(std::string_view packageName) const override;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ListTakenAt() const override;

    [[nodiscard]] std::string ListAccountFolder() const override;

private:
    void Forget(std::filesystem::path listPath);

    const FilesystemProbe& filesystemProbe_;
    std::filesystem::path listPath_;
    std::set<std::string, std::less<>> names_;
    std::optional<std::chrono::system_clock::time_point> takenAt_;
    std::size_t entries_ = 0;
    bool listWasRead_ = false;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_PACKAGES_H
