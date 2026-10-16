#include <QtTest/QtTest>

#include <filesystem>
#include <optional>
#include <vector>

#include "infrastructure/sim/ProtonPrefixes.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ProtonTree.h"

namespace
{
    class ProtonPrefixesTest : public QObject
    {
        Q_OBJECT

    private slots:
        void init();
        void cleanup();
        void TheRootsAreTheSteamFolderUnderTheDataHomeAndTheSteamLinkAtHome();
        void TheSteamLinkThatPointsAtTheDataHomeFolderIsNotARootTwice();
        void TheLibrariesAreTheRootsAndEveryFolderTheLibraryFileLists();
        void ALibraryListedUnderAnotherSpellingOfAFolderAlreadyKnownIsNotRepeated();
        void ARootWithoutALibraryFileIsStillALibrary();
        void APrefixExistsOnlyWhereTheManifestAndThePrefixFolderAre();
        void TheOldInstallationLeftInCommonIsNeverAPrefix();
        void ThePrefixesFollowTheOrderOfTheLibrariesAndTheSimulatorsInThem();
        void NoLibraryHasNoPrefix();

    private:
        std::optional<QTemporaryDir> root_{};
        QByteArray home_{};
        QByteArray data_{};
    };
}

void ProtonPrefixesTest::init()
{
    home_ = qgetenv("HOME");
    data_ = qgetenv("XDG_DATA_HOME");

    root_.emplace();
    QVERIFY(root_->isValid());

    qputenv("HOME", root_->filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_HOME", root_->filePath(QStringLiteral("home/.local/share")).toUtf8());
}

void ProtonPrefixesTest::cleanup()
{
    const auto restore = [](const char* name, const QByteArray& value)
    {
        if (value.isNull())
        {
            qunsetenv(name);
            return;
        }

        qputenv(name, value);
    };

    restore("HOME", home_);
    restore("XDG_DATA_HOME", data_);
}

void ProtonPrefixesTest::TheRootsAreTheSteamFolderUnderTheDataHomeAndTheSteamLinkAtHome()
{
    const std::filesystem::path home = AsPath(root_->filePath(QStringLiteral("home")));

    const std::vector<std::filesystem::path> roots = SteamRoots();

    QCOMPARE(roots.size(), std::size_t{2});
    QCOMPARE(roots[0], home / ".local" / "share" / "Steam");
    QCOMPARE(roots[1], home / ".steam" / "steam");
}

void ProtonPrefixesTest::TheSteamLinkThatPointsAtTheDataHomeFolderIsNotARootTwice()
{
    const std::filesystem::path home = AsPath(root_->filePath(QStringLiteral("home")));
    std::filesystem::create_directories(home / ".local" / "share" / "Steam");
    std::filesystem::create_directories(home / ".steam");
    std::filesystem::create_directory_symlink(home / ".local" / "share" / "Steam", home / ".steam" / "steam");

    const std::vector<std::filesystem::path> roots = SteamRoots();

    QCOMPARE(roots.size(), std::size_t{1});
    QCOMPARE(roots[0], home / ".local" / "share" / "Steam");
}

void ProtonPrefixesTest::TheLibrariesAreTheRootsAndEveryFolderTheLibraryFileLists()
{
    const ProtonTree tree;
    const std::filesystem::path games = tree.Home() / "Jogos" / "SteamLibrary";
    const std::filesystem::path usb = tree.Root() / "mnt" / "usb" / "Steam";
    tree.WriteLibraryFolders(tree.SteamRoot(), {tree.SteamRoot(), games, usb});

    const std::vector<std::filesystem::path> libraries = SteamLibraries({tree.SteamRoot()});

    QCOMPARE(libraries.size(), std::size_t{3});
    QCOMPARE(libraries[0], tree.SteamRoot());
    QCOMPARE(libraries[1], games);
    QCOMPARE(libraries[2], usb);
}

void ProtonPrefixesTest::ALibraryListedUnderAnotherSpellingOfAFolderAlreadyKnownIsNotRepeated()
{
    const ProtonTree tree;
    std::filesystem::create_directories(tree.SteamRoot());
    std::filesystem::create_directories(tree.Home() / ".steam");
    std::filesystem::create_directory_symlink(tree.SteamRoot(), tree.Home() / ".steam" / "steam");
    tree.WriteLibraryFolders(tree.SteamRoot(), {tree.Home() / ".steam" / "steam"});

    const std::vector<std::filesystem::path> libraries =
        SteamLibraries({tree.SteamRoot(), tree.Home() / ".steam" / "steam"});

    QCOMPARE(libraries.size(), std::size_t{1});
    QCOMPARE(libraries[0], tree.SteamRoot());
}

void ProtonPrefixesTest::ARootWithoutALibraryFileIsStillALibrary()
{
    const ProtonTree tree;

    const std::vector<std::filesystem::path> libraries = SteamLibraries({tree.SteamRoot()});

    QCOMPARE(libraries.size(), std::size_t{1});
    QCOMPARE(libraries[0], tree.SteamRoot());
}

void ProtonPrefixesTest::APrefixExistsOnlyWhereTheManifestAndThePrefixFolderAre()
{
    const ProtonTree tree;
    const std::filesystem::path withBoth = tree.Root() / "with-both";
    const std::filesystem::path manifestOnly = tree.Root() / "manifest-only";
    const std::filesystem::path prefixOnly = tree.Root() / "prefix-only";

    const std::filesystem::path prefix = tree.AddPrefix(withBoth, ProtonTree::kMsfs2024);
    static_cast<void>(tree.AddPrefix(manifestOnly, ProtonTree::kMsfs2024));
    std::filesystem::remove_all(manifestOnly / "steamapps" / "compatdata");
    static_cast<void>(tree.AddPrefix(prefixOnly, ProtonTree::kMsfs2024));
    std::filesystem::remove(prefixOnly / "steamapps" / "appmanifest_2537590.acf");

    const std::vector<ProtonPrefix> prefixes = ProtonPrefixes({withBoth, manifestOnly, prefixOnly});

    QCOMPARE(prefixes.size(), std::size_t{1});
    QCOMPARE(prefixes[0].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(prefixes[0].path, prefix);
}

void ProtonPrefixesTest::TheOldInstallationLeftInCommonIsNeverAPrefix()
{
    const ProtonTree tree;
    const std::filesystem::path games = tree.Home() / "Jogos" / "SteamLibrary";
    static_cast<void>(tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020));
    static_cast<void>(tree.AddPrefix(games, ProtonTree::kMsfs2024));
    std::filesystem::create_directories(games / "steamapps" / "common" / "MicrosoftFlightSimulator");
    static_cast<void>(tree.WriteText(games / "steamapps" / "common" / "MicrosoftFlightSimulator" / "cache.bin", "x"));

    const std::vector<ProtonPrefix> prefixes = ProtonPrefixes({tree.SteamRoot(), games});

    QCOMPARE(prefixes.size(), std::size_t{2});
    QCOMPARE(prefixes[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(prefixes[0].path, tree.SteamRoot() / "steamapps" / "compatdata" / "1250410" / "pfx");
    QCOMPARE(prefixes[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(prefixes[1].path, games / "steamapps" / "compatdata" / "2537590" / "pfx");
}

void ProtonPrefixesTest::ThePrefixesFollowTheOrderOfTheLibrariesAndTheSimulatorsInThem()
{
    const ProtonTree tree;
    const std::filesystem::path second = tree.Root() / "second";
    static_cast<void>(tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2024));
    static_cast<void>(tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020));
    static_cast<void>(tree.AddPrefix(second, ProtonTree::kMsfs2024));

    const std::vector<ProtonPrefix> prefixes = ProtonPrefixes({tree.SteamRoot(), second});

    QCOMPARE(prefixes.size(), std::size_t{3});
    QCOMPARE(prefixes[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(prefixes[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(prefixes[1].path.parent_path().parent_path().parent_path().parent_path(), tree.SteamRoot());
    QCOMPARE(prefixes[2].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(prefixes[2].path.parent_path().parent_path().parent_path().parent_path(), second);
}

void ProtonPrefixesTest::NoLibraryHasNoPrefix()
{
    QVERIFY(ProtonPrefixes({}).empty());
}

QTEST_APPLESS_MAIN(ProtonPrefixesTest)

#include "tst_proton_prefixes.moc"
