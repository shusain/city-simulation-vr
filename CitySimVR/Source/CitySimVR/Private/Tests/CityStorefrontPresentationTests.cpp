#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStorefrontAssetsTest, "CitySim.Art.StorefrontAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStorefrontAssetsTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Production catalog loads"), Catalog))
		return false;
	TestEqual(TEXT("Eight storefront businesses are cooked catalog references"),
	          Catalog->CommercialPresentationVariants.Num(), 8);
	TSet<FName> Businesses;
	for (const auto& Variant : Catalog->CommercialPresentationVariants)
	{
		TestFalse(TEXT("Business identity is unique"),
		          Variant.BusinessId.IsNone() || Businesses.Contains(Variant.BusinessId));
		Businesses.Add(Variant.BusinessId);
		TestEqual(TEXT("Three normal density variants"), Variant.DensityMeshes.Num(), 3);
		TestEqual(TEXT("Three damaged density variants"), Variant.DamagedDensityMeshes.Num(), 3);
		for (const auto& Meshes : {Variant.DensityMeshes, Variant.DamagedDensityMeshes})
			for (int32 Density = 0; Density < Meshes.Num(); ++Density)
			{
				auto* Mesh = Meshes[Density].LoadSynchronous();
				if (!TestNotNull(TEXT("Actual production sign mesh loads"), Mesh))
					continue;
				TestFalse(TEXT("Runtime mesh is outside the prototype directory"),
				          Mesh->GetPathName().Contains(TEXT("Prototypes")));
				const bool bDamaged = Mesh->GetName().Contains(TEXT("Damaged"));
				TestEqual(TEXT("Intact frames add one shared opaque section; damage stays unchanged"),
				          Mesh->GetNumSections(0), bDamaged ? 4 : 5);
				const int32 IntactCounts[] = {470, 1682, 2906};
				const int32 DamagedCounts[] = {118, 178, 250};
				TestEqual(TEXT("Sign choices retain the reviewed framed/damaged geometry"),
				          Mesh->GetNumTriangles(0),
				          bDamaged ? DamagedCounts[Density] : IntactCounts[Density]);
			}
	}
	TSet<FName> Choices;
	for (uint64 Id = 1; Id <= 128; ++Id)
	{
		FName Business, Again;
		Catalog->FindCommercialPresentation(Id, ECityDensityLevel::Low, false, Business);
		Choices.Add(Business);
		for (auto Density : {ECityDensityLevel::Low, ECityDensityLevel::Medium, ECityDensityLevel::High})
			for (bool Damaged : {false, true})
			{
				TestFalse(TEXT("Valid stable selection exists"),
				          Catalog->FindCommercialPresentation(Id, Density, Damaged, Again).IsNull());
				TestEqual(TEXT("Upgrade and damage retain business identity"), Again, Business);
			}
	}
	TestEqual(TEXT("Ordinary saved IDs reach all eight styles"), Choices.Num(), 8);
	FName Invalid;
	TestTrue(TEXT("Unplaced preview has no random style"),
	         Catalog->FindCommercialPresentation(0, ECityDensityLevel::Low, false, Invalid).IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStorefrontPresentationTest,
                                 "CitySim.Presentation.StorefrontSavedIdentityAndPools",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStorefrontPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("StorefrontPresentationTest"), nullptr,
	                                  true, ERHIFeatureLevel::Num, &Init);
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->SimulationSubsystem = Simulation;
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/StorefrontPresentation.json");
	FString Json, Error;
	do
	{
		if (!TestNotNull(TEXT("Authored catalog exists"), Board->BuildingCatalog.Get()))
			break;
		Simulation->RegisterBuildingCatalog(Board->BuildingCatalog);
		const auto* Shop = Board->BuildingCatalog->FindDefinition(TEXT("Shop"));
		if (!TestNotNull(TEXT("Shop definition exists"), Shop))
			break;
		FCitySaveDocument Doc;
		Doc.bSimulationPaused = true;
		Doc.Terrain.BroadElevationAmplitudeCm = Doc.Terrain.HillAmplitudeCm = Doc.Terrain.RidgeAmplitudeCm =
		    0;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(Doc.Terrain);
		FCityPlacementModel City(100000);
		City.SetTerrain(&Terrain);
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, {40, 8});
		if (!TestTrue(TEXT("Road fixture commits"), City.CommitRoad(Road).IsSuccess()))
			break;
		auto Preview = City.BeginPreview(*Shop, {8, 9});
		const auto Placed = City.CommitPlacement(Preview, *Shop);
		if (!TestTrue(TEXT("Saved shop commits"), Placed.IsSuccess()))
			break;
		Doc.Placement = City.ExportState();
		const uint64 Id = Placed.EntityId;
		Doc.Fire.ActivationStep = 0;
		Doc.Fire.GraceEndStep = 0;
		FCityDevelopmentState State;
		State.EntityId = Id;
		State.Stage = ECityDevelopmentStage::Operational;
		FName Business;
		Board->BuildingCatalog->FindCommercialPresentation(Id, ECityDensityLevel::Low, false, Business);
		for (auto Density : {ECityDensityLevel::Low, ECityDensityLevel::Medium, ECityDensityLevel::High})
			for (bool Damaged : {false, true, false})
			{
				State.DensityLevel = Density;
				Doc.Development.States.Add(Id, State);
				if (Damaged)
					Doc.Fire.DamagedBuildings.Add(Id, FCityFireDamage{50, -1, 0});
				else
					Doc.Fire.DamagedBuildings.Remove(Id);
				if (!TestTrue(TEXT("Saved fixture reloads"),
				              FCityPersistence::Serialize(Doc, Json, Error) &&
				                  FCityPersistence::WriteAtomic(Path, Json, Error) &&
				                  Simulation->LoadCityFromPath(Path, Error, false)))
				{
					AddError(Error);
					break;
				}
				FString Before, After;
				TestTrue(TEXT("Snapshot before presentation saves"), Simulation->SaveCityToPath(Path, Error));
				FFileHelper::LoadFileToString(Before, *Path);
				Board->PrepareCityView();
				const auto Resolved = Board->ResolvePresentationDefinition(Id, *Shop);
				FName Selected;
				auto Expected =
				    Board->BuildingCatalog->FindCommercialPresentation(Id, Density, Damaged, Selected);
				FName Appearance;
				const auto LowMesh = Board->BuildingCatalog->FindLowDensityPresentation(Id, TEXT("Shop"), Density, Damaged, Appearance);
				if (!LowMesh.IsNull())
					Expected = LowMesh;
				TestEqual(TEXT("Real board chooses saved density/damage variant"),
				          Resolved.PresentationMesh.ToSoftObjectPath(), Expected.ToSoftObjectPath());
				TestEqual(TEXT("Repair and reload keep the same business"), Selected, Business);
				TestTrue(TEXT("Pool key includes business identity"),
				         Resolved.DefinitionId.ToString().Contains(Business.ToString()));
				auto* Pool = Board->GetOrCreateInstances(Resolved);
				TestEqual(TEXT("Pool uses the actual chosen asset"), Pool->GetStaticMesh().Get(),
				          Expected.LoadSynchronous());
				TestFalse(TEXT("No per-store component ticking"), Pool->PrimaryComponentTick.bCanEverTick);
				TestTrue(TEXT("Snapshot after presentation saves"), Simulation->SaveCityToPath(Path, Error));
				FFileHelper::LoadFileToString(After, *Path);
				TestEqual(TEXT("Presentation cannot change saved simulation"), Before, After);
			}
	} while (false);
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	World->DestroyWorld(false);
	return true;
}
#endif
