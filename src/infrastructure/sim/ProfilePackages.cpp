#include "infrastructure/sim/ProfilePackages.h"

#include <utility>

ProfilePackages::ProfilePackages(const FilesystemProbe& filesystemProbe,
                                 std::vector<ContentListLocation> locations,
                                 std::function<std::vector<ContentListLocation>()> locateAgain)
    : filesystemProbe_(filesystemProbe),
      locations_(std::move(locations)),
      locateAgain_(std::move(locateAgain)),
      read_(filesystemProbe, {})
{
}

void ProfilePackages::Reload(const SimulatorVariant variant)
{
    std::optional<ChosenContentList> chosen = ChooseContentList(locations_, variant);

    if (!chosen.has_value() && locateAgain_)
    {
        locations_ = locateAgain_();
        chosen = ChooseContentList(locations_, variant);
    }

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
