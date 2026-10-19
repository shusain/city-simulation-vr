#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityParkMotion.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkCitizenScaleTest, "CitySim.Parks.CitizenRelativeFieldScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkCitizenScaleTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Main catalog remains available"), Catalog))
		return false;
	auto* Citizen = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CitySim/Art/Parks/Meshes/SK_Park_Field_Casual"));
	if (!TestNotNull(TEXT("Main rigged citizen remains available"), Citizen))
		return false;
	const auto& Rest = Citizen->GetResourceForRendering()->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
	FBox Body(EForceInit::ForceInit);
	for (uint32 I = 0; I < Rest.GetNumVertices(); ++I)
		Body += FVector(Rest.VertexPosition(I));
	const double Height = Body.GetSize().Z * .811;
	TestTrue(TEXT("Citizen source scale is measured from native vertices"), Height > 140 && Height < 148);
	for (const TCHAR* Kind : {TEXT("Soccer"), TEXT("Baseball")})
	{
		const FString Id = FString(Kind) + TEXT("Park");
		const auto* Definition = Catalog->FindDefinition(*Id);
		if (!TestNotNull(TEXT("Main park definition remains present"), Definition))
			continue;
		TestTrue(TEXT("Scale refinement preserves logical seven-cell footprint"),
		         Definition->Footprint.Width == 7 && Definition->Footprint.Height == 7);
		auto* Mesh = Definition->PresentationMesh.LoadSynchronous();
		if (!TestNotNull(TEXT("Main catalog resolves the updated field asset"), Mesh))
			continue;
		TestEqual(TEXT("Catalog resolves the existing native field package"), Mesh->GetName(),
		          FString::Printf(TEXT("SM_Building_%sPark_A"), Kind));
		TestTrue(TEXT("Park bounds retain the full footprint"),
		         Mesh->GetBoundingBox().GetSize().Equals(FVector(5600, 5600, 580), .2));
		const auto& Positions = Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
		FBox Goal(EForceInit::ForceInit), Plate(EForceInit::ForceInit);
		double CrossbarUnderside = TNumericLimits<double>::Max();
		double GrassTop = -1;
		for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
		{
			const FVector P(Positions.VertexPosition(I));
			if (FMath::IsNearlyEqual(FMath::Abs(P.X), 2800.0, .02) &&
			    FMath::IsNearlyEqual(FMath::Abs(P.Y), 2800.0, .02))
				GrassTop = FMath::Max(GrassTop, P.Z);
			if (FString(Kind) == TEXT("Soccer") && FMath::Abs(P.X) > 2190 && FMath::Abs(P.X) < 2450 &&
			    FMath::Abs(P.Y) < 260 && P.Z > 30)
			{
				Goal += P;
				if (FMath::IsNearlyEqual(FMath::Abs(P.Y), 240.0, .02) && P.Z > 100 && FMath::Abs(P.X) < 2210)
					CrossbarUnderside = FMath::Min(CrossbarUnderside, P.Z);
			}
			if (FString(Kind) == TEXT("Baseball") && FMath::IsNearlyEqual(FMath::Abs(P.X), 21.0, .02) &&
			    FMath::IsNearlyEqual(FMath::Abs(P.Y), 21.0, .02))
				Plate += P;
		}
		TestTrue(TEXT("Native lawn meets the authored citizen ground plane"),
		         FMath::IsNearlyEqual(GrassTop, 20.0, .02));
		if (FString(Kind) == TEXT("Soccer"))
		{
			TestTrue(TEXT("Updated goals are measured from their actual native geometry"), Goal.IsValid != 0);
			TestTrue(TEXT("Goal top retains the intended citizen-relative height"),
			         FMath::IsNearlyEqual(Goal.Max.Z, 229.1667, .03));
			const double Clearance = CrossbarUnderside - GrassTop;
			TestTrue(TEXT("Goal opening is proportionate to the native citizen"),
			         Clearance / Height > 1.3 && Clearance / Height < 1.4);
		}
		else
		{
			TestTrue(TEXT("Base markers are finite, small and low to the field"),
			         Plate.IsValid != 0 && Plate.GetSize().Equals(FVector(42, 42, 4), .03));
			TestTrue(TEXT("Base width stays below one third of citizen height"), Plate.GetSize().X / Height < .33);
		}
	}
	for (const auto Pair :
	     {TPair<const TCHAR*, double>{TEXT("Baseball"), 8.0}, TPair<const TCHAR*, double>{TEXT("Soccer"), 22.0}})
	{
		auto* Ball = LoadObject<UStaticMesh>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Meshes/SM_ParkNative_%s"), Pair.Key));
		if (TestNotNull(TEXT("Main native ball binding exists"), Ball))
			TestTrue(TEXT("Ball diameter is measured in native centimeters"),
			         Ball->GetBoundingBox().GetSize().Equals(FVector(Pair.Value), .03));
	}
	for (const auto Motion : {ECityParkMotion::SoccerKick, ECityParkMotion::SoccerRun})
	{
		FCityParkMotionSample Pose;
		FCityParkMotion::Sample(Motion, 0, Pose);
		TestTrue(TEXT("Smaller soccer ball touches the same ground as its citizen"),
		         FMath::IsNearlyEqual(Pose.Props[0].GetLocation().Z - 11, Pose.Body.GetLocation().Z, .03));
	}
	return true;
}
#endif
