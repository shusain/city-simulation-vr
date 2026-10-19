#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialTaxTest,
                                 "CitySim.Economy.IndustrialPolicyBillingAndMigration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityIndustrialTaxTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {15, 8});
	City.CommitRoad(Road);
	for (const TCHAR* Id : {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Shop"), TEXT("Factory")})
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Id;
		D.DisplayName = Id;
		D.Footprint = {1, 1};
		D.Cost = 0;
		auto P = City.BeginPreview(D, {City.GetEntities().Num() * 2 + 1, 9});
		TestTrue(TEXT("Industrial policy fixture builds"), City.CommitPlacement(P, D).IsSuccess());
	}
	FCityEconomyTuning T;
	T.FixedStepSeconds = .125;
	T.FixedStepsPerMonth = 4;
	FCityEconomySimulation E;
	E.Advance(.5, City, T);
	const int64 Legacy = E.GetSnapshot().GoodsSold * T.IncomePerOperationalIndustry /
	                     FMath::Max(1, T.OutputPerOperationalIndustry);
	TestTrue(TEXT("Fixture has actual goods sales"), E.GetSnapshot().GoodsSold > 0);
	TestEqual(TEXT("Default10% preserves previous industrial income exactly"),
	          E.GetSnapshot().LastIndustrialTaxRevenue, Legacy);
	TestEqual(TEXT("Three billed taxes reconcile to total treasury income"), E.GetSnapshot().LastIncome,
	          E.GetSnapshot().LastResidentialTaxRevenue + E.GetSnapshot().LastCommercialTaxRevenue +
	              E.GetSnapshot().LastIndustrialTaxRevenue);
	const auto High = E.GetProjectedBudget(City, T, 1000, 1000, 2500);
	TestTrue(TEXT("Higher industrial tax increases forecast treasury income"), High.IndustrialTax > Legacy);
	TestEqual(TEXT("Pending industrial preview does not change policy"),
	          E.GetSnapshot().IndustrialTaxBasisPoints, 1000);
	E.SetTaxRates(1000, 1000, 2500);
	const int64 Before = City.GetFunds();
	E.Advance(.5, City, T);
	TestEqual(TEXT("Industrial forecast and billed revenue match"), E.GetSnapshot().LastIndustrialTaxRevenue,
	          High.IndustrialTax);
	TestEqual(TEXT("Industrial taxation uses the existing single treasury settlement"),
	          City.GetFunds() - Before, E.GetSnapshot().LastIncome - E.GetSnapshot().LastUpkeep);
	E.SetTaxRates(1000, 1000, 0);
	E.Advance(.5, City, T);
	TestEqual(TEXT("Zero industrial tax has no residual trade charge"),
	          E.GetSnapshot().LastIndustrialTaxRevenue, int64(0));
	TestFalse(TEXT("Out-of-range policy rejects atomically"), E.SetTaxRates(0, 0, 2501));
	TestEqual(TEXT("Invalid command preserves residential policy"), E.GetSnapshot().ResidentialTaxBasisPoints,
	          1000);
	TestFalse(TEXT("Invalid negative industrial policy rejects"), E.SetTaxRates(1000, 1000, -2));
	E.SetTaxRates(1000, 1000, 1700);
	E.SetTaxRates(900, 1400);
	TestEqual(TEXT("Existing two-rate API retains an already selected industrial rate"),
	          E.GetSnapshot().IndustrialTaxBasisPoints, 1700);

	FCitySaveDocument Doc, Loaded;
	Doc.Placement = City.ExportState();
	Doc.Economy = E.GetSnapshot();
	FString Json, Error;
	TestTrue(TEXT("Industrial policy serializes"), FCityPersistence::Serialize(Doc, Json, Error));
	TestTrue(TEXT("Industrial policy deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("Rate survives save"), Loaded.Economy.IndustrialTaxBasisPoints, 1700);
	TestEqual(TEXT("Separate billed industrial revenue survives save"),
	          Loaded.Economy.LastIndustrialTaxRevenue, Doc.Economy.LastIndustrialTaxRevenue);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	auto Obj = Root->GetObjectField(TEXT("economy"));
	auto Encode = [&]() {
		FString Out;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
		return Out;
	};
	Obj->RemoveField(TEXT("industrialTaxBasisPoints"));
	TestFalse(TEXT("Schema31 missing rate rejects"), FCityPersistence::Deserialize(Encode(), Loaded, Error));
	Obj->SetNumberField(TEXT("industrialTaxBasisPoints"), 2501);
	TestFalse(TEXT("Schema31 invalid rate rejects"), FCityPersistence::Deserialize(Encode(), Loaded, Error));
	Obj->SetNumberField(TEXT("industrialTaxBasisPoints"), 1700);
	Obj->RemoveField(TEXT("lastIndustrialTaxRevenue"));
	TestFalse(TEXT("Schema31 missing billed amount rejects"),
	          FCityPersistence::Deserialize(Encode(), Loaded, Error));
	return true;
}
#endif
