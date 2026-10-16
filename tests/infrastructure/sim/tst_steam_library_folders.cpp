#include <QtTest/QtTest>

#include <filesystem>
#include <vector>

#include "infrastructure/sim/SteamLibraryFolders.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/TempFiles.h"

namespace
{
    class SteamLibraryFoldersTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPathOfTheFileIsAFolderInTheOrderOfTheFile();
        static void TheKeysAroundThePathAreNotFolders();
        static void AValueThatSaysPathIsNotTakenForTheKey();
        static void AnEscapedBackslashAndAnEscapedQuoteComeOutAsTheyWereMeant();
        static void ALineCommentIsSkipped();
        static void AFileThatIsNotThereOrIsEmptyHasNoFolders();
    };

    const std::string kRealFile = R"("libraryfolders"
{
	"0"
	{
		"path"		"/home/pilot/.local/share/Steam"
		"label"		""
		"contentid"		"3017528849140811684"
		"totalsize"		"0"
		"update_clean_bytes_tally"		"1572864"
		"apps"
		{
			"228980"		"223872"
			"1250410"		"76457349440"
		}
	}
	"1"
	{
		"path"		"/home/pilot/Jogos/SteamLibrary"
		"label"		""
		"apps"
		{
			"2537590"		"131072000000"
		}
	}
}
)";
}

void SteamLibraryFoldersTest::EveryPathOfTheFileIsAFolderInTheOrderOfTheFile()
{
    const TempFiles files;

    const std::vector<std::filesystem::path> folders =
        SteamLibraryFoldersIn(files.WriteText("libraryfolders.vdf", kRealFile));

    QCOMPARE(folders.size(), std::size_t{2});
    QCOMPARE(folders[0], std::filesystem::path("/home/pilot/.local/share/Steam"));
    QCOMPARE(folders[1], std::filesystem::path("/home/pilot/Jogos/SteamLibrary"));
}

void SteamLibraryFoldersTest::TheKeysAroundThePathAreNotFolders()
{
    const TempFiles files;
    const std::string file =
        "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"label\"\t\t\"/not/a/library\"\n\t\t\"path\"\t\t"
        "\"/real/library\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"path\"\t\t\"\"\n\t\t}\n\t}\n}\n";

    const std::vector<std::filesystem::path> folders = SteamLibraryFoldersIn(files.WriteText("a.vdf", file));

    QCOMPARE(folders.size(), std::size_t{1});
    QCOMPARE(folders[0], std::filesystem::path("/real/library"));
}

void SteamLibraryFoldersTest::AValueThatSaysPathIsNotTakenForTheKey()
{
    const TempFiles files;
    const std::string file =
        "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"label\"\t\t\"path\"\n\t\t\"path\"\t\t\"/real/library\"\n\t}\n}\n";

    const std::vector<std::filesystem::path> folders = SteamLibraryFoldersIn(files.WriteText("a.vdf", file));

    QCOMPARE(folders.size(), std::size_t{1});
    QCOMPARE(folders[0], std::filesystem::path("/real/library"));
}

void SteamLibraryFoldersTest::AnEscapedBackslashAndAnEscapedQuoteComeOutAsTheyWereMeant()
{
    const TempFiles files;
    const std::string file = R"("libraryfolders" { "0" { "path" "D:\\Games \"Steam\"" } })";

    const std::vector<std::filesystem::path> folders = SteamLibraryFoldersIn(files.WriteText("a.vdf", file));

    QCOMPARE(folders.size(), std::size_t{1});
    QCOMPARE(folders[0], std::filesystem::path(R"(D:\Games "Steam")"));
}

void SteamLibraryFoldersTest::ALineCommentIsSkipped()
{
    const TempFiles files;
    const std::string file = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t// \"path\" \"/commented/out\"\n\t\t\"path\" "
                             "\"/kept\"\n\t}\n}\n";

    const std::vector<std::filesystem::path> folders = SteamLibraryFoldersIn(files.WriteText("a.vdf", file));

    QCOMPARE(folders.size(), std::size_t{1});
    QCOMPARE(folders[0], std::filesystem::path("/kept"));
}

void SteamLibraryFoldersTest::AFileThatIsNotThereOrIsEmptyHasNoFolders()
{
    const TempFiles files;

    QVERIFY(SteamLibraryFoldersIn(files.Root() / "never-written.vdf").empty());
    QVERIFY(SteamLibraryFoldersIn(files.WriteText("empty.vdf", "")).empty());
}

QTEST_APPLESS_MAIN(SteamLibraryFoldersTest)

#include "tst_steam_library_folders.moc"
