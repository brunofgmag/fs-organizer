#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/PackageList.h"
#include "infrastructure/sim/ContentXmlDocument.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class ContentXmlDocumentTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPrefixAndEveryActivationValueOfTheRealFileIsRead();
        static void AValueTheFileUsesAndTheAppDoesNotKnowStaysUnknownInsteadOfPassingForActivated();
        static void TurningAnEntryBackOnLeavesEveryOtherByteWhereItWas();
        static void TurningAnEntryOffLeavesEveryOtherByteWhereItWas();
        static void NoEntryIsAddedRemovedOrReordered();
        static void APackageTheListDoesNotCarryChangesNothing();
        static void ThePrefixIsPartOfTheNameAndNotStrippedBeforeMatching();
        static void AnEntryWithNoActivationValueIsLeftAloneInsteadOfGainingOne();
        static void TheRootElementIsNotAnEntryAndAValueSpeltLikeTheAttributeIsNotOne();
    };

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        std::ifstream file(std::filesystem::path(FSORG_FIXTURES_DIR) / name, std::ios::binary);

        return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::size_t FirstDifference(const std::string& left, const std::string& right)
    {
        const std::size_t shared = std::min(left.size(), right.size());

        for (std::size_t at = 0; at < shared; ++at)
        {
            if (left[at] != right[at])
            {
                return at;
            }
        }

        return left.size() == right.size() ? std::string::npos : shared;
    }

    [[nodiscard]] QStringList NamesOf(const std::vector<PackageEntry>& entries)
    {
        QStringList names;
        for (const PackageEntry& entry : entries)
        {
            names << QString::fromStdString(entry.name);
        }

        return names;
    }

    [[nodiscard]] PackageActivation ActivationOf(const std::vector<PackageEntry>& entries, const std::string& name)
    {
        const auto found = std::ranges::find(entries, name, &PackageEntry::name);

        return found == entries.end() ? PackageActivation::ItSaysSomethingElse : found->activation;
    }
}

void ContentXmlDocumentTest::EveryPrefixAndEveryActivationValueOfTheRealFileIsRead()
{
    const std::vector<PackageEntry> entries = PackageEntriesIn(Fixture("simulator-content.xml"));

    QCOMPARE(entries.size(), std::size_t{9});
    QCOMPARE(entries.front().name, std::string("fs24-asobo-vcockpits-core"));
    QCOMPARE(ActivationOf(entries, "fs24-asobo-vcockpits-core"), PackageActivation::Activated);
    QCOMPARE(ActivationOf(entries, "fs24-asobo-airport-lpma-madeira"), PackageActivation::UserDisabled);
    QCOMPARE(ActivationOf(entries, "communityfs20-ag-airport-bgno-station-nord"), PackageActivation::SystemDisabled);
    QCOMPARE(ActivationOf(entries, "fs20-asobo-activities"), PackageActivation::Activated);
    QCOMPARE(ActivationOf(entries, "communityfs24-aaa-simaddons-animals"), PackageActivation::Activated);
    QCOMPARE(ActivationOf(entries, "communityfs20-xmd11_light_mod_fs24"), PackageActivation::Activated);
}

void ContentXmlDocumentTest::AValueTheFileUsesAndTheAppDoesNotKnowStaysUnknownInsteadOfPassingForActivated()
{
    constexpr std::string_view oneValueNobodyDocumented = R"(<Packages>
	<Package name="fs24-asobo-something" active="SomethingNobodyWroteDown"/>
</Packages>
)";

    const std::vector<PackageEntry> entries = PackageEntriesIn(oneValueNobodyDocumented);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().activation, PackageActivation::ItSaysSomethingElse);
}

void ContentXmlDocumentTest::TurningAnEntryBackOnLeavesEveryOtherByteWhereItWas()
{
    const std::optional<std::string> written =
        WithPackageSwitched(Fixture("simulator-content.xml"), "fs24-asobo-airport-lpma-madeira", true);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, Fixture("simulator-content-lpma-activated.xml")), std::string::npos);
}

