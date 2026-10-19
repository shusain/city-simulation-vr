#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Engine/World.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Sandbox/CitySandboxEnvironment.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRooftopMetalTest, "CitySim.Art.RooftopMetalMasks",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityRooftopMetalTest::RunTest(const FString& Parameters)
{
	UWorld* Sandbox = LoadObject<UWorld>(nullptr, TEXT("/Game/CitySim/Maps/CitySandbox.CitySandbox"));
	int32 Reflections = 0;
	if (TestNotNull(TEXT("Sandbox map is available"), Sandbox) && Sandbox->PersistentLevel)
		for (AActor* Actor : Sandbox->PersistentLevel->Actors)
			if (auto* Environment = Cast<ACitySandboxEnvironment>(Actor))
				if (auto* Capture = Environment->FindComponentByClass<USphereReflectionCaptureComponent>())
				{
					++Reflections;
					const auto* Data = Capture->GetMapBuildData();
					TestTrue(TEXT("Saved room reflection contains desktop and mobile precomputed data"),
					         Data && Data->CubemapSize > 0 && !Data->FullHDRCapturedData.IsEmpty() &&
					             !Data->EncodedHDRCapturedData.IsEmpty());
					TestTrue(TEXT("Saved reflection has usable linear HDR energy"),
					         Data && Data->AverageBrightness > .05f && Data->AverageBrightness < 1.0f);
				}
	TestEqual(TEXT("Sandbox has one room reflection"), Reflections, 1);
	UMaterial* Material =
	    LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/Art/Materials/M_CityMechanical.M_CityMechanical"));
	if (!TestNotNull(TEXT("Mechanical material is available"), Material))
		return false;
	const auto* Metallic = Material->GetExpressionInputForProperty(MP_Metallic);
	TestTrue(TEXT("Mechanical metallic input reads the authored red mask"),
	         Metallic && Cast<UMaterialExpressionVertexColor>(Metallic->Expression) && Metallic->MaskR &&
	             !Metallic->MaskG && !Metallic->MaskB);
	for (const TCHAR* Name : {TEXT("SM_Building_Residential_High_A"), TEXT("SM_Building_Factory_A"),
	                          TEXT("SM_Building_Factory_Medium_A"), TEXT("SM_Building_Factory_High_A")})
	{
		const FString Path = FString::Printf(TEXT("/Game/CitySim/Art/Meshes/%s.%s"), Name, Name);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!TestNotNull(Path, Mesh) || !Mesh->GetRenderData())
			continue;
		const auto& Lod = Mesh->GetRenderData()->LODResources[0];
		int32 Paint = 0, Metal = 0;
		for (int32 SectionIndex = 0; SectionIndex < Lod.Sections.Num(); ++SectionIndex)
		{
			const auto& Section = Lod.Sections[SectionIndex];
			int32 SectionPaint = 0;
			for (uint32 I = Section.FirstIndex; I < Section.FirstIndex + Section.NumTriangles * 3; ++I)
				SectionPaint += Lod.VertexBuffers.ColorVertexBuffer.VertexColor(Lod.IndexBuffer.GetIndex(I)).R == 0;
			AddInfo(FString::Printf(TEXT("Section %d Material=%s Override=%d Paint=%d"), SectionIndex,
			                        *Mesh->GetMaterial(Section.MaterialIndex)->GetName(),
			                        Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex, SectionPaint));
			if (!Mesh->GetMaterial(Section.MaterialIndex)->GetName().Contains(TEXT("Mechanical")))
				continue;
			for (uint32 I = Section.FirstIndex; I < Section.FirstIndex + Section.NumTriangles * 3; ++I)
			{
				const auto Color = Lod.VertexBuffers.ColorVertexBuffer.VertexColor(Lod.IndexBuffer.GetIndex(I));
				Paint += Color.R == 0;
				Metal += Color.R == 255;
				TestEqual(TEXT("Mechanical faces never use the night-window mask"), Color.A, uint8(255));
			}
		}
		AddInfo(FString::Printf(TEXT("%s Paint=%d Metal=%d"), Name, Paint, Metal));
		TestTrue(TEXT("Exposed-metal details retain their imported mask"), Metal > 0);
		if (!FString(Name).Equals(TEXT("SM_Building_Factory_A")))
			TestTrue(TEXT("Painted equipment retains its separate imported mask"), Paint > 0);
	}
	return true;
}
#endif
