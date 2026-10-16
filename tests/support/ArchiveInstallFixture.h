#ifndef FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_INSTALL_FIXTURE_H
#define FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_INSTALL_FIXTURE_H

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/ports/ManifestParser.h"
#include "support/PathText.h"
#include "tests/doubles/FakeArchiveReader.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/StartupOverFakes.h"
#include "viewmodel/ArchiveInstallViewModel.h"

namespace
{
    constexpr std::uintmax_t kMegabyte = 1024 * 1024;
    constexpr std::uintmax_t kManifestBytes = 100;
    constexpr std::uintmax_t kSceneryBytes = 400;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kUtils = "D:/Library/Utils";
    const std::filesystem::path kSceneries = "D:/Library/Sceneries";
    const std::filesystem::path kSpare = "F:/Spare";
    const std::filesystem::path kInLibrary = "D:/Library/Utils/simbridge";
    const std::filesystem::path kInDestination = "E:/Sim/Community/simbridge";
    const std::filesystem::path kHeldInLibrary = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path kFirst = "D:/Downloads/first.zip";
    const std::filesystem::path kSecond = "D:/Downloads/second.rar";
    const std::filesystem::path kThird = "D:/Downloads/third.7z";

    class ScriptedManifestParser final : public ManifestParser
    {
    public:
        [[nodiscard]] std::optional<Manifest> Parse(const std::string_view content) const override
        {
            constexpr std::string_view prefix = "ok:";

            if (!content.starts_with(prefix))
            {
                return std::nullopt;
            }

            return Manifest{.packageVersion = std::string{content.substr(prefix.size())}};
        }
    };

    class ArchivesByPath final : public ArchiveReader
    {
    public:
        explicit ArchivesByPath(InMemoryFileSystem& fileSystem) : fileSystem_(fileSystem)
        {
        }

        FakeArchiveReader& At(const std::filesystem::path& archive)
        {
            std::unique_ptr<FakeArchiveReader>& reader = readers_[archive.generic_string()];

            if (!reader)
            {
                reader = std::make_unique<FakeArchiveReader>(fileSystem_);
            }

            return *reader;
        }

        [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& archive) const override
        {
            const FakeArchiveReader* reader = Find(archive);

            return reader == nullptr ? ArchiveListing{.failure = ArchiveFailure::CouldNotOpen}
                                     : reader->EntriesOf(archive);
        }

        [[nodiscard]] ArchiveText ContentsOf(const std::filesystem::path& archive,
                                             const std::string& entry,
                                             const std::size_t most) const override
        {
            const FakeArchiveReader* reader = Find(archive);

            return reader == nullptr ? ArchiveText{.failure = ArchiveFailure::CouldNotOpen}
                                     : reader->ContentsOf(archive, entry, most);
        }

        [[nodiscard]] ArchiveFailure
        ExtractAll(const std::filesystem::path& archive,
                   const std::vector<ArchiveEntry>& listed,
                   const std::filesystem::path& into,
                   const std::function<bool(const CopyProgress&)>& onProgress) const override
        {
            const FakeArchiveReader* reader = Find(archive);

            return reader == nullptr ? ArchiveFailure::CouldNotOpen
                                     : reader->ExtractAll(archive, listed, into, onProgress);
        }

    private:
        [[nodiscard]] const FakeArchiveReader* Find(const std::filesystem::path& archive) const
        {
            const auto found = readers_.find(archive.generic_string());

            return found == readers_.end() ? nullptr : found->second.get();
        }

        InMemoryFileSystem& fileSystem_;
        std::map<std::string, std::unique_ptr<FakeArchiveReader>> readers_;
    };

    struct Packaged
    {
        std::string folder{};
        std::string version{};
    };

    ArchiveEntry File(std::string path, const std::uintmax_t bytes)
    {
        return ArchiveEntry{.path = std::move(path), .bytes = bytes, .kind = ArchiveEntryKind::File};
    }