void ContentXmlDocumentTest::TurningAnEntryOffLeavesEveryOtherByteWhereItWas()
{
    const std::optional<std::string> written =
        WithPackageSwitched(Fixture("simulator-content.xml"), "communityfs24-aaa-simaddons-animals", false);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, Fixture("simulator-content-animals-disabled.xml")), std::string::npos);
}

void ContentXmlDocumentTest::NoEntryIsAddedRemovedOrReordered()
{
    const std::string before = Fixture("simulator-content.xml");
    const std::optional<std::string> written =
        WithPackageSwitched(before, "communityfs24-aaa-simaddons-animals", false);

    QVERIFY(written.has_value());

    const std::vector<PackageEntry> was = PackageEntriesIn(before);
    const std::vector<PackageEntry> is = PackageEntriesIn(*written);

    QCOMPARE(NamesOf(is), NamesOf(was));
    QCOMPARE(written->size(), before.size() + std::string("UserDisabled").size() - std::string("Activated").size());
    QCOMPARE(ActivationOf(is, "communityfs24-aaa-simaddons-animals"), PackageActivation::UserDisabled);
    QCOMPARE(ActivationOf(is, "communityfs20-ag-airport-bgno-station-nord"), PackageActivation::SystemDisabled);
}

void ContentXmlDocumentTest::APackageTheListDoesNotCarryChangesNothing()
{
    QVERIFY(
        !WithPackageSwitched(Fixture("simulator-content.xml"), "fs24-nobody-ever-installed-this", false).has_value());
}

void ContentXmlDocumentTest::ThePrefixIsPartOfTheNameAndNotStrippedBeforeMatching()
{
    const std::string document = Fixture("simulator-content.xml");

    QVERIFY2(!WithPackageSwitched(document, "aaa-simaddons-animals", false).has_value(),
             "the name of an entry carries its prefix, and the match is exact because that name is the identity");
    QVERIFY2(!WithPackageSwitched(document, "COMMUNITYFS24-AAA-SIMADDONS-ANIMALS", false).has_value(),
             "the two sides of the match are byte for byte identical, so folding case would only invent a match");
}

void ContentXmlDocumentTest::AnEntryWithNoActivationValueIsLeftAloneInsteadOfGainingOne()
{
    constexpr std::string_view withoutTheAttribute = R"(<Packages>
	<Package name="fs24-asobo-something"/>
</Packages>
)";

    QVERIFY2(!WithPackageSwitched(withoutTheAttribute, "fs24-asobo-something", false).has_value(),
             "the app changes the value of an entry that has one, and never writes an attribute the file did not "
             "carry, in a file the simulator wipes whole when it dislikes it");
}

void ContentXmlDocumentTest::TheRootElementIsNotAnEntryAndAValueSpeltLikeTheAttributeIsNotOne()
{
    constexpr std::string_view awkward = R"(<Packages active="Activated">
	<Package name="communityfs24-active-scenery" active="Activated"/>
</Packages>
)";

    const std::vector<PackageEntry> entries = PackageEntriesIn(awkward);
    QCOMPARE(entries.size(), std::size_t{1});

    const std::optional<std::string> written = WithPackageSwitched(awkward, "communityfs24-active-scenery", false);

    QVERIFY(written.has_value());
    QCOMPARE(ActivationOf(PackageEntriesIn(*written), "communityfs24-active-scenery"), PackageActivation::UserDisabled);
    QVERIFY2(written->find(R"(<Packages active="Activated">)") == 0,
             "the root element carries the same attribute and the same opening text, and what keeps it out of the "
             "write is having no name attribute to match, not a check of its own");
}

QTEST_APPLESS_MAIN(ContentXmlDocumentTest)

#include "tst_content_xml_document.moc"
