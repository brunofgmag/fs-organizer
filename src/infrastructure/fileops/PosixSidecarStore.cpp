#include "infrastructure/fileops/PosixSidecarStore.h"

#include <fstream>
#include <system_error>

#include "support/FileReading.h"

bool PosixSidecarStore::Write(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }

    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();

    return !file.fail();
}

std::optional<std::string> PosixSidecarStore::Read(const std::filesystem::path& path) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    return BytesRemainingIn(file);
}

bool PosixSidecarStore::Forget(const std::filesystem::path& path)
{
    std::error_code error;

    return std::filesystem::remove(path, error) && !error;
}

std::vector<std::filesystem::path> PosixSidecarStore::FilesIn(const std::filesystem::path& folder) const
{
    std::vector<std::filesystem::path> files;
    std::error_code error;

    for (std::filesystem::directory_iterator entries(folder, error), end; !error && entries != end;
         entries.increment(error))
    {
        std::error_code kindError;
        if (entries->is_regular_file(kindError) && !kindError)
        {
            files.push_back(folder / entries->path().filename());
        }
    }

    return files;
}
