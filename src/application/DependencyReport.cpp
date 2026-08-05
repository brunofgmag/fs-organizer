#include "application/DependencyReport.h"

#include "domain/tree/AddonTree.h"

namespace
{
    DependencyAnswer
    AnswerFromTheLibrary(const DeclaredDependency& declared, const TreeNode& addon, const EnabledAddons& enabled)
    {
        return {.name = declared.name,
                .declaredVersion = declared.declaredVersion,
                .resolution = DependencyResolution::InThisLibrary,
                .enabled = enabled.Contains(addon.path),
                .libraryVersion = addon.addon.has_value() ? addon.addon->manifest.packageVersion : std::string()};
    }

    DependencyAnswer AnswerFromTheList(const DeclaredDependency& declared, const PackagePresence presence)
    {
        return {.name = declared.name,
                .declaredVersion = declared.declaredVersion,
                .resolution = presence == PackagePresence::Present ? DependencyResolution::InTheSimulator
                                                                   : DependencyResolution::Unverifiable,
                .enabled = false,
                .libraryVersion = {}};
    }
}

DependencyReport
ReportDependencies(const Addon& addon, const ProfileSnapshot& snapshot, const SimulatorPackages& packages)
{
    DependencyReport report;
    bool theListAnswered = false;

    for (const DeclaredDependency& declared : addon.manifest.dependencies)
    {
        if (const TreeNode* known = AddonNamed(snapshot.libraries, declared.name))
        {
            report.answers.push_back(AnswerFromTheLibrary(declared, *known, snapshot.enabled));
            continue;
        }

        DependencyAnswer answer = AnswerFromTheList(declared, packages.PresenceOf(declared.name));
        theListAnswered = theListAnswered || answer.resolution == DependencyResolution::InTheSimulator;

        report.answers.push_back(std::move(answer));
    }

    if (theListAnswered)
    {
        report.listTakenAt = packages.ListTakenAt();
        report.listAccountFolder = packages.ListAccountFolder();
    }

    return report;
}
