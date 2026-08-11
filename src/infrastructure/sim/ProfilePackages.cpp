#include "infrastructure/sim/ProfilePackages.h"

#include <utility>

ProfilePackages::ProfilePackages(const FilesystemProbe& filesystemProbe, std::vector<ContentListLocation> locations)
    : filesystemProbe_(filesystemProbe), locations_(std::move(locations)), read_(filesystemProbe, {})
{
}

void ProfilePackages::Reload(const SimulatorVariant variant)
{
    const std::optional<ChosenContentList> chosen = ChooseContentList(locations_, variant);

    accountFolder_ = chosen.has_value() ? chosen->accountFolder : std::string();
    read_.ReadAgain(chosen.has_value() ? chosen->listPath : std::filesystem::path{});
}

PackagePresence ProfilePackages::PresenceOf(const std::string_view packageName) const
{
    return read_.PresenceOf(packageName);
}

std::optional<std::chrono::system_clock::time_point> ProfilePackages::ListTakenAt() const
{
    return read_.ListTakenAt();
}

std::string ProfilePackages::ListAccountFolder() const
{
    return accountFolder_;
}
