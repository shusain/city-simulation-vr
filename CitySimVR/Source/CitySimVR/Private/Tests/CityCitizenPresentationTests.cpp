#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityCitizenPresentation.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenPresentationTest, "CitySim.Presentation.CitizenAssetsAndPlatform",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityCitizenPresentationTest::RunTest(const FString& Parameters)
{
	const auto* Baseline = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/CitySim/Art/Meshes/SM_Citizen_A.SM_Citizen_A"));
	if (!TestNotNull(TEXT("Quest retains its existing baseline citizen"), Baseline))
		return false;
	const auto* ParkMesh = LoadObject<UStaticMesh>(
	    nullptr, TEXT("/Game/CitySim/Art/Parks/Meshes/SM_Park_Field_Casual.SM_Park_Field_Casual"));
	if (!TestNotNull(TEXT("Park field mesh provides the accepted citizen scale reference"), ParkMesh))
		return false;
	const double ParkHeightCm = ParkMesh->GetBoundingBox().GetSize().Z * .811;
	auto CheckScale = [&](const UStaticMesh* Mesh)
	{
		const auto Bounds = Mesh->GetBoundingBox();
		const double HeightCm = Bounds.GetSize().Z * CityCitizenPresentation::StreetUniformScale;
		TestTrue(TEXT("Street citizen matches the neutral 1.43m park scale"),
		         FMath::IsNearlyEqual(HeightCm, 143.175, .02));
		TestTrue(TEXT("Street citizen is within4cm of the slightly posed park field mesh"),
		         FMath::Abs(HeightCm - ParkHeightCm) < 4.0);
		TestTrue(TEXT("Actual imported feet retain the12cm sidewalk support plane"),
		         FMath::IsNearlyEqual(CityCitizenPresentation::StreetOriginOffsetCm +
		                                  Bounds.Min.Z * CityCitizenPresentation::StreetUniformScale,
		                              12.0, .02));
	};
	CheckScale(Baseline);
	for (const TCHAR* Style : {TEXT("Casual"), TEXT("Office"), TEXT("Worker")})
	{
		const FString Path =
		    FString::Printf(TEXT("/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_%s.SM_Citizen_PC_%s"), Style, Style);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!TestNotNull(Path, Mesh))
			continue;
		TestEqual(TEXT("Refined citizen has two real authored LODs"), Mesh->GetNumLODs(), 2);
		TestEqual(TEXT("Refined LOD retains accepted geometry"), Mesh->GetNumTriangles(0), 754);
		TestEqual(TEXT("Distant LOD retains accepted simpler geometry"), Mesh->GetNumTriangles(1), 420);
		const auto Bounds = Mesh->GetBoundingBox();
		TestTrue(TEXT("Refinement preserves baseline imported feet and height"),
		         FMath::IsNearlyEqual(Bounds.Min.Z, Baseline->GetBoundingBox().Min.Z, .02) &&
		             FMath::IsNearlyEqual(Bounds.Max.Z, Baseline->GetBoundingBox().Max.Z, .02));
		CheckScale(Mesh);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const auto& Lod = Mesh->GetRenderData()->LODResources[Index];
			TestEqual(TEXT("Citizen LOD has one section"), Lod.Sections.Num(), 1);
			TSet<FColor> Colors;
			for (uint32 V = 0; V < Lod.VertexBuffers.ColorVertexBuffer.GetNumVertices(); ++V)
				Colors.Add(Lod.VertexBuffers.ColorVertexBuffer.VertexColor(V));
			TestTrue(TEXT("Clothing, skin, hair and shoes retain their imported palette"), Colors.Num() >= 5);
			TestFalse(TEXT("Citizen LOD does not collide with tools"),
			          Mesh->GetSectionInfoMap().Get(Index, 0).bEnableCollision);
		}
		TestTrue(TEXT("Distant geometry threshold is explicitly authored"),
		         FMath::IsNearlyEqual(Mesh->GetRenderData()->ScreenSize[1].Default, .025f));
		auto* Material = Mesh->GetMaterial(0)->GetMaterial();
		const auto* Color =
		    Cast<UMaterialExpressionPower>(Material->GetExpressionInputForProperty(MP_BaseColor)->Expression);
		TestTrue(TEXT("Lit clothing decodes imported RGB rather than using a flat tint"),
		         Color && Cast<UMaterialExpressionVertexColor>(Color->Base.Expression) &&
		             FMath::IsNearlyEqual(Color->ConstExponent, 2.2f));
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACityBoardActor* Board = World->SpawnActor<ACityBoardActor>();
	const bool Refined = PLATFORM_WINDOWS && !FParse::Param(FCommandLine::Get(), TEXT("CityLegacyCitizens"));
	TestEqual(TEXT("Runtime platform policy applies after serialized components initialize"), Board->bRefinedCitizens,
	          Refined);
	UInstancedStaticMeshComponent* Pools[] = {Board->CitizenInstances,           Board->OfficeCitizenInstances,
	                                          Board->WorkerCitizenInstances,     Board->ParkVisitorInstances,
	                                          Board->ParkOfficeVisitorInstances, Board->ParkWorkerVisitorInstances};
	for (auto* Pool : Pools)
	{
		const bool Park = Pool->GetName().StartsWith(TEXT("Park"));
		TestTrue(TEXT("Refined or baseline mesh follows the platform policy"),
		         Pool->GetStaticMesh()->GetPathName().Contains(Park      ? TEXT("/Art/Parks/")
		                                                       : Refined ? TEXT("/Art/PC/")
		                                                                 : TEXT("SM_Citizen_A")));
		TestFalse(TEXT("Citizen pools never create per-person ticks"), Pool->PrimaryComponentTick.bCanEverTick);
		TestEqual(TEXT("Citizen pools never intercept picking"), Pool->GetCollisionEnabled(),
		          ECollisionEnabled::NoCollision);
	}
	for (uint64 Identity = 1; Identity < 100; ++Identity)
	{
		const int32 Style = CityCitizenPresentation::VariantIndex(Identity, true);
		TestTrue(TEXT("Saved identity has a bounded deterministic style"),
		         Style >= 0 && Style < 3 && Style == CityCitizenPresentation::VariantIndex(Identity, true));
		TestEqual(TEXT("Quest and comparison mode always use their original pool"),
		          CityCitizenPresentation::VariantIndex(Identity, false), 0);
	}
	World->DestroyWorld(false);
	return true;
}
#endif
