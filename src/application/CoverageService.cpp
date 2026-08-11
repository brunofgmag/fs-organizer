#include "application/CoverageService.h"

#include <algorithm>

CoverageService::CoverageService(PackageList& packages, const ProcessProbe& processProbe, const bool managing)
    : packages_(packages), processProbe_(processProbe), managing_(managing)
{
}

void CoverageService::Manage(const bool managing)
{
    managing_ = managing;
}

bool CoverageService::Managing() const
{
    return managing_;
}

std::optional<std::string> CoverageService::RunningSimulator() const
{
    return processProbe_.RunningSimulator();
}

std::vector<TurnedOffPackage> CoverageService::TurnedOff() const
{
    if (!managing_)
    {
        return {};
    }

    const std::vector<SimulatorAirport> airports = packages_.AirportsTheSimulatorShips();

    std::vector<TurnedOffPackage> turnedOff;

    for (const PackageEntry& entry : packages_.Entries())
    {
        if (entry.activation != PackageActivation::UserDisabled)
        {
            continue;
        }

        const auto airport = std::ranges::find(airports, entry.name, &SimulatorAirport::packageName);

        turnedOff.push_back({.name = entry.name, .code = airport == airports.end() ? std::string{} : airport->code});
    }

    return turnedOff;
}

std::vector<AirportTheSimulatorAlsoCovers>
CoverageService::WhatTheSimulatorAlsoCovers(const std::vector<AirportsOfAnAddon>& addons) const
{
    if (!managing_)
    {
        return {};
    }

    return AirportsTheSimulatorAlsoCovers(addons, packages_.AirportsTheSimulatorShips());
}

FileResult CoverageService::Switch(const std::string_view packageName, const bool activated)
{
    if (!managing_)
    {
        return FileResult::ThePackageListIsLeftLoose;
    }

    if (processProbe_.SimulatorIsRunning())
    {
        return FileResult::TheSimulatorIsRunning;
    }

    return packages_.Switch(packageName, activated);
}
