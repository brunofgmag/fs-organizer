#include "application/LoadReport.h"

#include <algorithm>

#include "domain/support/PathUtils.h"
#include "domain/support/StringUtils.h"
#include "domain/tree/AddonTree.h"

namespace
{
    [[nodiscard]] std::filesystem::path AddonCalled(const std::string& folderName, const ProfileSnapshot& snapshot)
    {
        for (const TreeNode& library : snapshot.libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                if (EqualsIgnoringCase(AsUtf8(addon->path.filename()), folderName))
                {
                    return addon->path.lexically_relative(library.path);
                }
            }
        }

        return {};
    }

    [[nodiscard]] bool TheHeavierOf(const ModuleLine& one, const ModuleLine& other)
    {
        if (one.memoryBytes.has_value() != other.memoryBytes.has_value())
        {
            return one.memoryBytes.has_value();
        }

        return one.memoryBytes.value_or(0) > other.memoryBytes.value_or(0);
    }
}

LoadDiagnostics ReportTheLoad(const std::optional<LoadingReport>& report, const ProfileSnapshot& snapshot)
{
    if (!report.has_value())
    {
        return {};
    }

    LoadDiagnostics reported{
        .packagesRegistered = report->packagesRegistered, .runAt = report->runAt, .reportWasRead = true};

    for (const LoadedModule& module : report->modules)
    {
        reported.modules.push_back({.moduleName = module.moduleName,
                                    .packageName = module.packageName,
                                    .memoryBytes = module.memoryBytes,
                                    .addonUnderLibrary = AddonCalled(module.packageFolderName, snapshot)});
    }

    std::ranges::stable_sort(reported.modules, TheHeavierOf);

    return reported;
}
