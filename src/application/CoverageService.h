#ifndef FS_ORGANIZER_APPLICATION_COVERAGE_SERVICE_H
#define FS_ORGANIZER_APPLICATION_COVERAGE_SERVICE_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/PackageList.h"
#include "application/ports/ProcessProbe.h"
#include "domain/model/FileResult.h"
#include "domain/scenery/AirportCoverage.h"

struct TurnedOffPackage
{
    std::string name{};
    std::string code{};
};

class CoverageService
{
public:
    CoverageService(PackageList& packages, const ProcessProbe& processProbe, bool managing);

    void Manage(bool managing);

    [[nodiscard]] bool Managing() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] std::vector<TurnedOffPackage> TurnedOff() const;

    [[nodiscard]] std::vector<AirportTheSimulatorAlsoCovers>
    WhatTheSimulatorAlsoCovers(const std::vector<AirportsOfAnAddon>& addons) const;

    [[nodiscard]] FileResult Switch(std::string_view packageName, bool activated);

private:
    PackageList& packages_;
    const ProcessProbe& processProbe_;
    bool managing_;
};

#endif // FS_ORGANIZER_APPLICATION_COVERAGE_SERVICE_H
