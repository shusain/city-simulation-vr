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
#include "Presentation/CityIndustrialEntrances.h"
#include "Serialization/JsonSerializer.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialGroundCookedTest, "CitySim.Presentation.IndustrialGroundCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityIndustrialGroundCookedTest::RunTest(const FString&)
{
    TSet<FString> Targets;
    for(const auto& Entry:CityIndustrialEntrances::All()) Targets.Add(CityResidentialEntrances::ObjectPath(Entry));
    TestEqual(TEXT("All industrial presentations"),Targets.Num(),14);
    for(const auto& Target:Targets)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Target);
        if(!TestNotNull(TEXT("Cooked industrial body resolves"),Mesh)) continue;
        TestTrue(TEXT("Cooked body has explicit materials"),!Mesh->GetStaticMaterials().IsEmpty());
        for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(TEXT("Cooked industrial material resolves"),Slot.MaterialInterface.Get());
        TestTrue(TEXT("Cooked body excludes the rigid ground walk"),Mesh->GetBoundingBox().Max.Y<800.1);
        const auto Bounds=Mesh->GetBoundingBox();
        TestTrue(TEXT("Complete industrial body fits the reserved2x2 plot"),
            FMath::Max(FMath::Abs(Bounds.Min.X),FMath::Abs(Bounds.Max.X))<=800.1 &&
            FMath::Max(FMath::Abs(Bounds.Min.Y),FMath::Abs(Bounds.Max.Y))<=800.1);
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialGroundTest, "CitySim.Terrain.IndustrialGroundVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityIndustrialGroundTest::RunTest(const FString&)
{
	FString Text;
	TSharedPtr<FJsonObject> Document;
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->SimulationSubsystem = World->GetSubsystem<UCitySimulationSubsystem>();
	int32 Checked = 0;
	for(const TCHAR* Manifest:{TEXT("city_industrial_ground.json")})
	{
	if(!TestTrue(TEXT("Selected industrial manifest loads"),FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("../ArtSource/Manifests")/Manifest))) ||
	   !TestTrue(TEXT("Selected manifest parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Document))) continue;
	for (const auto& Value : Document->GetArrayField(TEXT("assets")))
	{
		const auto Entry = Value->AsObject();
		const FString Target = Entry->GetStringField(TEXT("target"));
		const FString TargetObject = Target + TEXT(".") + FPaths::GetBaseFilename(Target);
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, *TargetObject);
		if (!TestNotNull(TEXT("Derived industrial mesh exists"), Mesh)) continue;
		const auto* Profile = CityIndustrialEntrances::Find(Mesh->GetName());
		if (!TestNotNull(TEXT("Derived industrial entrance is declared"), Profile)) continue;
		const auto& Portal = Entry->GetArrayField(TEXT("entry_native_cm"));
		TestTrue(TEXT("Runtime entry matches measured source"), Profile->EntryCm.Equals(FVector(Portal[0]->AsNumber(),Portal[1]->AsNumber(),Portal[2]->AsNumber()),.01));
		TestEqual(TEXT("Landing uses retained porch contract"), Profile->LandingCm, Entry->GetNumberField(TEXT("landing_cm")));
		TestEqual(TEXT("Short frontage reserve matches source contract"), Profile->GroundReserveCm, Entry->GetNumberField(TEXT("ground_reserve_cm")));
        TestFalse(TEXT("Personnel corridor uses a direct flight"),Profile->bFrontageFlight);
        TestEqual(TEXT("Personnel width matches source"),Profile->WidthCm,Entry->GetNumberField(TEXT("path_width_cm")));
        const auto Bays=CityIndustrialEntrances::Bays(Mesh->GetName());
        TestEqual(TEXT("Native bay count matches source entrances"),Bays.Num(),Entry->GetArrayField(TEXT("freight")).Num());
        for(const auto& Bay:Bays) {
            TestTrue(TEXT("Bay and personnel lanes are separate"),FMath::Abs(Bay.EntryCm.X-Profile->EntryCm.X)>(Bay.WidthCm+Profile->WidthCm)*.5);
            TestTrue(TEXT("Vehicle access remains below a raised dock"),!Bay.bRaisedDock || Bay.EntryCm.Z<20.);
        }

		const auto& LOD = Mesh->GetRenderData()->LODResources[0];
        for(int32 BayIndex=0;BayIndex<Bays.Num();++BayIndex) {
            const auto SourceBay=Entry->GetArrayField(TEXT("freight"))[BayIndex]->AsObject();
            const auto& Point=SourceBay->GetArrayField(TEXT("entry_native_cm"));
            TestTrue(TEXT("Freight portal matches measured source"),Bays[BayIndex].EntryCm.Equals(FVector(Point[0]->AsNumber(),Point[1]->AsNumber(),Point[2]->AsNumber()),.01));
            double Distance=MAX_dbl;
            for(int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3) {
                const FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
                const FVector B(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
                const FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
                Distance=FMath::Min(Distance,FVector::Dist(Bays[BayIndex].EntryCm,FMath::ClosestPointOnTriangleToPoint(Bays[BayIndex].EntryCm,A,B,C)));
            }
            TestTrue(TEXT("Truck paving starts on the actual sill or vertical dock face"),Distance<.1);
        }

		TestEqual(TEXT("Native triangles match checked FBX"), static_cast<int32>(LOD.GetNumTriangles()), static_cast<int32>(Entry->GetNumberField(TEXT("triangles"))));
		TestTrue(TEXT("Old ground walk no longer reaches the lot boundary"), Mesh->GetBoundingBox().Max.Y < 800.1);
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
		Definition.DefinitionId=TEXT("Factory"); Definition.ConstructionSteps=0;
		const FString Original = Entry->GetStringField(TEXT("native"));
		Definition.PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Original+TEXT(".")+FPaths::GetBaseFilename(Original)));
		const auto Resolved=Board->ResolvePresentationDefinition(0,Definition);
		TestEqual(TEXT("Real board resolves each original appearance to its derivative"),Resolved.PresentationMesh.Get(),Mesh);
        const FCityCellCoord Anchor(18,16);
        const auto Pose=Board->MakeBuildingTransform(Anchor,Resolved,0);
        TestEqual(TEXT("Ground-level industrial loading floors use5cm clearance"),Pose.GetLocation().Z-Board->GetBuildingBaseHeight(Anchor,Resolved,0),5.);
        const auto OriginalPose=Board->MakeBuildingTransform(Anchor,Definition,0);
        TestEqual(TEXT("Original source presentations retain their existing datum"),OriginalPose.GetLocation().Z-Board->GetBuildingBaseHeight(Anchor,Definition,0),40.);
		++Checked;
	}
	}
	TestEqual(TEXT("All fourteen industrial presentations checked"),Checked,14);
	World->DestroyWorld(false);
	return true;
}
#endif

#endif
