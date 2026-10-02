#ifndef FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>

#include "application/Session.h"
#include "application/StartupEditor.h"
#include "application/StartupService.h"
#include "domain/ports/Clock.h"

class StartupViewModel final : public QObject
{
    Q_OBJECT

public:
    StartupViewModel(StartupService& service,
                     StartupEditor& editor,
                     Session& session,
                     const Clock& clock,
                     QObject* parent = nullptr);

    void Show();

    [[nodiscard]] bool Managing() const;

    void Manage(bool managing);

    [[nodiscard]] const std::vector<StartupLine>& Lines() const;

    [[nodiscard]] const std::vector<StartupRemovedEntry>& Removed() const;

    [[nodiscard]] std::optional<StartupUndoPlan> UndoPlan() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ReadAt() const;

    [[nodiscard]] std::optional<std::string> RunningSimulator() const;

    [[nodiscard]] StartupDraftCheck Check(const std::filesystem::path& chosenFile,
                                          const std::optional<std::filesystem::path>& editedEntry) const;

    [[nodiscard]] FileResult Switch(const std::filesystem::path& entryPath, bool enabled);

    [[nodiscard]] StartupGestureOutcome Add(const StartupDraft& draft);

    [[nodiscard]] StartupGestureOutcome Edit(const std::filesystem::path& entryPath, const StartupDraft& draft);

    [[nodiscard]] StartupGestureOutcome Remove(const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome Restore(const std::filesystem::path& entryPath);

    [[nodiscard]] StartupGestureOutcome Discard(const std::filesystem::path& entryPath);

    [[nodiscard]] std::optional<StartupGestureOutcome> Undo();

signals:
    void Changed();

    void SettingsCouldNotBeSaved();

private:
    void Read();

    [[nodiscard]] StartupGestureOutcome Settled(StartupGestureOutcome outcome);

    StartupService& service_;
    StartupEditor& editor_;
    Session& session_;
    const Clock& clock_;
    StartupReport report_;
    std::vector<StartupRemovedEntry> removed_;
    std::optional<std::chrono::system_clock::time_point> readAt_;
};

#endif // FS_ORGANIZER_VIEWMODEL_STARTUP_VIEW_MODEL_H
