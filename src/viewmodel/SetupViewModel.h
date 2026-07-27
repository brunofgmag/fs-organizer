#ifndef FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <QtCore/QObject>

#include "application/SetupService.h"

class SetupViewModel final : public QObject
{
    Q_OBJECT

public:
    explicit SetupViewModel(SetupService& service, QObject* parent = nullptr);

    void Detect() const;

    [[nodiscard]] std::vector<SimulatorCandidate> Candidates() const;

    [[nodiscard]] DestinationCheck CheckDestination(const std::filesystem::path& path) const;

    void AddManualCandidate(const std::filesystem::path& destination, SimulatorVariant variant) const;

    void ChooseCandidate(std::size_t index) const;

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path, const std::string& label) const;

    [[nodiscard]] std::vector<RegisteredLibrary> Libraries() const;

    void Complete() const;

private:
    SetupService& service_;
};

#endif // FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H
