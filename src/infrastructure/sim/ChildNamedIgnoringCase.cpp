#include "infrastructure/sim/ChildNamedIgnoringCase.h"

#include <system_error>

#include "domain/support/PathUtils.h"

std::optional<std::filesystem::path> ChildNamedIgnoringCase(const std::filesystem::path& folder,
                                                            const std::string& name)
{
    std::error_code error;

    if (std::filesystem::exists(std::filesystem::symlink_status(folder / PathFromUtf8(name), error)))
    {
        return folder / PathFromUtf8(name);
    }

    const std::string wanted = LoweredForComparison(name);

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error))
    {
        if (LoweredForComparison(AsUtf8(entry.path().filename())) == wanted)
        {
            return entry.path();
        }
    }

    return std::nullopt;
}
