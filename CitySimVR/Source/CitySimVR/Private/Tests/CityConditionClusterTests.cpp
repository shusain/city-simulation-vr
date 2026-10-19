#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Presentation/CityConditionIndicators.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionClusterModelTest,
                                 "CitySim.Indicators.ClusterTotalsTypesAndRegions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionClusterModelTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FCityBuildingDefinition Def;
	Def.DefinitionId = TEXT("Home");
	Def.DisplayName = TEXT("Home");
	Def.Footprint = {1, 1};
	Def.Cost = 1;
	auto Place = [&](FCityCellCoord Cell) {
		auto Preview = City.BeginPreview(Def, Cell);
		return City.CommitPlacement(Preview, Def).EntityId;
	};
	const uint64 A = Place({4, 12}), B = Place({7, 12}), C = Place({10, 12}), Far = Place({32, 20}),
	             Damaged = Place({6, 14}), Hospital = Place({5, 16});
	FCityPopulationSnapshot People;
	FCityHealthSnapshot Health;
	FCityFireSnapshot Fire;
	FCityCrimeSnapshot Crime;
	auto Add = [&](uint64 Home, int32 Count) {
		for (int32 I = 0; I < Count; ++I)
		{
			auto& Citizen = People.Citizens.Emplace_GetRef();
			Citizen.Id = People.Citizens.Num();
			Citizen.HomeEntityId = Home;
			auto& Patient = Health.Patients.Emplace_GetRef();
			Patient.CitizenId = Citizen.Id;
		}
	};
	Add(A, 4);
	Add(B, 6);
	Add(C, 3);
	Add(Far, 2);
	Add(Damaged, 1);
	for (int32 I = 0; I < 3; ++I)
	{
		Health.Patients[I].HospitalId = Hospital;
		Health.Patients[I].Illness = ECityIllness::Severe;
		Health.Patients[I].Care = ECityHealthCare::Hospital;
	}
	Fire.DamagedBuildings.Add(Damaged, {40, -1});
	auto Rows = FCityConditionIndicators::Build(City, Fire, Crime, 10, &Health, &People);
	TestEqual(TEXT("Nearby illness, distant illness, repair and care stay distinct"), Rows.Num(), 4);
	const auto* Group = Rows.FindByPredicate([&](const auto& R) { return R.EntityIds.Contains(A); });
	if (!TestNotNull(TEXT("Illness neighborhood exists"), Group))
		return false;
	TestEqual(TEXT("4 plus 6 plus 3 residents share ILL13"), Group->Title(0), FString(TEXT("ILL 13")));
	TestEqual(TEXT("Every source building belongs to the shared popup"), Group->EntityIds.Num(), 3);
	TestTrue(TEXT("Region has a bounded diameter"),
	         Group->RegionBounds.GetSize().Size() <= FCityConditionIndicators::MaximumClusterDiameterCells);
	TestTrue(TEXT("Treatment totals have their own marker"), Rows.ContainsByPredicate([&](const auto& R) {
		return R.EntityId == Hospital && R.PatientsInCare == 3 && R.IllResidents == 0;
	}));
	TestTrue(TEXT("Repair remains actionable beside a health cluster"),
	         Rows.ContainsByPredicate([&](const auto& R) {
		         return R.EntityId == Damaged && R.bNeedsRepair && R.IllResidents == 1;
	         }));
	auto Excluded = FCityConditionIndicators::Build(City, Fire, Crime, 10, &Health, &People, B);
	TestTrue(TEXT("Inspecting one member retains the other two and their total"),
	         Excluded.ContainsByPredicate([&](const auto& R) {
		         return R.EntityIds.Contains(A) && R.IllResidents == 7 && R.EntityIds.Num() == 2 &&
		                !R.EntityIds.Contains(B);
	         }));
	Algo::Reverse(Health.Patients);
	Algo::Reverse(People.Citizens);
	const auto Reverse = FCityConditionIndicators::Build(City, Fire, Crime, 10, &Health, &People);
	for (int32 I = 0; I < Rows.Num(); ++I)
	{
		TestTrue(TEXT("Snapshot iteration order cannot change cluster membership"),
		         Rows[I].EntityIds == Reverse[I].EntityIds);
		TestEqual(TEXT("Totals stay deterministic"), Rows[I].Title(0), Reverse[I].Title(0));
	}
	// A succession of close buildings must not join opposite ends of the city.
	FCityPlacementModel ChainCity(100000);
	FCityFireSnapshot ChainFire;
	for (int32 X = 0; X < 60; X += 3)
	{
		auto Preview = ChainCity.BeginPreview(Def, {X, 2});
		const uint64 Id = ChainCity.CommitPlacement(Preview, Def).EntityId;
		ChainFire.DamagedBuildings.Add(Id, {40, -1});
	}
	const auto Chain = FCityConditionIndicators::Build(ChainCity, ChainFire, Crime, 10);
	TestTrue(TEXT("Neighbor chains split into local regions"), Chain.Num() > 1);
	int32 Members = 0;
	for (const auto& R : Chain)
	{
		Members += R.EntityIds.Num();
		TestTrue(TEXT("Every region remains compact"),
		         R.RegionBounds.GetSize().Size() <= FCityConditionIndicators::MaximumClusterDiameterCells);
	}
	TestEqual(TEXT("Aggregation does not lose members"), Members, 20);
	// Twenty nearby markers previously lost four sources to the16-popup limit.
	FCityPlacementModel Dense(100000);
	FCityPopulationSnapshot DensePeople;
	FCityHealthSnapshot DenseHealth;
	for (int32 I = 0; I < 20; ++I)
	{
		auto Preview = Dense.BeginPreview(Def, {(I % 5) * 2, 20 + (I / 5) * 2});
		const uint64 Id = Dense.CommitPlacement(Preview, Def).EntityId;
		FCityCitizenRecord Person;
		Person.Id = I + 1;
		Person.HomeEntityId = Id;
		DensePeople.Citizens.Add(Person);
		FCityHealthPatient Patient;
		Patient.CitizenId = Person.Id;
		DenseHealth.Patients.Add(Patient);
	}
	const auto DenseRows =
	    FCityConditionIndicators::Build(Dense, FCityFireSnapshot(), Crime, 10, &DenseHealth, &DensePeople);
	int32 Total = 0;
	for (const auto& R : DenseRows)
		Total += R.IllResidents;
	TestEqual(TEXT("Totals include all sources before the popup cap"), Total, 20);
	TestTrue(TEXT("Dense illness uses fewer popups"), DenseRows.Num() < 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionClusterPresentationTest,
                                 "CitySim.Indicators.ClusterLeadersSelectionAndCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionClusterPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConditionClusters"), nullptr, true,
	                                ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = W->SpawnActor<ACityBoardActor>();
	auto* UI = W->SpawnActor<ACityVRToolController>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->DispatchBeginPlay();
	UI->Board = Board;
	Board->SetActorTransform(FTransform(FRotator(0, 20, 0), FVector(-80, -40, 80), FVector(.004)));
	Board->SetToolMode(ECityToolMode::Inspect);
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocationAndRotation(FVector(0, 0, 160), FRotator(-45, 0, 0));
	UI->HeadsetCamera = Camera;
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	const auto* Home = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
	if (!TestNotNull(TEXT("Native cluster fixture has homes"), Home))
	{
		W->DestroyWorld(false);
		return false;
	}
	TArray<uint64> Ids;
	for (int32 X : {4, 7, 10})
	{
		auto Preview = City.BeginPreview(*Home, {X, 12});
		Ids.Add(City.CommitPlacement(Preview, *Home).EntityId);
	}
	Sim->Development.InitializeExistingAsOperational(City, Board->BuildingCatalog, 0);
	FCityPopulationSnapshot People;
	const int32 Counts[] = {4, 6, 3};
	FString Error;
	for (int32 I = 0; I < 3; ++I)
	{
		FCityHouseholdRecord Household;
		Household.Id = Ids[I];
		Household.HomeEntityId = Ids[I];
		People.Households.Add(Household);
		for (int32 J = 0; J < Counts[I]; ++J)
		{
			auto& C = People.Citizens.Emplace_GetRef();
			C.Id = People.Citizens.Num();
			C.HouseholdId = Ids[I];
			C.HomeEntityId = C.CurrentLocationEntityId = Ids[I];
			C.UnemployedSinceStep = 0;
			Sim->Health.StartIllness(C.Id, ECityIllness::Mild);
		}
	}
	People.NextCitizenId = People.Citizens.Num() + 1;
	TestTrue(TEXT("Native citizen fixture validates"), Sim->Population.RestoreState(People, Error));
	Board->RebuildAllPresentation();
	// Isolate illness aggregation from unconnected utility prerequisites.
	Sim->ServiceSnapshot.EntityFailures.Reset();
	UI->UpdateConditionIndicators();
	if (!TestEqual(TEXT("Three nearby sick homes use one actual popup"), UI->ConditionIndicators.Num(), 1))
	{
		W->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Actual popup displays the combined13 residents"), UI->ConditionTitles[0]->Text.ToString(),
	          FString(TEXT("3 lots")));
	TestEqual(TEXT("One instanced leader reaches each building"),
	          UI->ConditionLeaderInstances->GetInstanceCount(), 3);
	TestEqual(TEXT("Shared leaders do not block raycast or placement"),
	          UI->ConditionLeaderInstances->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	const FVector End = UI->ConditionRoots[0]->GetComponentTransform().TransformPosition(
	    FVector(0, 0, -FCityConditionIndicators::HalfHeight));
	for (int32 I = 0; I < 3; ++I)
	{
		FVector Roof;
		Board->GetBuildingMarkerAnchor(Ids[I], Roof);
		FTransform Line;
		UI->ConditionLeaderInstances->GetInstanceTransform(I, Line, true);
		const FVector Half = Line.GetRotation().GetForwardVector() * Line.GetScale3D().X * 50;
		TestTrue(TEXT("Each line begins at its own roof"), (Line.GetLocation() - Half).Equals(Roof, .01));
		TestTrue(TEXT("All lines terminate at the same info box"),
		         (Line.GetLocation() + Half).Equals(End, .01));
		TestEqual(
		    TEXT("Cluster selection finds the pointed member"),
		    UI->PickConditionMember(0, Camera->GetComponentLocation(), Roof - Camera->GetComponentLocation()),
		    Ids[I]);
	}
	FString Before, After;
	TestTrue(TEXT("Presentation fixture serializes"),
	         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Before, Error));
	UI->InspectedEntityId = Ids[1];
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Inspected member does not hide its neighbors"), UI->ConditionTitles[0]->Text.ToString(),
	          FString(TEXT("2 lots")));
	TestEqual(TEXT("Only remaining member lines render"), UI->ConditionLeaderInstances->GetInstanceCount(),
	          2);
	UI->InspectedEntityId = 0;
	UI->SetConditionIndicatorsVisible(false);
	TestEqual(TEXT("Hide removes all leader instances"), UI->ConditionLeaderInstances->GetInstanceCount(), 0);
	TestFalse(TEXT("Hidden leader pool is invisible"), UI->ConditionLeaderInstances->IsVisible());
	UI->SetConditionIndicatorsVisible(true);
	Board->SetActorScale3D(FVector(.008));
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Board zoom retains cluster membership"), UI->ConditionIndicators[0].EntityIds.Num(), 3);
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Clustering and selection do not change authority"), After, Before);
	const FString Preview =
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/ConditionClusters-City.json"));
	FFileHelper::SaveStringToFile(Before, *Preview);
	auto Health = Sim->Health.GetSnapshot();
	Health.Patients.RemoveAll([](const auto& P) { return P.CitizenId <= 4; });
	Sim->Health.RestoreState(Health, Error);
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Recovery updates the combined total"), UI->ConditionTitles[0]->Text.ToString(),
	          FString(TEXT("2 lots")));
	TestEqual(TEXT("Recovered homes lose their lines"), UI->ConditionLeaderInstances->GetInstanceCount(), 2);
	City.DemolishEntity(Ids[2]);
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Demolition drops stale sources immediately"), UI->ConditionTitles[0]->Text.ToString(),
	          FString(TEXT("1 lot")));
	TestEqual(TEXT("Demolition removes its connection"), UI->ConditionLeaderInstances->GetInstanceCount(), 1);
	Board->SetToolMode(ECityToolMode::Home);
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Building tools retain informative cluster leaders"),
	          UI->ConditionLeaderInstances->GetInstanceCount(), 1);
	W->DestroyWorld(false);
	return true;
}
#endif
