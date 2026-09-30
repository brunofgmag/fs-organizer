#ifndef FS_ORGANIZER_TOOLS_TIMING_SESSION_FOR_MEASURING_H
#define FS_ORGANIZER_TOOLS_TIMING_SESSION_FOR_MEASURING_H

#include <atomic>
#include <chrono>
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

    [[nodiscard]] const AppSettings& Stored() const
    {
        return stored_;
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

class TimedRunner final : public BackgroundRunner
{
public:
    explicit TimedRunner(BackgroundRunner& inner) : inner_(inner)
    {
    }

    void Run(std::function<void()> work, std::function<void()> doneOnTheCallingThread) override
    {
        inner_.Run(
            [this, work = std::move(work)]
            {
                const auto began = std::chrono::steady_clock::now();

                work();

                worked_ = MillisecondsSince(began);
            },
            [this, done = std::move(doneOnTheCallingThread)]
            {
                const auto began = std::chrono::steady_clock::now();

                done();

                adopted_ = MillisecondsSince(began);
                ++landed_;
            });
    }

    [[nodiscard]] double LastWorkMilliseconds() const
    {
        return worked_;
    }

    [[nodiscard]] double LastAdoptionMilliseconds() const
    {
        return adopted_;
    }

    [[nodiscard]] int Landed() const
    {
        return landed_;
    }

private:
    [[nodiscard]] static double MillisecondsSince(const std::chrono::steady_clock::time_point began)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    }

    BackgroundRunner& inner_;
    std::atomic<double> worked_ = 0;
    std::atomic<double> adopted_ = 0;
    std::atomic<int> landed_ = 0;
};

class NoLibrariesToScan final : public CatalogScanner
{
public:
    [[nodiscard]] TreeNode ScanWhile(const std::filesystem::path&, const ScanGate&) const override
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
