#ifndef FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>

#include "application/Session.h"
#include "application/StartupService.h"
#include "application/ports/SettingsRepository.h"
#include "domain/ports/Clock.h"

class StartupViewModel final : public QObject
{
    Q_OBJECT

public:
    StartupViewModel(StartupService& service,
                     Session& session,
                     SettingsRepository& settings,
                     const Clock& clock,
                     QObject* parent = nullptr);

    void Show();

    [[nodiscard]] bool Managing() const;

    void Manage(bool managing);

    [[nodiscard]] const std::vector<StartupLine>& Lines() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ReadAt() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, bool enabled);

signals:
    void Changed();

    void SettingsCouldNotBeSaved();

private:
    void Read();

    StartupService& service_;
    Session& session_;
    SettingsRepository& settings_;
    const Clock& clock_;
    StartupReport report_;
    std::optional<std::chrono::system_clock::time_point> readAt_;
};

#endif // FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H
