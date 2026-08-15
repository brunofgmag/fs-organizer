#include <QtTest/QtTest>

#include "domain/bisection/CoupledUnits.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CoupledUnitsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonThatTouchesNobodyIsAUnitOfItsOwn();
        static void TwoAddonsWritingIntoTheSameModelFolderLandInOneUnit();
        static void ADeclaredPackageJoinsTheSatelliteToTheAddonItNames();
        static void TheSameRelativePathWrittenByTwoAddonsJoinsThem();
        static void TheClosureIsTransitive();
        static void TheBaseIsTheOnlyMemberThatDeclaresNoBaseOfItsOwn();
        static void AUnitWhoseMembersAllDeclareABaseHasNoBase();
        static void AUnitWithTwoMembersDeclaringNoBaseHasNoBase();
        static void UnitsComeOutInAStableOrder();
        static void TheEighteenOfTheMd11ComeTogetherOnTheNameAloneAndNothingElseHoldsThem();
    };
}

namespace
{
    constexpr auto kMd11Base = "D:/MSFS 2024/Aircrafts (2024)/tfdidesign-aircraft-md11";
    constexpr auto kMd11LiveryOne = "D:/MSFS 2024/Liveries/tfdidesign-md11f-ces-b2170";
    constexpr auto kMd11LiveryTwo = "D:/MSFS 2024/Liveries/tfdidesign-md11f-ces-b2171";
    constexpr auto kSevenSevenSevenBase = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77er";
    constexpr auto kSevenSevenSevenLiveries = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77er-liveries";
    constexpr auto kAirport = "D:/MSFS 2024/Airports/aerosoft-airport-ebbr-brussels";

    constexpr auto kMd11Model = "TFDi_Design_MD-11";
    constexpr auto kSevenSevenSevenModel = "PMDG 777-200ER";

    CouplingFacts Alone(const std::filesystem::path& folder)
    {
        return CouplingFacts{.folder = folder};
    }

    CouplingFacts Providing(const std::filesystem::path& folder, const std::string& modelFolder)
    {
        return CouplingFacts{.folder = folder, .modelFolders = {modelFolder}};
    }

    CouplingFacts Satellite(const std::filesystem::path& folder, const std::string& modelFolder)
    {
        return CouplingFacts{.folder = folder, .modelFolders = {modelFolder}, .declaresABaseContainer = true};
    }

    std::vector<std::filesystem::path> AddonsOf(const SearchUnit& unit)
    {
        return unit.addons;
    }
}

void CoupledUnitsTest::AnAddonThatTouchesNobodyIsAUnitOfItsOwn()
{
    const std::vector<SearchUnit> units = UnitsFrom({Alone(kAirport), Providing(kMd11Base, kMd11Model)});

    QCOMPARE(units.size(), std::size_t{2});
    QCOMPARE(AddonsOf(units.front()), std::vector<std::filesystem::path>{kMd11Base});
    QCOMPARE(AddonsOf(units.back()), std::vector<std::filesystem::path>{kAirport});
}

void CoupledUnitsTest::TwoAddonsWritingIntoTheSameModelFolderLandInOneUnit()
{
    const std::vector<SearchUnit> units =
        UnitsFrom({Providing(kMd11Base, kMd11Model), Satellite(kMd11LiveryOne, kMd11Model)});

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(AddonsOf(units.front()), (std::vector<std::filesystem::path>{kMd11Base, kMd11LiveryOne}));
}

void CoupledUnitsTest::ADeclaredPackageJoinsTheSatelliteToTheAddonItNames()
{
    CouplingFacts liveries = Alone(kSevenSevenSevenLiveries);
    liveries.declaredPackages = {"pmdg-aircraft-77er"};

    const std::vector<SearchUnit> units = UnitsFrom({Alone(kSevenSevenSevenBase), liveries});

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(AddonsOf(units.front()),
             (std::vector<std::filesystem::path>{kSevenSevenSevenBase, kSevenSevenSevenLiveries}));
}

void CoupledUnitsTest::TheSameRelativePathWrittenByTwoAddonsJoinsThem()
{
    CouplingFacts base = Alone(kSevenSevenSevenBase);
    base.writesInside = {"SimObjects/Airplanes/PMDG 777-200ER/model/wing.gltf"};

    CouplingFacts mod = Alone(kSevenSevenSevenLiveries);
    mod.writesInside = {"SimObjects/Airplanes/PMDG 777-200ER/model/wing.gltf"};

    const std::vector<SearchUnit> units = UnitsFrom({base, mod});

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(AddonsOf(units.front()),
             (std::vector<std::filesystem::path>{kSevenSevenSevenBase, kSevenSevenSevenLiveries}));
}

