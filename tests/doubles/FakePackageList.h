#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_PACKAGE_LIST_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_PACKAGE_LIST_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "application/ports/PackageList.h"

class FakePackageList final : public PackageList
{
public:
    void Carry(const std::string& name, const PackageActivation activation)
    {
        entries_.push_back({.name = name, .activation = activation});
    }

    void CarryAnAirport(const std::string& name, const std::string& code, const PackageActivation activation)
    {
        Carry(name, activation);

        airports_.push_back(
            {.packageName = name, .code = code, .activated = activation == PackageActivation::Activated});
    }

    [[nodiscard]] std::vector<PackageEntry> Entries() const override
    {
        return entries_;
    }

    [[nodiscard]] std::vector<SimulatorAirport> AirportsTheSimulatorShips() const override
    {
        std::vector<SimulatorAirport> shipped = airports_;

        for (SimulatorAirport& airport : shipped)
        {
            airport.activated = ActivationOf(airport.packageName) == PackageActivation::Activated;
        }

        return shipped;
    }

    [[nodiscard]] FileResult Switch(const std::string_view packageName, const bool activated) override
    {
        switched.emplace_back(std::string(packageName), activated);

        const auto entry = std::ranges::find(entries_, packageName, &PackageEntry::name);
        if (entry == entries_.end())
        {
            return FileResult::TheDiskDisagreesWithTheScan;
        }

        entry->activation = activated ? PackageActivation::Activated : PackageActivation::UserDisabled;

        return answer;
    }

    [[nodiscard]] FileResult SwitchAll(const std::vector<std::string>& packageNames, const bool activated) override
    {
        ++batches;

        for (const std::string& packageName : packageNames)
        {
            if (const FileResult result = Switch(packageName, activated); !Succeeded(result))
            {
                return result;
            }
        }

        return answer;
    }

    std::vector<std::pair<std::string, bool>> switched;
    std::size_t batches = 0;
    FileResult answer = FileResult::Completed;

private:
    [[nodiscard]] PackageActivation ActivationOf(const std::string& name) const
    {
        const auto entry = std::ranges::find(entries_, name, &PackageEntry::name);

        return entry == entries_.end() ? PackageActivation::ItSaysSomethingElse : entry->activation;
    }

    std::vector<PackageEntry> entries_;
    std::vector<SimulatorAirport> airports_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_PACKAGE_LIST_H
