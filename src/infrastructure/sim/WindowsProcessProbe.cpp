#include "infrastructure/sim/WindowsProcessProbe.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    bool NamesMatch(const std::string& left, const std::string& right)
    {
        return std::ranges::equal(left, right,
                                  [](const unsigned char a, const unsigned char b)
                                  {
                                      return std::tolower(a) == std::tolower(b);
                                  });
    }
}

WindowsProcessProbe::WindowsProcessProbe(std::vector<std::string> executableNames)
    : executableNames_(std::move(executableNames))
{
}

std::optional<std::string> WindowsProcessProbe::RunningSimulator() const
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    std::optional<std::string> running;
    for (BOOL more = Process32FirstW(snapshot, &entry); more != FALSE && !running.has_value();
         more = Process32NextW(snapshot, &entry))
    {
        const std::string name = std::filesystem::path(entry.szExeFile).filename().string();
        const auto match = std::ranges::find_if(executableNames_,
                                                [&name](const std::string& candidate)
                                                {
                                                    return NamesMatch(name, candidate);
                                                });

        if (match != executableNames_.end())
        {
            running = *match;
        }
    }

    CloseHandle(snapshot);

    return running;
}
