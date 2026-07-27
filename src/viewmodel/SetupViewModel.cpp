#include "viewmodel/SetupViewModel.h"

SetupViewModel::SetupViewModel(SetupService& service, QObject* parent) : QObject(parent), service_(service)
{
}

void SetupViewModel::Detect() const
{
    service_.Detect();
}

std::vector<SimulatorCandidate> SetupViewModel::Candidates() const
{
    return service_.Candidates();
}

DestinationCheck SetupViewModel::CheckDestination(const std::filesystem::path& path) const
{
    return service_.CheckDestination(path);
}

void SetupViewModel::AddManualCandidate(const std::filesystem::path& destination, const SimulatorVariant variant) const
{
    service_.AddManualCandidate(destination, variant);
}

void SetupViewModel::ChooseCandidate(const std::size_t index) const
{
    service_.ChooseCandidate(index);
}

LibraryReport SetupViewModel::RegisterLibrary(const std::filesystem::path& path, const std::string& label) const
{
    return service_.RegisterLibrary(path, label);
}

std::vector<RegisteredLibrary> SetupViewModel::Libraries() const
{
    return service_.Libraries();
}

void SetupViewModel::Complete() const
{
    service_.Complete();
}
