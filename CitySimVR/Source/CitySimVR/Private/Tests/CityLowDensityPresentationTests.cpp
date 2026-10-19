#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityPrivateEntrances.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLowDensityAssetsTest, "CitySim.Art.LowDensityVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLowDensityAssetsTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Production catalog"), Catalog)) return false;
	TestEqual(TEXT("Twelve coordinated appearances"), Catalog->LowDensityPresentationVariants.Num(), 12);
	for (const FName Id : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory"))})
	{
		TSet<FName> Appearances; TSet<FString> Signs;
		for (uint64 Entity = 1; Entity <= 512; ++Entity)
		{
			FName Appearance, DamagedAppearance;
			const auto Healthy = Catalog->FindLowDensityPresentation(Entity, Id, ECityDensityLevel::Low, false, Appearance);
			const auto Damaged = Catalog->FindLowDensityPresentation(Entity, Id, ECityDensityLevel::Low, true, DamagedAppearance);
			TestFalse(TEXT("Healthy asset reference"), Healthy.IsNull());
			TestFalse(TEXT("Damage asset reference"), Damaged.IsNull());
			TestEqual(TEXT("Damage retains shape identity"), DamagedAppearance, Appearance);
			Appearances.Add(Appearance);
			if (Id == TEXT("Shop")) Signs.Add(Healthy.ToString());
			TestEqual(TEXT("Repeated selection is stable"), Catalog->FindLowDensityPresentation(Entity, Id, ECityDensityLevel::Low, false, DamagedAppearance).ToString(), Healthy.ToString());
		}
		TestEqual(TEXT("All four appearances reached"), Appearances.Num(), 4);
		if (Id == TEXT("Shop")) TestEqual(TEXT("Eight independent signs on four shapes"), Signs.Num(), 32);
		FName Invalid;
		TestTrue(TEXT("Construction preview ID has no random choice"), Catalog->FindLowDensityPresentation(0, Id, ECityDensityLevel::Low, false, Invalid).IsNull());
		TestTrue(TEXT("Medium density remains existing family"), Catalog->FindLowDensityPresentation(1, Id, ECityDensityLevel::Medium, false, Invalid).IsNull());
		TestTrue(TEXT("High density remains existing family"), Catalog->FindLowDensityPresentation(1, Id, ECityDensityLevel::High, false, Invalid).IsNull());
	}
	for (const auto& Variant : Catalog->LowDensityPresentationVariants)
	{
		TArray<TSoftObjectPtr<UStaticMesh>> Meshes{Variant.PresentationMesh, Variant.DamagedMesh};
		for (const auto& Pair : Variant.BusinessMeshes) Meshes.Add(Pair.Value);
		for (const auto& Reference : Meshes)
		{
			auto* Mesh = Reference.LoadSynchronous();
			if (!TestNotNull(TEXT("Cookable production mesh loads"), Mesh)) continue;
			TestTrue(TEXT("Actual geometry"), Mesh->GetNumTriangles(0) > 100);
			TestTrue(TEXT("Semantic sections retained"), Mesh->GetNumSections(0) >= 4);
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				auto* Material = Mesh->GetMaterial(Slot);
				if (TestNotNull(TEXT("Bound material exists"), Material))
					TestEqual(TEXT("Opaque building material"), Material->GetBlendMode(), BLEND_Opaque);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLowDensityLifecycleTest, "CitySim.Presentation.LowDensityLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLowDensityLifecycleTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LowDensityLifecycle"), nullptr, true, ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>(); auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->SimulationSubsystem = Sim; const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/LowDensity-Lifecycle.json");
	FString Json, Error;
	do
	{
		if (!TestNotNull(TEXT("Catalog"), Board->BuildingCatalog.Get())) break;
		Sim->RegisterBuildingCatalog(Board->BuildingCatalog);
		FCitySaveDocument Doc; Doc.bSimulationPaused = true; Doc.Fire.ActivationStep = Doc.Fire.GraceEndStep = 0;
		Doc.Terrain.BroadElevationAmplitudeCm = Doc.Terrain.HillAmplitudeCm = Doc.Terrain.RidgeAmplitudeCm = 0;
		FCityTerrainHeightfield Terrain; Terrain.Generate(Doc.Terrain); FCityPlacementModel City(100000); City.SetTerrain(&Terrain);
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell); City.UpdateRoadPreviewEnd(Road, {40, 8});
		if (!TestTrue(TEXT("Fixture road"), City.CommitRoad(Road).IsSuccess())) break;
		TMap<FName, uint64> Entities; int32 X = 1;
		for (const FName Type : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory"))})
		{
			const auto* Definition = Board->BuildingCatalog->FindDefinition(Type);
			if (!TestNotNull(TEXT("Definition"), Definition)) continue;
			auto Preview = City.BeginPreview(*Definition, {X, 9}); const auto Placed = City.CommitPlacement(Preview, *Definition);
			TestTrue(TEXT("Actual placement"), Placed.IsSuccess()); Entities.Add(Type, Placed.EntityId); X += 5;
			FCityDevelopmentState State; State.EntityId = Placed.EntityId; State.Stage = ECityDevelopmentStage::Operational;
			Doc.Development.States.Add(Placed.EntityId, State);
		}
		Doc.Placement = City.ExportState();
		for (const auto Density : {ECityDensityLevel::Low, ECityDensityLevel::Medium, ECityDensityLevel::High, ECityDensityLevel::Low})
			for (const bool Damaged : {false, true, false})
			{
				for (const auto& Pair : Entities)
				{
					Doc.Development.States.FindChecked(Pair.Value).DensityLevel = Density;
					if (Damaged) Doc.Fire.DamagedBuildings.Add(Pair.Value, FCityFireDamage{50, -1, 0});
					else Doc.Fire.DamagedBuildings.Remove(Pair.Value);
				}
				if (!TestTrue(TEXT("Current-format save/reload"), FCityPersistence::Serialize(Doc, Json, Error) && FCityPersistence::WriteAtomic(Path, Json, Error) && Sim->LoadCityFromPath(Path, Error, false)))
				{ AddError(Error); break; }
				FString Before, After; Sim->SaveCityToPath(Path, Error); FFileHelper::LoadFileToString(Before, *Path);
				Board->PrepareCityView();
				for (const auto& Pair : Entities)
				{
					const auto& Definition = *Board->BuildingCatalog->FindDefinition(Pair.Key);
					const auto Resolved = Board->ResolvePresentationDefinition(Pair.Value, Definition);
					FName Appearance; const auto Expected = Board->BuildingCatalog->FindLowDensityPresentation(Pair.Value, Pair.Key, Density, Damaged, Appearance);
					if (Density == ECityDensityLevel::Low)
					{
						// Private presentation replaces the authored ground walk while retaining
						// the catalog's stable appearance and healthy/damaged lifecycle selection.
						FString ExpectedPath = Expected.ToString();
						if (Pair.Key == TEXT("Home") || Pair.Key == TEXT("Shop") || Pair.Key == TEXT("Factory"))
							if (const auto* Entrance = CityPrivateEntrances::Find(FPaths::GetBaseFilename(Expected.ToSoftObjectPath().GetLongPackageName())))
								ExpectedPath = CityResidentialEntrances::ObjectPath(*Entrance);
						TestEqual(TEXT("Real board resolves matching healthy/damage shape"), Resolved.PresentationMesh.ToString(), ExpectedPath);
						TestTrue(TEXT("Pool key includes appearance"), Resolved.DefinitionId.ToString().Contains(Appearance.ToString()));
					}
					else
                    {
                        TestFalse(TEXT("Density upgrade leaves low family"), Resolved.PresentationMesh.ToString().Contains(TEXT("/LowDensity/")));
                        if(Pair.Key==TEXT("Home") || Pair.Key==TEXT("Factory"))
                        {
                            FString Original=Definition.FindDensityLevel(Density)->PresentationMesh.GetAssetName();
                            if(Damaged) Original+=TEXT("_Damaged");
                            const auto* Entry=CityPrivateEntrances::Find(Original);
                            if(TestNotNull(TEXT("Actual density has a ground profile"),Entry))
                                TestEqual(TEXT("Healthy/fire/repaired density uses the pathless body"),Resolved.PresentationMesh.ToString(),CityResidentialEntrances::ObjectPath(*Entry));
                        }
                    }
					auto* Pool = Board->GetOrCreateInstances(Resolved);
					TestEqual(TEXT("Actual pool mesh"), Pool->GetStaticMesh().Get(), Resolved.PresentationMesh.LoadSynchronous());
					TestFalse(TEXT("No per-building/pool ticking"), Pool->PrimaryComponentTick.bCanEverTick);
				}
				Sim->SaveCityToPath(Path, Error); FFileHelper::LoadFileToString(After, *Path);
				TestEqual(TEXT("Presentation leaves simulation save unchanged"), Before, After);
			}
		// Actual construction state and completion use the normal catalog. Fire
        // damage presentation remains an operational-only existing rule.
        for(const FName Type:{FName(TEXT("Home")),FName(TEXT("Factory"))})
        {
        const uint64 StagedEntity=Entities.FindChecked(Type);
        const auto& StagedDefinition=*Board->BuildingCatalog->FindDefinition(Type);
        for(const auto Stage:{ECityDevelopmentStage::Construction,ECityDevelopmentStage::Operational})
        {
            Doc.Development.States.FindChecked(StagedEntity).Stage=Stage;
            Doc.Development.States.FindChecked(StagedEntity).DensityLevel=ECityDensityLevel::High;
            Doc.Fire.DamagedBuildings.Remove(StagedEntity);
            if(!TestTrue(TEXT("Construction/completion current save loads"),FCityPersistence::Serialize(Doc,Json,Error) && FCityPersistence::WriteAtomic(Path,Json,Error) && Sim->LoadCityFromPath(Path,Error,false))) break;
            Board->PrepareCityView();
            const auto Resolved=Board->ResolvePresentationDefinition(StagedEntity,StagedDefinition);
            const FString Name=Stage==ECityDevelopmentStage::Construction?TEXT("SM_ResidentialGround_Construction"):(Type==TEXT("Home")?TEXT("SM_ResidentialGround_High"):TEXT("SM_IndustrialGround_Building_Factory_High_A"));
            TestEqual(TEXT("Stage selects its dedicated pathless asset"),Resolved.PresentationMesh.GetAssetName(),Name);
            auto* Pool=Board->GetOrCreateInstances(Resolved);
            TestTrue(TEXT("Stage pool actually draws the selected mesh"),Pool->GetStaticMesh()==Resolved.PresentationMesh.LoadSynchronous() && Pool->GetInstanceCount()==1);
        }
        }
	} while (false);
	IFileManager::Get().Delete(*Path); IFileManager::Get().Delete(*(Path + TEXT(".bak"))); World->DestroyWorld(false); return true;
}
#endif
