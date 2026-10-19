#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityResidentialEntrances.h"
#include "Serialization/JsonSerializer.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityResidentialGroundCookedTest, "CitySim.Presentation.ResidentialGroundCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityResidentialGroundCookedTest::RunTest(const FString&)
{
    TSet<FString> Targets;
    for(const auto& Entry:CityResidentialEntrances::All()) Targets.Add(CityResidentialEntrances::ObjectPath(Entry));
    TestEqual(TEXT("Pilot, low-rise and six residential stage meshes"),Targets.Num(),15);
    for(const auto& Target:Targets)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Target);
        if(!TestNotNull(TEXT("Cooked residential body resolves"),Mesh)) continue;
        TestTrue(TEXT("Cooked body has explicit materials"),!Mesh->GetStaticMaterials().IsEmpty());
        for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(TEXT("Cooked residential material resolves"),Slot.MaterialInterface.Get());
        TestTrue(TEXT("Cooked body excludes the rigid ground walk"),Mesh->GetBoundingBox().Max.Y<799.);
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityResidentialGroundTest, "CitySim.Terrain.ResidentialGroundVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityResidentialGroundTest::RunTest(const FString&)
{
	FString Text;
	TSharedPtr<FJsonObject> Document;
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->SimulationSubsystem = World->GetSubsystem<UCitySimulationSubsystem>();
	int32 Checked = 0;
	for(const TCHAR* Manifest:{TEXT("city_residential_ground_paths.json"),TEXT("city_residential_stages_ground.json")})
	{
	if(!TestTrue(TEXT("Selected residential manifest loads"),FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("../ArtSource/Manifests")/Manifest))) ||
	   !TestTrue(TEXT("Selected manifest parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Document))) continue;
	for (const auto& Value : Document->GetArrayField(TEXT("assets")))
	{
		const auto Entry = Value->AsObject();
		const FString Target = Entry->GetStringField(TEXT("target"));
		const FString TargetObject = Target + TEXT(".") + FPaths::GetBaseFilename(Target);
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, *TargetObject);
		if (!TestNotNull(TEXT("Derived residential mesh exists"), Mesh)) continue;
		const auto* Profile = CityResidentialEntrances::Find(Mesh->GetName());
		if (!TestNotNull(TEXT("Derived residential entrance is declared"), Profile)) continue;
		const auto& Portal = Entry->GetArrayField(TEXT("entry_native_cm"));
		TestTrue(TEXT("Runtime entry matches measured source"), Profile->EntryCm.Equals(FVector(Portal[0]->AsNumber(),Portal[1]->AsNumber(),Portal[2]->AsNumber()),.01));
		TestEqual(TEXT("Landing uses retained porch contract"), Profile->LandingCm, Entry->GetNumberField(TEXT("landing_cm")));
		TestEqual(TEXT("Short frontage reserve matches source contract"), Profile->GroundReserveCm, Entry->GetNumberField(TEXT("ground_reserve_cm")));
        if(Entry->HasField(TEXT("path_width_cm")))
        {
            TestTrue(TEXT("Short apartment/construction frontage uses a turning flight"),Profile->bFrontageFlight);
            TestEqual(TEXT("Clear approach width matches measured source reserve"),Profile->WidthCm,Entry->GetNumberField(TEXT("path_width_cm")));
            TestEqual(TEXT("Doorway landing spans the authored entrance"),Profile->DoorWidthCm,Entry->GetNumberField(TEXT("door_width_cm")));
        }
		const auto& LOD = Mesh->GetRenderData()->LODResources[0];
		TestEqual(TEXT("Native triangles match checked FBX"), static_cast<int32>(LOD.GetNumTriangles()), static_cast<int32>(Entry->GetNumberField(TEXT("triangles"))));
		TestTrue(TEXT("Old ground walk no longer reaches the lot boundary"), Mesh->GetBoundingBox().Max.Y < 799.);
		const auto Bindings = Entry->GetObjectField(TEXT("bindings"));
		TestEqual(TEXT("All native semantic slots survive"), Mesh->GetStaticMaterials().Num(), Bindings->Values.Num());
		for (const auto& Slot : Mesh->GetStaticMaterials())
		{
			const auto* Material = Slot.MaterialInterface.Get();
			TestTrue(TEXT("Healthy/charred binding stays explicit"), Material && Bindings->HasField(Slot.ImportedMaterialSlotName.ToString()) &&
				Material->GetOutermost()->GetName() == Bindings->GetStringField(Slot.ImportedMaterialSlotName.ToString()));
		}
		const auto Supports = CityBuildingSupports::Find(Target);
		TestTrue(TEXT("Derived body retains measured structural support"), Supports.Num() == CityBuildingSupports::Find(Profile->SupportMesh).Num() && Supports.Num() > 0);
		if (Profile->EntryCm.Y > 600.)
		{
			bool OnDeck = false;
			for (int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3)
			{
				const FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
				const FVector B(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
				const FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
				if (FMath::Abs(A.Z-Profile->EntryCm.Z)+FMath::Abs(B.Z-Profile->EntryCm.Z)+FMath::Abs(C.Z-Profile->EntryCm.Z)>.1 || FMath::Abs(FVector::CrossProduct(B-A,C-A).Z)<.001) continue;
				const FVector Weights = FMath::ComputeBaryCentric2D(Profile->EntryCm,A,B,C);
				OnDeck |= Weights.X>=-.001 && Weights.Y>=-.001 && Weights.Z>=-.001;
			}
			TestTrue(TEXT("Porch step start touches the retained native deck"), OnDeck);
		}
		FCityBuildingDefinition Definition;
		Definition.DefinitionId=TEXT("Home"); Definition.ConstructionSteps=0;
		const FString Original = Entry->GetStringField(TEXT("native"));
		Definition.PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Original+TEXT(".")+FPaths::GetBaseFilename(Original)));
		const auto Resolved=Board->ResolvePresentationDefinition(0,Definition);
		TestEqual(TEXT("Real board resolves each original appearance to its derivative"),Resolved.PresentationMesh.Get(),Mesh);
		++Checked;
	}
	}
	TestEqual(TEXT("All fourteen new residential presentations checked"),Checked,14);
	World->DestroyWorld(false);
	return true;
}
#endif

#endif