    TreeNode AddonNodeDeclaring(const std::filesystem::path& path, const std::string& version)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{.packageVersion = version}};

        return node;
    }

    SimulatorProfile Profile(std::vector<Library> libraries)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = std::move(libraries);

        return profile;
    }

    std::vector<Library> OneLibrary()
    {
        return {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};
    }

    inline std::vector<Library> TwoLibraries()
    {
        return {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"},
                Library{.id = "library-2", .path = kSpare}};
    }

    struct Fixture
    {
        explicit Fixture(const std::vector<Library>& libraries = OneLibrary())
            : settings(SettingsWith(Profile(libraries)))
        {
            fileSystem.AddDirectory(kDestination);

            for (const Library& library : libraries)
            {
                fileSystem.AddDirectory(library.path);
            }

            fileSystem.AddDirectory(kUtils);
            TheLibraryHoldsNodes({});
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService importing{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                                linking, log,          LinkType::Junction};
        ArchivesByPath archives{fileSystem};
        ScriptedManifestParser manifests;
        ArchiveInstallService service{archives, manifests, filesystemProbe, files, log, importing};
        StartupOverFakes startup{filesystemProbe};
        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        ArchiveInstallViewModel viewModel{service, profiles, processProbe, session, notifier, runner};

        void TheLibraryHoldsNodes(const std::vector<TreeNode>& addons, const std::vector<TreeNode>& categories = {})
        {
            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children = categories;
            library.children.insert(library.children.end(), addons.begin(), addons.end());

            catalog.SetTree(kLibrary, std::move(library));
            session.ShowActiveProfile();
        }

        void ASimbridgeIsInstalled(const std::string& version = "1.0.0", const bool withALink = false)
        {
            fileSystem.AddDirectory(kInLibrary);
            fileSystem.AddFile(kInLibrary / "manifest.json", kMegabyte);
            TheLibraryHoldsNodes({AddonNodeDeclaring(kInLibrary, version)});

            if (withALink)
            {
                fileSystem.AddLink(kInDestination, kInLibrary);
                session.ShowActiveProfile();
            }
        }

        void TheQuarantineHoldsASimbridge(const std::string& version)
        {
            fileSystem.AddDirectory(kHeldInLibrary.parent_path());
            fileSystem.AddDirectory(kHeldInLibrary);
            fileSystem.AddFile(kHeldInLibrary / "manifest.json", 3 * kMegabyte);
            catalog.SetTree(kHeldInLibrary, AddonNodeDeclaring(kHeldInLibrary, version));
        }

        void ArchiveHolds(const std::filesystem::path& archive, const std::vector<Packaged>& packages)
        {
            std::vector<ArchiveEntry> entries;
            FakeArchiveReader& reader = archives.At(archive);

            for (const Packaged& package : packages)
            {
                entries.push_back(File(package.folder + "/manifest.json", kManifestBytes));
                entries.push_back(File(package.folder + "/scenery/a.bgl", kSceneryBytes));
                reader.Holds(package.folder + "/manifest.json", "ok:" + package.version);
            }

            reader.Lists(std::move(entries));
        }

        [[nodiscard]] InstallOffer Reads(const std::vector<std::filesystem::path>& paths,
                                         const std::filesystem::path& droppedOn = {})
        {
            const QSignalSpy offers(&viewModel, &ArchiveInstallViewModel::OfferReady);

            viewModel.Read(paths, droppedOn);

            return offers.isEmpty() ? InstallOffer{} : offers.back().front().value<InstallOffer>();
        }

        [[nodiscard]] static ChosenAddon Chooses(const InstallOffer& offer,
                                                 const int archive,
                                                 const int addon,
                                                 const bool discard = false,
                                                 const std::size_t library = 0)
        {
            const InstallArchiveLine& line = offer.archives[static_cast<std::size_t>(archive)];
            const InstallAddonLine& chosen = line.addons[static_cast<std::size_t>(addon)];

            return ChosenAddon{.archive = archive,
                               .addon = addon,
                               .discardsTheQuarantinedCopy = discard,
                               .archivePath = line.path,
                               .root = chosen.root,
                               .folderName = chosen.folderName,
                               .replaces = chosen.inEachLibrary[library].occupant,
                               .source = line.source};
        }

        [[nodiscard]] static TreeNode ADeclaredCategory(const std::filesystem::path& path)
        {
            return TreeNode{.kind = TreeNodeKind::Category, .path = path, .declaredAsCategory = true};
        }
    };
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_INSTALL_FIXTURE_H
