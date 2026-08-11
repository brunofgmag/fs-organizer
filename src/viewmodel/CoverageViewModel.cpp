#include "viewmodel/CoverageViewModel.h"

#include <algorithm>

#include "domain/tree/LibraryLookup.h"

namespace
{
    [[nodiscard]] CoverageLine LineOf(const AirportPair& pair)
    {
        return {.code = QString::fromStdString(pair.code),
                .covered = QString::fromStdString(pair.one.folderName),
                .andBy = QString::fromStdString(pair.other.folderName),
                .againstTheSimulator = false,
                .packageName = {},
                .one = pair.one,
                .other = pair.other};
    }

    [[nodiscard]] CoverageLine LineOf(const AirportTheSimulatorAlsoCovers& covered)
    {
        return {.code = QString::fromStdString(covered.code),
                .covered = QString::fromStdString(covered.addon.folderName),
                .andBy = QString::fromStdString(covered.packageName),
                .againstTheSimulator = true,
                .packageName = covered.packageName,
                .one = covered.addon,
                .other = {}};
    }
}

CoverageViewModel::CoverageViewModel(CoverageService& service,
                                     SceneryService& scenery,
                                     Session& session,
                                     const Clock& clock,
                                     QObject* parent)
    : QObject(parent), service_(service), scenery_(scenery), session_(session), clock_(clock)
{
}

void CoverageViewModel::Show()
{
    Read();

    emit Changed();
}

std::vector<CoverageLine> CoverageViewModel::WhatTheSimulatorAlsoCovers(const std::vector<const TreeNode*>& nodes)
{
    if (!service_.Managing())
    {
        return {};
    }

    const SimulatorProfile& profile = session_.Profile();

    std::vector<SceneryOfAnAddon> scenery;
    scenery.reserve(nodes.size());

    for (const TreeNode* node : nodes)
    {
        scenery.push_back(scenery_.SceneryOf({.addon = IdentityOf(profile, node->path), .folder = node->path}));
    }

    std::vector<CoverageLine> lines;

    for (const AirportTheSimulatorAlsoCovers& covered :
         service_.WhatTheSimulatorAlsoCovers(AirportsOfEachAddon(scenery)))
    {
        lines.push_back(LineOf(covered));
    }

    return lines;
}

bool CoverageViewModel::Managing() const
{
    return service_.Managing();
}

void CoverageViewModel::Manage(const bool managing)
{
    if (service_.Managing() == managing)
    {
        return;
    }

    const bool written = session_.Rewrite(
        [managing](AppSettings& settings)
        {
            settings.managePackageList = managing;

            return true;
        });

    if (!written)
    {
        emit SettingsCouldNotBeSaved();
        return;
    }

    service_.Manage(managing);
    Read();

    emit Changed();
}

const std::vector<CoverageLine>& CoverageViewModel::Conflicts() const
{
    return conflicts_;
}

const std::vector<TurnedOffLine>& CoverageViewModel::TurnedOff() const
{
    return turnedOff_;
}

std::size_t CoverageViewModel::AddonsWhoseSceneryWasRead() const
{
    return read_;
}

std::size_t CoverageViewModel::AddonsInTheLibraries() const
{
    return addons_;
}

std::optional<std::chrono::system_clock::time_point> CoverageViewModel::ReadAt() const
{
    return readAt_;
}

std::optional<std::string> CoverageViewModel::RunningSimulator() const
{
    return service_.RunningSimulator();
}

FileResult CoverageViewModel::Switch(const std::string& packageName, const bool activated)
{
    const FileResult result = service_.Switch(packageName, activated);
    if (!Succeeded(result))
    {
        return result;
    }

    Read();

    emit Changed();

    return result;
}

void CoverageViewModel::TheyCanCoexist(const AddonId& one, const AddonId& other)
{
    const CoexistingPair marked{.one = one, .other = other};

    const bool written = session_.Rewrite(
        [&marked](AppSettings& settings)
        {
            const bool known = std::ranges::any_of(settings.coexistingAirports,
                                                   [&marked](const CoexistingPair& candidate)
                                                   {
                                                       return ItIsTheSamePair(candidate, marked);
                                                   });

            if (!known)
            {
                settings.coexistingAirports.push_back(marked);
            }

            return true;
        });

    if (!written)
    {
        emit SettingsCouldNotBeSaved();
        return;
    }

    Read();

    emit Changed();
}

void CoverageViewModel::Read()
{
    const std::vector<AddonToRead> addons = SceneryService::AddonsOf(session_.Profile(), session_.Snapshot());
    const std::vector<SceneryOfAnAddon> known = scenery_.WhatIsAlreadyKnown(addons);
    const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon(known);

    addons_ = addons.size();
    read_ = airports.size();

    conflicts_.clear();

    for (const AirportPair& pair : PairsOfTheSameAirport(airports, session_.Settings().coexistingAirports))
    {
        conflicts_.push_back(LineOf(pair));
    }

    for (const AirportTheSimulatorAlsoCovers& covered : service_.WhatTheSimulatorAlsoCovers(airports))
    {
        conflicts_.push_back(LineOf(covered));
    }

    turnedOff_.clear();

    for (const TurnedOffPackage& entry : service_.TurnedOff())
    {
        turnedOff_.push_back({.name = QString::fromStdString(entry.name), .code = QString::fromStdString(entry.code)});
    }

    readAt_ = service_.Managing() ? std::optional(clock_.Now()) : std::nullopt;
}
