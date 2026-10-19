#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
namespace
{
uint64 PlaceUtility(FCityPlacementModel& City, const UCityBuildingCatalog* Catalog, FName Name, int32 X)
{
	const auto* D = Catalog->FindDefinition(Name);
	auto P = City.BeginPreview(*D, {X, 9});
	return City.CommitPlacement(P, *D).EntityId;
}
void UtilityRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {63, 8});
	City.CommitRoad(P);
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityUpgradeAuthorityTest, "CitySim.Utilities.PurchaseTimerAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityUpgradeAuthorityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	for (FName Name : {FName(TEXT("Power")), FName(TEXT("Service"))})
	{
		const auto* D = Catalog->FindDefinition(Name);
		if (!TestTrue(TEXT("Valid upgradeable utility"), D && D->bPlayerPurchasedLevels && D->IsValid()))
			return false;
		FCityPlacementModel City(100000);
		UtilityRoad(City);
		const uint64 Id = PlaceUtility(City, Catalog, Name, 8);
		const auto Original = City.GetEntities().FindChecked(Id);
		FCityDevelopmentSimulation Dev;
		Dev.InitializeExistingAsOperational(City, Catalog, 0);
		FCityServiceSnapshot S;
		S.bUsesPublicStaffing = true;
		FString Error;
		const int64 Before = City.GetFunds();
		TestFalse(TEXT("Unstaffed utility cannot purchase"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestEqual(TEXT("Rejected purchase does not charge"), City.GetFunds(), Before);
		S.PublicStaffPercent.Add(Id, 100);
		TestTrue(TEXT("Utility expansion starts"),
		         Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestEqual(TEXT("Exact expansion price"), Before - City.GetFunds(),
		          D->FindDensityLevel(ECityDensityLevel::Medium)->UpgradeCost);
		TestFalse(TEXT("Repeated confirmation does not charge"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestEqual(TEXT("Utility uses full-day saved clock"),
		          Dev.GetSnapshot().Find(Id)->StationCompleteHalfDay, int64(-1));
		Dev.AdvanceStationsToHalfDay(100, S);
		TestEqual(TEXT("Station half-day clock cannot finish utility"),
		          Dev.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::Low);
		Dev.AdvanceToStep(4, City, Catalog, S, {}, {}, {});
		TestEqual(TEXT("No early upgrade"), Dev.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::Low);
		FCityDevelopmentSimulation Loaded;
		TestTrue(TEXT("Pending utility resumes"),
		         Loaded.RestoreState(Dev.GetSnapshot(), City, Catalog, 4, Error));
		TestEqual(TEXT("Resume retains deadline"), Loaded.GetSnapshot().Find(Id)->CivicUpgradeCompleteStep,
		          int64(5));
		S.EntityFailures.Add(Id, ECityOperationFailure::FireDamage);
		Loaded.AdvanceToStep(5, City, Catalog, S, {}, {}, {});
		TestTrue(TEXT("Damage holds completion"), Loaded.GetSnapshot().Find(Id)->HasCivicUpgrade());
		S.EntityFailures.Reset();
		Loaded.AdvanceToStep(6, City, Catalog, S, {}, {}, {});
		TestEqual(TEXT("Recovery completes expansion"), Loaded.GetSnapshot().Find(Id)->DensityLevel,
		          ECityDensityLevel::Medium);
		TestTrue(TEXT("Stable identity/footprint"),
		         City.GetEntities().FindChecked(Id).Anchor == Original.Anchor &&
		             City.GetEntities().FindChecked(Id).DefinitionId == Original.DefinitionId);
		TestFalse(TEXT("Stale level blocked"),
		          Loaded.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestTrue(TEXT("Final expansion starts"),
		         Loaded.PurchaseCivicUpgrade(Id, ECityDensityLevel::Medium, City, Catalog, S, Error));
		Loaded.AdvanceToStep(16, City, Catalog, S, {}, {}, {});
		TestEqual(TEXT("Final stage completes"), Loaded.GetSnapshot().Find(Id)->DensityLevel,
		          ECityDensityLevel::High);
		TestFalse(TEXT("Maximum does not charge"),
		          Loaded.PurchaseCivicUpgrade(Id, ECityDensityLevel::High, City, Catalog, S, Error));
		for (const auto& L : D->DensityLevels)
		{
			auto* Mesh = L.PresentationMesh.LoadSynchronous();
			if (TestNotNull(TEXT("Stage mesh"), Mesh))
			{
				const int32 Stage = static_cast<int32>(L.Level);
				const int32 PowerCounts[] = {540, 588, 700};
				const int32 WaterCounts[] = {2228, 3004, 3404};
				TestEqual(TEXT("Utility retains reviewed brick/asphalt sections"), Mesh->GetNumSections(0),
				          4);
				TestEqual(TEXT("Accepted utility detail count"), Mesh->GetNumTriangles(0),
				          D->DefinitionId == TEXT("Power") ? PowerCounts[Stage] : WaterCounts[Stage]);
			}
			const FString Package =
			    L.PresentationMesh.ToSoftObjectPath().GetLongPackageName() + TEXT("_Damaged");
			TestNotNull(
			    TEXT("Damaged stage mesh"),
			    LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + FPaths::GetBaseFilename(Package))));
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilitySupplyRecoveryTest,
                                 "CitySim.Utilities.SupplyOutagePayrollAndBudget",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilitySupplyRecoveryTest::RunTest(const FString& Parameters)
{
	auto* C = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), C))
		return false;
	FCityPlacementModel City(100000);
	UtilityRoad(City);
	uint64 Power = PlaceUtility(City, C, TEXT("Power"), 8),
	       Water = PlaceUtility(City, C, TEXT("Service"), 12);
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, C, 0);
	FCityEconomyTuning T;
	FCityPopulationSnapshot P;
	FCityFireSnapshot Fire;
	for (int32 Stage = 0; Stage < 3; ++Stage)
	{
		auto D = Dev.GetSnapshot();
		for (uint64 Id : {Power, Water})
			D.States.FindChecked(Id).DensityLevel = static_cast<ECityDensityLevel>(Stage);
		auto Build = [&]() {
			return FCityServiceEvaluator::BuildSnapshot(City, T, C, &D, nullptr, &P, &Fire);
		};
		P = {};
		auto S = Build();
		TestEqual(TEXT("Per-stage unstaffed power floor"), S.PowerCapacity, 100 * (Stage + 1));
		TestEqual(TEXT("Per-stage unstaffed water floor"), S.ServiceCapacity, 100 * (Stage + 1));
		for (uint64 Id : {Power, Water})
		{
			const int32 Jobs = 2 * (Stage + 1);
			P.PublicEmployers.Add({Id, Jobs, Jobs * 10, Jobs * 10});
			for (int32 I = 0; I < Jobs; ++I)
			{
				FCityCitizenRecord Citizen;
				Citizen.Id = P.Citizens.Num() + 1;
				Citizen.WorkplaceEntityId = Id;
				P.Citizens.Add(Citizen);
			}
		}
		S = Build();
		TestEqual(TEXT("Completed capacity with full paid staff"), S.PowerCapacity, 200 * (Stage + 1));
		TestEqual(TEXT("Completed water capacity"), S.ServiceCapacity, 200 * (Stage + 1));
		TestEqual(TEXT("Public jobs scale"), S.PublicJobs.FindRef(Power), 2 * (Stage + 1));
		FCityEconomySimulation E;
		E.Advance(T.FixedStepSeconds, City, T, C, &D, &P);
		const auto B = E.GetProjectedBudget(City, T);
		TestEqual(TEXT("Upgrade upkeep belongs to utilities once"), B.UtilityUpkeep,
		          int64(Stage == 0   ? 80
		                : Stage == 1 ? 120
		                             : 160));
		Fire.DamagedBuildings.Add(Power, {100, -1, 1});
		S = Build();
		TestEqual(TEXT("Earthquake damage stops all supply"), S.PowerCapacity, 0);
		TestEqual(TEXT("Independent water remains"), S.ServiceCapacity, 200 * (Stage + 1));
		TestEqual(TEXT("Damaged provider has no available jobs"), S.PublicJobs.FindRef(Power), 0);
		Fire.DamagedBuildings.FindChecked(Power).RepairCompleteStep = 10;
		TestEqual(TEXT("Repairing utility still unavailable"), Build().PowerCapacity, 0);
		FCityFireSimulation Repair;
		FString Error;
		Fire.DamagedBuildings.FindChecked(Power).RepairCompleteStep = -1;
		if (!TestTrue(TEXT("Damaged utility restores for actual repair"),
		              Repair.RestoreState(Fire, Fire.LastFixedStepId, Error)))
		{
			AddError(Error);
			return false;
		}
		const int64 Before = City.GetFunds();
		if (!TestTrue(TEXT("Paid utility repair starts"),
		              Repair.Repair(Power, City, *C->FindDefinition(TEXT("Power")), T, Error)))
		{
			AddError(Error);
			return false;
		}
		TestTrue(TEXT("Repair charges treasury"), City.GetFunds() < Before);
		const auto Deadline = Repair.GetSnapshot().DamagedBuildings.FindChecked(Power).RepairCompleteStep;
		Repair.AdvanceToStep(Deadline, City, S, D, {}, T, false);
		TestFalse(TEXT("Actual repair clears damage"), Repair.GetSnapshot().DamagedBuildings.Contains(Power));
		Fire = Repair.GetSnapshot();
		TestEqual(TEXT("Repair restores completed stage output"), Build().PowerCapacity, 200 * (Stage + 1));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityPersistenceTest, "CitySim.Persistence.UtilityExpansion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityPersistenceTest::RunTest(const FString& Parameters)
{
	auto* C = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), C))
		return false;
	FCityPlacementModel City(100000);
	UtilityRoad(City);
	uint64 Id = PlaceUtility(City, C, TEXT("Power"), 8);
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, C, 0);
	FCityServiceSnapshot S;
	FString Error, Json;
	TestTrue(TEXT("Starts expansion"),
	         Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, C, S, Error));
	FCitySaveDocument D;
	D.Placement = City.ExportState();
	D.Development = Dev.GetSnapshot();
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("Pending utility serializes"), FCityPersistence::Serialize(D, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("Pending utility deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Saved utility deadline unchanged"), Loaded.Development.Find(Id)->CivicUpgradeCompleteStep,
	          int64(5));
	return true;
}
#endif
