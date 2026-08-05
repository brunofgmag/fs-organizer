#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_PACKAGES_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_PACKAGES_H

#include <chrono>
#include <functional>
#include <optional>
#include <set>
#include <string>

#include "domain/ports/SimulatorPackages.h"

class FakeSimulatorPackages final : public SimulatorPackages
{
public:
    void ReportAsInstalled(const std::string& packageName)
    {
        installed_.insert(packageName);
    }

    void ReportTheListAsTakenAt(const std::chrono::system_clock::time_point& moment)
    {
        takenAt_ = moment;
    }

    [[nodiscard]] PackagePresence PresenceOf(const std::string_view packageName) const override
    {
        if (installed_.empty())
        {
            return PackagePresence::Unverifiable;
        }

        return installed_.contains(packageName) ? PackagePresence::Present : PackagePresence::Absent;
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ListTakenAt() const override
    {
        return takenAt_;
    }

private:
    std::set<std::string, std::less<>> installed_;
    std::optional<std::chrono::system_clock::time_point> takenAt_;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_SIMULATOR_PACKAGES_H
