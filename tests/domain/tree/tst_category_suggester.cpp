#include <QtTest/QtTest>

#include "domain/tree/CategorySuggester.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CategorySuggesterTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAirportInTheNameGoesToTheSceneriesCategory();
        static void TheSceneryContentTypeGoesToTheSceneriesCategory();
        static void BothShapesOfTheSoundContentTypeGoToTheSoundsCategory();
        static void TrafficInTheNameGoesToTheTrafficCategory();
        static void TheLiveryContentTypeGoesToTheLiveriesCategory();
        static void AnAddonNoRuleRecognisesIsLeftWhereItIs();
        static void ARuleThatNamesACategoryTheLibraryDoesNotHaveSuggestsNothing();
        static void EverySuggestionSaysWhichRuleProducedIt();
        static void AnAddonAlreadyInTheSuggestedCategoryIsNotProposedForAMove();
        static void TheContentTypeIsReadWithoutDistinguishingCase();
    };
}

namespace
{
    TreeNode AddonNode(const std::filesystem::path& path, const std::string& contentType)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path,
                           .manifest = Manifest{.title = "",
                                                .creator = "",
                                                .manufacturer = "",
                                                .contentType = contentType,
                                                .packageVersion = "",
                                                .minimumGameVersion = ""}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode DeclaredCategoryNode(const std::filesystem::path& path)
    {
        TreeNode node = CategoryNode(path, {});
        node.declaredAsCategory = true;

        return node;
    }

    TreeNode Library(std::vector<TreeNode> categories)
    {
        TreeNode node = CategoryNode("D:/Library", std::move(categories));
        node.kind = TreeNodeKind::Library;

        return node;
    }

    TreeNode ReferenceLibrary()
    {
        return Library({DeclaredCategoryNode("D:/Library/Sceneries"), DeclaredCategoryNode("D:/Library/Sounds"),
                        DeclaredCategoryNode("D:/Library/Traffic"), DeclaredCategoryNode("D:/Library/Liveries"),
                        DeclaredCategoryNode("D:/Library/Aircrafts")});
    }

    CategorySuggestion SuggestOne(const TreeNode& library, const TreeNode& addon)
    {
        const std::vector<CategorySuggestion> suggestions = SuggestCategories(library, {&addon});

        return suggestions.empty() ? CategorySuggestion{} : suggestions.front();
    }
}

void CategorySuggesterTest::AnAirportInTheNameGoesToTheSceneriesCategory()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/aaa-airport-sbgr-guarulhos", "MISC");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QCOMPARE(suggestion.suggestedCategory, std::filesystem::path("D:/Library/Sceneries"));
    QCOMPARE(suggestion.rule, CategoryRule::TheNameSaysAirport);
}

void CategorySuggesterTest::TheSceneryContentTypeGoesToTheSceneriesCategory()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/orbx-ybbn", "SCENERY");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QCOMPARE(suggestion.suggestedCategory, std::filesystem::path("D:/Library/Sceneries"));
    QCOMPARE(suggestion.rule, CategoryRule::TheContentTypeIsScenery);
}

void CategorySuggesterTest::BothShapesOfTheSoundContentTypeGoToTheSoundsCategory()
{
    const TreeNode plain = AddonNode("D:/Library/Aircrafts/xbaw-b777", "SOUND");
    const TreeNode spelled = AddonNode("D:/Library/Aircrafts/b777-sp-ge", "Sound Set");

    QCOMPARE(SuggestOne(ReferenceLibrary(), plain).suggestedCategory, std::filesystem::path("D:/Library/Sounds"));
    QCOMPARE(SuggestOne(ReferenceLibrary(), spelled).suggestedCategory, std::filesystem::path("D:/Library/Sounds"));
    QCOMPARE(SuggestOne(ReferenceLibrary(), spelled).rule, CategoryRule::TheContentTypeIsSound);
}

void CategorySuggesterTest::TrafficInTheNameGoesToTheTrafficCategory()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/aig-aitraffic-oci", "MISC");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QCOMPARE(suggestion.suggestedCategory, std::filesystem::path("D:/Library/Traffic"));
    QCOMPARE(suggestion.rule, CategoryRule::TheNameSaysTraffic);
}

void CategorySuggesterTest::TheLiveryContentTypeGoesToTheLiveriesCategory()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/inibuilds-a350-tam", "LIVERY");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QCOMPARE(suggestion.suggestedCategory, std::filesystem::path("D:/Library/Liveries"));
    QCOMPARE(suggestion.rule, CategoryRule::TheContentTypeIsLivery);
}

void CategorySuggesterTest::AnAddonNoRuleRecognisesIsLeftWhereItIs()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/pmdg-aircraft-77w", "AIRCRAFT");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QVERIFY(!suggestion.Classified());
    QVERIFY(suggestion.suggestedCategory.empty());
    QCOMPARE(suggestion.rule, CategoryRule::None);
    QCOMPARE(suggestion.currentCategory, std::filesystem::path("D:/Library/Aircrafts"));
}

void CategorySuggesterTest::ARuleThatNamesACategoryTheLibraryDoesNotHaveSuggestsNothing()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/orbx-ybbn", "SCENERY");
    const TreeNode library = Library({DeclaredCategoryNode("D:/Library/Aircrafts")});

    const CategorySuggestion suggestion = SuggestOne(library, addon);

    QVERIFY(!suggestion.Classified());
    QCOMPARE(suggestion.rule, CategoryRule::None);
}

void CategorySuggesterTest::EverySuggestionSaysWhichRuleProducedIt()
{
    const TreeNode airport = AddonNode("D:/Library/Aircrafts/aaa-airport-sbgr", "MISC");
    const TreeNode scenery = AddonNode("D:/Library/Aircrafts/orbx-ybbn", "SCENERY");
    const TreeNode unknown = AddonNode("D:/Library/Aircrafts/pmdg-aircraft-77w", "AIRCRAFT");

    const std::vector<CategorySuggestion> suggestions =
        SuggestCategories(ReferenceLibrary(), {&airport, &scenery, &unknown});

    QCOMPARE(suggestions.size(), std::size_t{3});

    for (const CategorySuggestion& suggestion : suggestions)
    {
        QCOMPARE(suggestion.Classified(), suggestion.rule != CategoryRule::None);
    }
}

void CategorySuggesterTest::AnAddonAlreadyInTheSuggestedCategoryIsNotProposedForAMove()
{
    const TreeNode addon = AddonNode("D:/Library/Sceneries/orbx-ybbn", "SCENERY");

    const CategorySuggestion suggestion = SuggestOne(ReferenceLibrary(), addon);

    QVERIFY(suggestion.Classified());
    QVERIFY(!suggestion.WouldMove());
    QCOMPARE(suggestion.suggestedCategory, suggestion.currentCategory);
}

void CategorySuggesterTest::TheContentTypeIsReadWithoutDistinguishingCase()
{
    const TreeNode addon = AddonNode("D:/Library/Aircrafts/some-thing", "misc");
    const TreeNode livery = AddonNode("D:/Library/Aircrafts/other-thing", "livery");

    QVERIFY(!SuggestOne(ReferenceLibrary(), addon).Classified());
    QCOMPARE(SuggestOne(ReferenceLibrary(), livery).suggestedCategory, std::filesystem::path("D:/Library/Liveries"));
}

QTEST_APPLESS_MAIN(CategorySuggesterTest)

#include "tst_category_suggester.moc"
