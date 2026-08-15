#include "domain/bisection/CouplingScan.h"

#include <set>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kModelsFolder = "SimObjects/Airplanes";
    constexpr auto kLiveriesFolder = "liveries";
    constexpr auto kLiveryConfiguration = "livery.cfg";
    constexpr auto kLiveryDescription = "livery.json";
    constexpr auto kBaseContainerKey = "base_container";
    constexpr auto kProductPackageKey = "productPackage";

    [[nodiscard]] bool ItMentions(const std::string& text, const std::string& key)
    {
        return text.find(key) != std::string::npos;
    }

    [[nodiscard]] std::string TheQuotedValueAfter(const std::string& text, const std::string& key)
    {
        const std::size_t named = text.find(key);

        if (named == std::string::npos)
        {
            return {};
        }

        const std::size_t assigned = text.find(':', named + key.size());

        if (assigned == std::string::npos)
        {
            return {};
        }

        const std::size_t opening = text.find('"', assigned);

        if (opening == std::string::npos)
        {
            return {};
        }

        const std::size_t closing = text.find('"', opening + 1);

        if (closing == std::string::npos)
        {
            return {};
        }

        return text.substr(opening + 1, closing - opening - 1);
    }
}

CouplingScan::CouplingScan(const FilesystemProbe& filesystemProbe) : filesystemProbe_(filesystemProbe)
{
}

std::vector<CouplingFacts> CouplingScan::FactsAbout(const std::vector<std::filesystem::path>& addonFolders) const
{
    std::vector<CouplingFacts> facts;
    facts.reserve(addonFolders.size());

    for (const std::filesystem::path& addonFolder : addonFolders)
    {
        facts.push_back(FactsAboutOne(addonFolder));
    }

    return facts;
}

std::vector<SearchUnit> CouplingScan::WithTheKindOfEachGroup(std::vector<SearchUnit> units) const
{
    for (SearchUnit& unit : units)
    {
        unit.coupling = TheKindOf(unit);
    }

    return units;
}

Coupling CouplingScan::TheKindOf(const SearchUnit& unit) const
{
    if (unit.addons.size() < 2)
    {
        return Coupling::Alone;
    }

    if (SomeFileIsClaimedTwice(unit))
    {
        return Coupling::Shadowing;
    }

    return unit.writingApart.empty() ? Coupling::Merge : Coupling::OnlyTheSharedModelFolder;
}

bool CouplingScan::SomeFileIsClaimedTwice(const SearchUnit& unit) const
{
    std::set<std::string> claimed;

    for (const std::filesystem::path& addon : unit.addons)
    {
        const std::optional<TreeFingerprint> walked =
            filesystemProbe_.FingerprintTree(PathUnder(addon, PathFromUtf8(kModelsFolder)));

        if (!walked.has_value())
        {
            continue;
        }

        std::set<std::string> mine;
        for (const FileFingerprint& file : walked->files)
        {
            mine.insert(ComparablePath(file.relativePath));
        }

        for (const std::string& path : mine)
        {
            if (!claimed.insert(path).second)
            {
                return true;
            }
        }
    }

    return false;
}

CouplingFacts CouplingScan::FactsAboutOne(const std::filesystem::path& addonFolder) const
{
    CouplingFacts facts;
    facts.folder = addonFolder;

    const std::filesystem::path models = PathUnder(addonFolder, PathFromUtf8(kModelsFolder));
    const std::size_t partsOfTheAddon = PartsIn(addonFolder);

    for (const std::filesystem::path& modelFolder : filesystemProbe_.ChildDirectories(models))
    {
        facts.modelFolders.push_back(AsUtf8(modelFolder.filename()));

        for (const std::filesystem::path& inside : filesystemProbe_.ChildDirectories(modelFolder))
        {
            facts.writesInside.push_back(TailBelow(inside, partsOfTheAddon));
        }

        ReadTheLiveriesUnder(modelFolder, facts);
    }

    return facts;
}

void CouplingScan::ReadTheLiveriesUnder(const std::filesystem::path& modelFolder, CouplingFacts& facts) const
{
    const std::filesystem::path liveries = PathUnder(modelFolder, PathFromUtf8(kLiveriesFolder));

    for (const std::filesystem::path& vendor : filesystemProbe_.ChildDirectories(liveries))
    {
        for (const std::filesystem::path& livery : filesystemProbe_.ChildDirectories(vendor))
        {
            const std::optional<std::string> configuration =
                filesystemProbe_.ContentsOf(PathUnder(livery, PathFromUtf8(kLiveryConfiguration)));

            if (configuration.has_value() && ItMentions(*configuration, kBaseContainerKey))
            {
                facts.declaresABaseContainer = true;
            }

            const std::optional<std::string> description =
                filesystemProbe_.ContentsOf(PathUnder(livery, PathFromUtf8(kLiveryDescription)));

            if (!description.has_value())
            {
                continue;
            }

            const std::string declared = TheQuotedValueAfter(*description, kProductPackageKey);

            if (!declared.empty())
            {
                facts.declaredPackages.push_back(declared);
            }
        }
    }
}
