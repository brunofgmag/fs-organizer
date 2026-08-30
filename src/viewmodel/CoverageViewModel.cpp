#include "viewmodel/CoverageViewModel.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string>

#include <QtCore/QMetaObject>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    [[nodiscard]] AddonToRead ToRead(const SimulatorProfile& profile, const TreeNode& node)
    {
        return {.addon = IdentityOf(profile, node.path),
                .folder = node.path,
                .itIsNavigationData = node.addon.has_value() && ItDeclaresNavigationData(node.addon->manifest)};
    }

    [[nodiscard]] std::vector<AddonToRead> WhatIsAlreadyOn(const SimulatorProfile& profile,
                                                           const ProfileSnapshot& snapshot,
                                                           const std::vector<AddonToRead>& except)
    {
        std::set<std::string> theseOnes;
        for (const AddonToRead& addon : except)
        {
            theseOnes.insert(ComparablePath(addon.folder));
        }

        std::vector<AddonToRead> on;

        for (const TreeNode& library : snapshot.libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                if (snapshot.enabled.Contains(addon->path) && !theseOnes.contains(ComparablePath(addon->path)))
                {
                    on.push_back(ToRead(profile, *addon));
                }
            }
        }

        return on;
    }

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

    [[nodiscard]] SharedAirportsLine LineOf(const SharedAirports& shared)
    {
        QStringList codes;
        for (const std::string& code : shared.codes)
        {
            codes << QString::fromStdString(code);
        }

        return {.turningOn = QString::fromStdString(shared.turningOn.folderName),
                .alreadyOn = QString::fromStdString(shared.alreadyOn.folderName),
                .codes = codes,
                .one = shared.turningOn,
                .other = shared.alreadyOn};
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
                                     BackgroundRunner& runner,
                                     QObject* parent)
    : QObject(parent), service_(service), scenery_(scenery), session_(session), clock_(clock), checking_(runner)
{
}

void CoverageViewModel::CheckWhatWasTurnedOn(const std::vector<const TreeNode*>& nodes)
{
    if (nodes.empty())
    {
        return;
    }

    const SimulatorProfile& profile = session_.Profile();

    std::vector<AddonToRead> turningOn;
    turningOn.reserve(nodes.size());

    for (const TreeNode* node : nodes)
    {
        turningOn.push_back(ToRead(profile, *node));
    }

    Check(turningOn);
}

void CoverageViewModel::Check(const std::vector<AddonToRead>& turningOn)
{
    if (checking_.Busy())
    {
        waiting_.insert(waiting_.end(), turningOn.begin(), turningOn.end());
        return;
    }

    const std::vector<AddonToRead> alreadyOn = WhatIsAlreadyOn(session_.Profile(), session_.Snapshot(), turningOn);
    const std::vector<CoexistingPair> coexisting = session_.Settings().coexistingAirports;
    const bool managing = service_.Managing();

    stopChecking_ = false;

    auto found = std::make_shared<WhatTurningThemOnFound>();

    checking_.Run(
        [this, turningOn, alreadyOn, coexisting, managing, found]
        {
            const std::vector<SceneryOfAnAddon> read = scenery_.SceneryOfEach(turningOn, TellingHowFarItGot());

            if (stopChecking_)
            {
                return;
            }

            const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon(read);

            if (managing)
            {
                for (const AirportTheSimulatorAlsoCovers& covered : service_.WhatTheSimulatorAlsoCovers(airports))
                {
                    found->alsoCovered.push_back(LineOf(covered));
                }
            }

            for (const SharedAirports& shared : PairsWithWhatIsAlreadyOn(
                     airports, AirportsOfEachAddon(scenery_.WhatIsAlreadyKnown(alreadyOn)), coexisting))
            {
                found->shared.push_back(LineOf(shared));
            }
        },
        [this, found]
        {
            TheAnswerCameBack(*found);
        });
}

SceneryProgress CoverageViewModel::TellingHowFarItGot()
{
    return [this](const std::size_t done, const std::size_t outOf)
    {
        QMetaObject::invokeMethod(this,
                                  [this, done, outOf]
                                  {
                                      emit CheckProgressed(static_cast<int>(done), static_cast<int>(outOf));
                                  });

        return !stopChecking_;
    };
}

void CoverageViewModel::TheAnswerCameBack(const WhatTurningThemOnFound& found)
{
    emit TurningThemOnWasChecked(found);

    if (waiting_.empty())
    {
        return;
    }

    const std::vector<AddonToRead> next = std::move(waiting_);
    waiting_.clear();

    Check(next);
}

void CoverageViewModel::StopChecking()
{
    stopChecking_ = true;
}

bool CoverageViewModel::Checking() const
{
    return checking_.Busy();
}

void CoverageViewModel::Show()
{
    Read();

    emit Changed();
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

FileResult CoverageViewModel::SwitchAll(const std::vector<std::string>& packageNames, const bool activated)
{
    const FileResult result = service_.SwitchAll(packageNames, activated);
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
    TheyCanAllCoexist({{.one = one, .other = other}});
}

void CoverageViewModel::TheyCanAllCoexist(const std::vector<CoexistingPair>& pairs)
{
    const bool written = session_.Rewrite(
        [&pairs](AppSettings& settings)
        {
            for (const CoexistingPair& marked : pairs)
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
