#include "infrastructure/fileops/WindowsSidecarStore.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <system_error>

#include "infrastructure/fileops/ExtendedPaths.h"

bool WindowsSidecarStore::Write(const std::filesystem::path& path, const std::string& contents)
{
    const HANDLE file = CreateFileW(WithExtendedPrefix(path).wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD written = 0;
    const bool wrote =
        WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) != FALSE;
    const bool closed = CloseHandle(file) != FALSE;

    return closed && wrote && static_cast<std::size_t>(written) == contents.size();
}

std::optional<std::string> WindowsSidecarStore::Read(const std::filesystem::path& path) const
{
    std::ifstream file(WithExtendedPrefix(path), std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
}

bool WindowsSidecarStore::Forget(const std::filesystem::path& path)
{
    std::error_code error;

    return std::filesystem::remove(WithExtendedPrefix(path), error) && !error;
}
