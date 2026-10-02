#include "application/CoverageService.h"

#include <algorithm>

CoverageService::CoverageService(PackageList& packages,
                                 const ProcessProbe& processProbe,
                                 const OperationLog& log,
                                 const bool managing)
    : packages_(packages), processProbe_(processProbe), log_(log), managing_(managing)
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

FileResult CoverageService::Refusal() const
{
    if (!managing_)
    {
        return FileResult::ThePackageListIsLeftLoose;
    }

    return processProbe_.SimulatorIsRunning() ? FileResult::TheSimulatorIsRunning : FileResult::Completed;
}

void CoverageService::Record(const std::vector<std::string>& packageNames,
                             const bool activated,
                             const FileResult result) const
{
    const OperationKind kind =
        activated ? OperationKind::TurnOnTheSimulatorPackage : OperationKind::TurnOffTheSimulatorPackage;

    for (const std::string& name : packageNames)
    {
        log_.RecordImport(kind, AddonId{}, {}, {}, result, OriginSource::Unknown, name);
    }
}

FileResult CoverageService::Switch(const std::string_view packageName, const bool activated)
{
    const FileResult refusal = Refusal();
    const FileResult result = refusal == FileResult::Completed ? packages_.Switch(packageName, activated) : refusal;

    Record({std::string(packageName)}, activated, result);

    return result;
}

FileResult CoverageService::SwitchAll(const std::vector<std::string>& packageNames, const bool activated)
{
    const FileResult refusal = Refusal();
    const FileResult result = refusal == FileResult::Completed ? packages_.SwitchAll(packageNames, activated) : refusal;

    Record(packageNames, activated, result);

    return result;
}
