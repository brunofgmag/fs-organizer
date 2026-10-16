#include "infrastructure/sim/ProcProcessProbe.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <utility>

#include "support/FileReading.h"

namespace
{
    constexpr auto kSeparators = "/\\";

    bool NamesMatch(const std::string_view left, const std::string_view right)
    {
        return std::ranges::equal(left, right,
                                  [](const unsigned char a, const unsigned char b)
                                  {
                                      return std::tolower(a) == std::tolower(b);
                                  });
    }

    bool IsAProcessFolder(const std::string& name)
    {
        return !name.empty()
            && std::ranges::all_of(name,
                                   [](const unsigned char letter)
                                   {
                                       return std::isdigit(letter) != 0;
                                   });
    }

    std::vector<std::string> ArgumentsOf(const std::filesystem::path& cmdline)
    {
        std::ifstream stream(cmdline, std::ios::binary);
        if (!stream.is_open())
        {
            return {};
        }

        const std::optional<std::string> bytes = BytesRemainingIn(stream);
        if (!bytes.has_value())
        {
            return {};
        }

        std::vector<std::string> arguments;
        std::size_t from = 0;

        while (from < bytes->size())
        {
            const std::size_t end = bytes->find('\0', from);
            const std::size_t stop = end == std::string::npos ? bytes->size() : end;

            arguments.push_back(bytes->substr(from, stop - from));
            from = stop + 1;
        }

        return arguments;
    }

    bool RunsProgramNamed(const std::vector<std::string>& arguments, const std::string& executable)
    {
        for (std::size_t at = 0; at < arguments.size(); ++at)
        {
            const std::string& argument = arguments[at];
            const std::size_t separator = argument.find_last_of(kSeparators);

            if (at > 0 && separator == std::string::npos)
            {
                continue;
            }

            const std::size_t nameAt = separator == std::string::npos ? 0 : separator + 1;

            if (NamesMatch(std::string_view(argument).substr(nameAt), executable))
            {
                return true;
            }
        }

        return false;
    }

    bool ComesBefore(const std::string& left, const std::string& right)
    {
        if (left.size() != right.size())
        {
            return left.size() < right.size();
        }

        return left < right;
    }

    std::vector<std::string> OtherProcessesIn(const std::filesystem::path& procFolder)
    {
        const std::string own = std::to_string(::getpid());
        std::vector<std::string> processes;
        std::error_code error;

        for (std::filesystem::directory_iterator entries(procFolder, error), end; !error && entries != end;
             entries.increment(error))
        {
            std::string name = entries->path().filename().string();
            if (IsAProcessFolder(name) && name != own)
            {
                processes.push_back(std::move(name));
            }
        }

        std::ranges::sort(processes, ComesBefore);
        return processes;
    }
}

ProcProcessProbe::ProcProcessProbe(std::vector<std::string> executableNames, std::filesystem::path procFolder)
    : executableNames_(std::move(executableNames)), procFolder_(std::move(procFolder))
{
}

std::optional<std::string> ProcProcessProbe::RunningSimulator() const
{
    if (executableNames_.empty())
    {
        return std::nullopt;
    }

    for (const std::string& process : OtherProcessesIn(procFolder_))
    {
        const std::vector<std::string> arguments = ArgumentsOf(procFolder_ / process / "cmdline");

        for (const std::string& executable : executableNames_)
        {
            if (RunsProgramNamed(arguments, executable))
            {
                return executable;
            }
        }
    }

    return std::nullopt;
}