void CoupledUnitsTest::TheClosureIsTransitive()
{
    CouplingFacts second = Satellite(kMd11LiveryTwo, kSevenSevenSevenModel);
    second.declaredPackages = {"tfdidesign-md11f-ces-b2170"};

    const std::vector<SearchUnit> units =
        UnitsFrom({Providing(kMd11Base, kMd11Model), Satellite(kMd11LiveryOne, kMd11Model), second});

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(AddonsOf(units.front()), (std::vector<std::filesystem::path>{kMd11Base, kMd11LiveryOne, kMd11LiveryTwo}));
}

void CoupledUnitsTest::TheBaseIsTheOnlyMemberThatDeclaresNoBaseOfItsOwn()
{
    const std::vector<SearchUnit> units =
        UnitsFrom({Providing(kMd11Base, kMd11Model), Satellite(kMd11LiveryOne, kMd11Model),
                   Satellite(kMd11LiveryTwo, kMd11Model)});

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(units.front().base, std::optional<std::filesystem::path>{kMd11Base});
}

void CoupledUnitsTest::AUnitWhoseMembersAllDeclareABaseHasNoBase()
{
    const std::vector<SearchUnit> units =
        UnitsFrom({Satellite(kMd11LiveryOne, kMd11Model), Satellite(kMd11LiveryTwo, kMd11Model)});

    QCOMPARE(units.size(), std::size_t{1});
    QVERIFY(!units.front().base.has_value());
}

void CoupledUnitsTest::AUnitWithTwoMembersDeclaringNoBaseHasNoBase()
{
    const std::vector<SearchUnit> units =
        UnitsFrom({Providing(kMd11Base, kMd11Model), Providing(kMd11LiveryOne, kMd11Model),
                   Satellite(kMd11LiveryTwo, kMd11Model)});

    QCOMPARE(units.size(), std::size_t{1});
    QVERIFY(!units.front().base.has_value());
}

void CoupledUnitsTest::UnitsComeOutInAStableOrder()
{
    const std::vector<CouplingFacts> facts = {Alone(kAirport), Alone(kSevenSevenSevenBase), Alone(kMd11Base)};
    const std::vector<SearchUnit> units = UnitsFrom(facts);
    const std::vector<CouplingFacts> shuffled = {facts.back(), facts.front(), facts[1]};

    QCOMPARE(units.size(), std::size_t{3});
    QCOMPARE(AddonsOf(units.front()), std::vector<std::filesystem::path>{kMd11Base});
    QCOMPARE(AddonsOf(units.back()), std::vector<std::filesystem::path>{kAirport});

    for (std::size_t index = 0; index < units.size(); ++index)
    {
        QCOMPARE(AddonsOf(UnitsFrom(shuffled)[index]), AddonsOf(units[index]));
    }
}

void CoupledUnitsTest::TheEighteenOfTheMd11ComeTogetherOnTheNameAloneAndNothingElseHoldsThem()
{
    std::vector<CouplingFacts> library;
    library.push_back(Providing(kMd11Base, kMd11Model));

    for (int at = 0; at < 17; ++at)
    {
        library.push_back(Satellite("D:/MSFS 2024/Liveries/tfdidesign-md11-livery-" + std::to_string(at), kMd11Model));
    }

    const std::vector<SearchUnit> units = UnitsFrom(library);

    const std::vector<std::filesystem::path> together = AddonsOf(units.front());

    QCOMPARE(units.size(), std::size_t{1});
    QCOMPARE(together.size(), std::size_t{18});

    for (const CouplingFacts& member : library)
    {
        QVERIFY2(std::ranges::find(together, member.folder) != together.end(),
                 "every member of the shape the real library has lands in the one unit");
    }

    std::vector<CouplingFacts> withoutTheNameEdge = library;
    for (CouplingFacts& member : withoutTheNameEdge)
    {
        member.modelFolders.clear();
    }

    QCOMPARE(UnitsFrom(withoutTheNameEdge).size(), std::size_t{18});
}

QTEST_APPLESS_MAIN(CoupledUnitsTest)

#include "tst_coupled_units.moc"
