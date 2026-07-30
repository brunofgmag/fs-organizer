#ifndef FS_ORGANIZER_TOOLS_TIMING_SESSION_FOR_MEASURING_H
#define FS_ORGANIZER_TOOLS_TIMING_SESSION_FOR_MEASURING_H

#include <filesystem>
#include <functional>
#include <utility>

#include "application/model/AppSettings.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/SessionObserver.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/TreeNode.h"
#include "domain/ports/CatalogScanner.h"

class OneProfileRepository final : public SettingsRepository
{
public:
    explicit OneProfileRepository(SimulatorProfile profile)
    {
        stored_.activeProfileId = profile.id;
        stored_.profiles = {std::move(profile)};
    }

    [[nodiscard]] std::optional<AppSettings> Load() const override
    {
        return stored_;
    }

    [[nodiscard]] bool Save(const AppSettings& settings) override
    {
        stored_ = settings;

        return true;
    }

private:
    AppSettings stored_;
};

class InlineRunner final : public BackgroundRunner
{
public:
    void Run(const std::function<void()> work, const std::function<void()> doneOnTheCallingThread) override
    {
        work();
        doneOnTheCallingThread();
    }
};

class NoLibrariesToScan final : public CatalogScanner
{
public:
    [[nodiscard]] TreeNode Scan(const std::filesystem::path&) const override
    {
        return {};
    }
};

class SilentObserver final : public SessionObserver
{
public:
    void OnScanStarted() override
    {
    }

    void OnScanFinished() override
    {
    }

    void OnRefreshed() override
    {
    }

    void OnSettingsCouldNotBeSaved() override
    {
    }

    void OnSimulatorIsRunning() override
    {
    }

    void OnRestartPendingChanged(bool) override
    {
    }
};

#endif // FS_ORGANIZER_TOOLS_TIMING_SESSION_FOR_MEASURING_H
