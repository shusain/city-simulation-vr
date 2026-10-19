#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityRoadRibbon.h"
#include "Presentation/CityHomeWalk.h"
#include "Presentation/CityPrivateEntrances.h"
#include "ProceduralMeshComponent.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UObject/StrongObjectPtr.h"

// An opt-in visual investigation. Catalog edits and the candidate connector
// exist only in this automation process; no production asset is saved.
class FCityGroundPlotStudyCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityGroundPlotStudyCommand(FAutomationTestBase* InTest, bool Residential=false, bool Stages=false, bool Commercial=false, bool Industrial=false, bool Civic=false, bool Utility=false, bool UtilityBatch=false) : Test(InTest) {bResidentialStudy=Residential;bStagesStudy=Stages;bCommercialStudy=Commercial;bIndustrialStudy=Industrial;bCivicStudy=Civic || Utility || UtilityBatch;bUtilityStudy=Utility || UtilityBatch;bUtilityBatchStudy=UtilityBatch;}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		if (!World)
		{
			if (++Attempts < 120) { Next = FPlatformTime::Seconds() + .5; return false; }
			Test->AddError(TEXT("Ground plot study world missing")); return true;
		}
		if (!Board)
		{
			for (TActorIterator<ACityBoardActor> It(World); It; ++It) { Board = *It; break; }
			if (!Test->TestNotNull(TEXT("Study uses the real city board"), Board)) return true;
			for (TActorIterator<ACityVRToolController> It(World); It; ++It) It->ReturnToCity();
			for (TActorIterator<ACitySandboxEnvironment> It(World); It; ++It) It->SetLightingPreset(ECityLightingPreset::Day);
			Sim = World->GetSubsystem<UCitySimulationSubsystem>();
			OriginalCatalog.Reset(Board->BuildingCatalog.Get());
			bFoundationStudy = FParse::Param(FCommandLine::Get(), TEXT("CityFoundationStudy"));
			bHomePilot = bResidentialStudy || FParse::Param(FCommandLine::Get(), TEXT("CityHomeGroundPilot"));
			if (bHomePilot) { CaseIndex = bUtilityBatchStudy ? 152 : bUtilityStudy ? 141 : bCivicStudy ? 102 : bIndustrialStudy ? 76 : bCommercialStudy ? 54 : bStagesStudy ? 37 : bResidentialStudy ? 25 : 15; Mode = 2; }
			bOriginalBoardTick = Board->IsActorTickEnabled();
			Board->SetActorTickEnabled(false);
			FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), Directory);
			if (Directory.IsEmpty()) { Test->AddError(TEXT("Study needs an isolated report directory")); return true; }
			Directory /= bUtilityBatchStudy ? TEXT("UtilityBatchGround") : bUtilityStudy ? TEXT("UtilityGround") : bCivicStudy ? TEXT("CivicGround") : bIndustrialStudy ? TEXT("IndustrialGround") : bCommercialStudy ? TEXT("CommercialGround") : bStagesStudy ? TEXT("ResidentialStages") : bResidentialStudy ? TEXT("ResidentialGround") : bHomePilot ? TEXT("HomeGroundPilot") : bFoundationStudy ? TEXT("GroundPlotsFoundations") : TEXT("GroundPlotsGroundPlots"); IFileManager::Get().MakeDirectory(*Directory, true);
			Camera = World->SpawnActor<ASceneCapture2D>();
			auto* Capture = Camera->GetCaptureComponent2D();
			Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
			Capture->CaptureSource = SCS_FinalColorLDR; Capture->FOVAngle = 48;
			Capture->bOverride_CustomNearClippingPlane = true; Capture->CustomNearClippingPlane = .1f;
			Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
			Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
			Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
			auto* Target = NewObject<UTextureRenderTarget2D>(Camera);
			Target->InitCustomFormat(1200, 900, PF_B8G8R8A8, false); Capture->TextureTarget = Target;
			Metrics = TEXT("case,mode,tiles_x,tiles_y,rotation,lot_relief_cm,old_road_contact_error_cm,contact_error_cm,connector_lateral_offset_cm\n");
		}
		if (Stage == 0)
		{
			if (!Prepare()) return Finish(false);
			Stage = 1; Next = FPlatformTime::Seconds() + 2; return false;
		}
		if (Stage == 1 || Stage == 3)
		{
			if (Mode == 2 && !bFoundationStudy)
			{
				const auto* Join = Board->BuildingEntranceProfiles->GetProcMeshSection(0);
				for (int32 I = Join->ProcVertexBuffer.Num() - 2; I < Join->ProcVertexBuffer.Num(); ++I)
				{
					const auto& P = Join->ProcVertexBuffer[I].Position;
					Test->TestTrue(TEXT("Captured connector retains its studied road contact"), FMath::Abs(P.Z - Board->GetRoadProfileHeight(P.X / 800, P.Y / 800) - FCityRoadRibbonMesh::SurfaceLiftCm) <= .51);
				}
			}
			const bool bDetail = Stage == 3;
			auto* Capture = Camera->GetCaptureComponent2D();
			FVector Aim = bDetail ? (Door + Road) * .5 : Center + FVector(0, 0, bUtilityBatchStudy && Cases[CaseIndex].ExtraCells==9?600.:bStagesStudy?CameraSpanCm*.45:300);
			if (bDetail) Aim.Z = Board->GetRoadProfileHeight(Road.X / 800, Road.Y / 800) + 150;
			const FVector Outward = bStagesStudy ? FRotator(0,Cases[CaseIndex].Turns*90.,0).RotateVector(FVector(0,-1,0)) : (Road - Door).GetSafeNormal2D();
			FVector Along = FVector::CrossProduct(FVector::UpVector, Outward);
            if(bStagesStudy && FVector::DotProduct(Road-Door,Along)<0) Along=-Along;
			const double Distance = bDetail ? (bCivicStudy?2400.:1800.) : CameraSpanCm * 1.45;
			const FVector Offset = Outward * Distance + Along * Distance * .65 + FVector(0, 0, Distance * (bDetail ? .55 : .95));
			const auto Pose = Board->GetActorTransform();
			const FVector WorldAim = Pose.TransformPosition(Aim);
			const FVector Position = Pose.TransformPosition(Aim + Offset);
			Camera->SetActorLocation(Position); Camera->SetActorRotation((WorldAim - Position).Rotation());
			Capture->CaptureScene(); Stage++; Next = FPlatformTime::Seconds() + .6; return false;
		}
		TArray<FColor> Pixels;
		auto* Target = Camera->GetCaptureComponent2D()->TextureTarget.Get();
		if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
		{ Test->AddError(TEXT("Study render readback failed")); return Finish(false); }
		int32 Visible = 0; for (const auto& P : Pixels) if (P.R > 25 || P.G > 25 || P.B > 25) ++Visible;
		Test->TestTrue(TEXT("Study capture contains rendered geometry"), Visible > 10000);
		const FString Label = FString(Cases[CaseIndex].Name) + TEXT("-") + ModeName() + (Stage == 2 ? TEXT("-Overview") : TEXT("-Contact"));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1200, 900, Pixels, Png);
		Test->TestTrue(TEXT("Ground plot screenshot saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / (Label + TEXT(".png")))));
		++Captures;
		if (Stage == 2) { Stage = 3; return false; }
		Stage = 0;
		if (bHomePilot) Mode = 2;
		if (++Mode == 1 && (bFoundationStudy || Cases[CaseIndex].ExtraCells == 0)) ++Mode;
		if (Mode > 2) { Mode = 0; ++CaseIndex; }
		if (bHomePilot) Mode = 2;
		if (CaseIndex == (bUtilityBatchStudy ? UE_ARRAY_COUNT(Cases) : bUtilityStudy ? 152 : bCivicStudy ? 141 : bIndustrialStudy ? 102 : bCommercialStudy ? 76 : bStagesStudy ? 54 : bResidentialStudy ? 37 : bHomePilot || bFoundationStudy ? 25 : 8)) return Finish(true);
		return false;
	}

  private:
	struct FCase { const TCHAR* Name; const TCHAR* Id; int32 SlopeX; int32 SlopeY; int32 Turns; int32 ExtraCells; const TCHAR* Mesh = nullptr; int32 PartialFrontage = 0; const TCHAR* RoadId = TEXT("LocalRoad"); };
	const FCase Cases[177] = {
		{TEXT("Home-Flat"), TEXT("Home"), 0, 0, 0, 0},
		{TEXT("Home-Uphill"), TEXT("Home"), 0, 80, 0, 0},
		{TEXT("Home-Downhill"), TEXT("Home"), 0, -80, 0, 0},
		{TEXT("Home-CrossSlope"), TEXT("Home"), 80, 0, 0, 0},
		{TEXT("Home-RotatedCrossSlope"), TEXT("Home"), 0, 80, 1, 0},
		{TEXT("Computing-Slope"), TEXT("ComputingCenter"), 0, 60, 0, 1},
		{TEXT("Research-Slope"), TEXT("ResearchCampus"), 0, 50, 0, 1},
		{TEXT("Hospital-CrossSlope"), TEXT("Hospital"), 60, 0, 0, 0},
		{TEXT("DigitalExchange-Slope"), TEXT("DigitalExchange"), 0, 60, 0, 0},
		{TEXT("AIInstitute-Slope"), TEXT("AIInstitute"), 60, 0, 1, 0},
		{TEXT("EnterpriseAI-Slope"), TEXT("EnterpriseAICenter"), 0, -60, 0, 0},
		{TEXT("School-Slope"), TEXT("School"), 60, 0, 0, 0},
		{TEXT("Solar-Slope"), TEXT("SolarPower"), 0, 50, 0, 0},
		{TEXT("WaterTower-Slope"), TEXT("WaterTower"), 0, 80, 0, 0},
		{TEXT("Shop-LegacySlope"), TEXT("Shop"), 0, 80, 0, 0},
		{TEXT("Pilot-Flat"), TEXT("Home"), 0, 0, 0, 0},
		{TEXT("Pilot-Uphill"), TEXT("Home"), 0, 80, 0, 0},
		{TEXT("Pilot-Downhill"), TEXT("Home"), 0, -80, 0, 0},
		{TEXT("Pilot-Crest"), TEXT("Home"), 0, 0, 0, 1},
		{TEXT("Pilot-Hollow"), TEXT("Home"), 0, 0, 0, 2},
		{TEXT("Pilot-WavesNorth"), TEXT("Home"), 25, 20, 0, 3},
		{TEXT("Pilot-WavesEast"), TEXT("Home"), 25, 20, 1, 3},
		{TEXT("Pilot-WavesSouth"), TEXT("Home"), 25, 20, 2, 3},
		{TEXT("Pilot-WavesWest"), TEXT("Home"), 25, 20, 3, 3},
		{TEXT("Pilot-RollingSeed"), TEXT("Home"), 0, 0, 0, 9},
        {TEXT("Residential-B-Flat"), TEXT("Home"), 0, 0, 0, 0, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_B")},
        {TEXT("Residential-C-Flat"), TEXT("Home"), 0, 0, 1, 0, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_C")},
        {TEXT("Residential-D-Flat"), TEXT("Home"), 0, 0, 2, 0, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_D")},
        {TEXT("Residential-A-Damaged"), TEXT("Home"), 25, 20, 0, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_A_Damaged")},
        {TEXT("Residential-B-Hills"), TEXT("Home"), 25, 20, 0, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_B")},
        {TEXT("Residential-B-Damaged"), TEXT("Home"), 25, 20, 0, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_B_Damaged")},
        {TEXT("Residential-C-Hills"), TEXT("Home"), 25, 20, 1, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_C")},
        {TEXT("Residential-C-Damaged"), TEXT("Home"), 25, 20, 1, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_C_Damaged")},
        {TEXT("Residential-D-Hills"), TEXT("Home"), 25, 20, 2, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_D")},
        {TEXT("Residential-D-Damaged"), TEXT("Home"), 25, 20, 2, 3, TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_D_Damaged")},
        {TEXT("Residential-Legacy-Damaged"), TEXT("Home"), 25, 20, 3, 3, TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_A_Damaged")},
        {TEXT("Residential-Playable"), TEXT("Home"), 0, 0, 0, 9},
        {TEXT("Stages-Medium-Waves0"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Stages-Medium-Waves1"),TEXT("Home"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Stages-Medium-Waves2"),TEXT("Home"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Stages-Medium-Waves3"),TEXT("Home"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Stages-Medium-Flat"),TEXT("Home"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Stages-Medium-Damaged"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A_Damaged")},
        {TEXT("Stages-High-Waves0"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A")},
        {TEXT("Stages-High-Waves1"),TEXT("Home"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A")},
        {TEXT("Stages-High-Waves2"),TEXT("Home"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A")},
        {TEXT("Stages-High-Waves3"),TEXT("Home"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A")},
        {TEXT("Stages-High-Flat"),TEXT("Home"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A")},
        {TEXT("Stages-High-Damaged"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A_Damaged")},
        {TEXT("Stages-Construction"),TEXT("Home"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A")},
        {TEXT("Stages-Construction_Damaged"),TEXT("Home"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A_Damaged")},
        {TEXT("Stages-Partial1"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A"),1},
        {TEXT("Stages-Partial2"),TEXT("Home"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A"),2},
        {TEXT("Stages-Playable"),TEXT("Home"),0,0,0,9,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A")},
        {TEXT("Commercial-A-Flat"),TEXT("Shop"),0,0,0,0,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_A"),0},
        {TEXT("Commercial-A-Waves"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_A"),0},
        {TEXT("Commercial-B-Waves"),TEXT("Shop"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_B"),0},
        {TEXT("Commercial-C-Waves"),TEXT("Shop"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_C"),0},
        {TEXT("Commercial-D-Waves0"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),0},
        {TEXT("Commercial-D-Waves1"),TEXT("Shop"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),0},
        {TEXT("Commercial-D-Waves2"),TEXT("Shop"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),0},
        {TEXT("Commercial-D-Waves3"),TEXT("Shop"),25,20,3,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),0},
        {TEXT("Commercial-A-Damaged"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_A_Damaged"),0},
        {TEXT("Commercial-B-Damaged"),TEXT("Shop"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_B_Damaged"),0},
        {TEXT("Commercial-C-Damaged"),TEXT("Shop"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_C_Damaged"),0},
        {TEXT("Commercial-D-Damaged"),TEXT("Shop"),25,20,3,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D_Damaged"),0},
        {TEXT("Commercial-Medium"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialMedium_StoreCafe"),0},
        {TEXT("Commercial-High"),TEXT("Shop"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialHigh_StoreTech"),0},
        {TEXT("Commercial-Medium-Damaged"),TEXT("Shop"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialMedium_StoreCafe_Damaged"),0},
        {TEXT("Commercial-High-Damaged"),TEXT("Shop"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialHigh_StoreTech_Damaged"),0},
        {TEXT("Commercial-D-Partial1"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),1},
        {TEXT("Commercial-Medium-Partial2"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialMedium_StoreBooks"),2},
        {TEXT("Commercial-Construction"),TEXT("Shop"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0},
        {TEXT("Commercial-D-Avenue"),TEXT("Shop"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_D"),0,TEXT("Avenue")},
        {TEXT("Commercial-Medium-Avenue"),TEXT("Shop"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_CommercialMedium_StoreCafe"),0,TEXT("Avenue")},
        {TEXT("Commercial-Playable"),TEXT("Shop"),0,0,0,9,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_A"),0},
        {TEXT("Industrial-A-Flat"),TEXT("Factory"),0,0,0,0,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-A-Waves"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-B-Waves"),TEXT("Factory"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_B"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Dock0"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Dock1"),TEXT("Factory"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Dock2"),TEXT("Factory"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Dock3"),TEXT("Factory"),25,20,3,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-D-TwinBays"),TEXT("Factory"),25,20,3,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_D"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-A-Damaged"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-B-Damaged"),TEXT("Factory"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_B_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Damaged"),TEXT("Factory"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-D-Damaged"),TEXT("Factory"),25,20,3,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_D_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-Medium"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-Medium-Damaged"),TEXT("Factory"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_Medium_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-High"),TEXT("Factory"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_High_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-High-Damaged"),TEXT("Factory"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_High_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-A-Partial1"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),1,TEXT("LocalRoad")},
        {TEXT("Industrial-C-Partial2"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),2,TEXT("LocalRoad")},
        {TEXT("Industrial-D-Partial1"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_D"),1,TEXT("LocalRoad")},
        {TEXT("Industrial-A-Avenue"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),0,TEXT("Avenue")},
        {TEXT("Industrial-C-Avenue"),TEXT("Factory"),25,20,1,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_C"),0,TEXT("Avenue")},
        {TEXT("Industrial-D-Avenue"),TEXT("Factory"),25,20,2,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_D"),0,TEXT("Avenue")},
        {TEXT("Industrial-Construction"),TEXT("Factory"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-Legacy"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_A"),0,TEXT("LocalRoad")},
        {TEXT("Industrial-A-Partial2"),TEXT("Factory"),25,20,0,3,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),2,TEXT("LocalRoad")},
        {TEXT("Industrial-Playable"),TEXT("Factory"),0,0,0,9,TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Factory_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_A"),TEXT("Police"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_A"),TEXT("Fire"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_A_Damaged"),TEXT("Police"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_A_Damaged"),TEXT("Fire"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-School_A"),TEXT("School"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_School_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-School_A_Damaged"),TEXT("School"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_School_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_Medium_A"),TEXT("Police"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_High_A"),TEXT("Police"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_High_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_Medium_A"),TEXT("Fire"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_High_A"),TEXT("Fire"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_High_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_Medium_A_Damaged"),TEXT("Police"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_Medium_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_High_A_Damaged"),TEXT("Police"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_High_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_Medium_A_Damaged"),TEXT("Fire"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_Medium_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_High_A_Damaged"),TEXT("Fire"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_High_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-COLLEGE_A"),TEXT("College"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_BUILDING_COLLEGE_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-College_A_Damaged"),TEXT("College"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_College_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-PrimarySchool_A"),TEXT("PrimarySchool"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_PrimarySchool_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-PrimarySchool_A_Damaged"),TEXT("PrimarySchool"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_PrimarySchool_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-SecondarySchool_A"),TEXT("SecondarySchool"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SecondarySchool_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-SecondarySchool_A_Damaged"),TEXT("SecondarySchool"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SecondarySchool_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Civic-HOSPITAL_A"),TEXT("Hospital"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_BUILDING_HOSPITAL_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital_Medium_A"),TEXT("Hospital"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital_Damaged_A"),TEXT("Hospital"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Damaged_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital_Medium_Damaged_A"),TEXT("Hospital"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_Damaged_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Police_A-Flat"),TEXT("Police"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Fire_High_A-Flat"),TEXT("Fire"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_High_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-PrimarySchool_A-Flat"),TEXT("PrimarySchool"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_PrimarySchool_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-COLLEGE_A-Flat"),TEXT("College"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_BUILDING_COLLEGE_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital_Medium_A-Flat"),TEXT("Hospital"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital-Rotation1"),TEXT("Hospital"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),0,TEXT("Avenue")},
        {TEXT("Civic-Hospital-Rotation2"),TEXT("Hospital"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),0,TEXT("Avenue")},
        {TEXT("Civic-Hospital-Rotation3"),TEXT("Hospital"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),0,TEXT("Avenue")},
        {TEXT("Civic-School-Partial"),TEXT("School"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_School_A"),1,TEXT("LocalRoad")},
        {TEXT("Civic-Primary-Partial"),TEXT("PrimarySchool"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_PrimarySchool_A"),1,TEXT("LocalRoad")},
        {TEXT("Civic-College-Partial"),TEXT("College"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_BUILDING_COLLEGE_A"),3,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital-Partial"),TEXT("Hospital"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A"),4,TEXT("LocalRoad")},
        {TEXT("Civic-PrimarySchool-Construction"),TEXT("PrimarySchool"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Hospital-Construction"),TEXT("Hospital"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("Civic-Civic-PlayableHills"),TEXT("Police"),0,0,0,9,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Flat"),TEXT("NuclearPower"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Hills"),TEXT("NuclearPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Rotation1"),TEXT("NuclearPower"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Rotation2"),TEXT("NuclearPower"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Rotation3"),TEXT("NuclearPower"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Damaged"),TEXT("NuclearPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-FrontageRestored"),TEXT("NuclearPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),1,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-Construction"),TEXT("NuclearPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-LOD1"),TEXT("NuclearPower"),0,0,0,5,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-LOD2"),TEXT("NuclearPower"),25,20,0,6,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("Utility-Nuclear-PlayableHills"),TEXT("NuclearPower"),0,0,0,9,TEXT("/Game/CitySim/Art/Meshes/SM_Building_NuclearPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-coal-Hills"),TEXT("CoalPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_CoalPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-oil-Hills"),TEXT("OilPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_OilPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-power_base-Hills"),TEXT("Power"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Power_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-water_base-Hills"),TEXT("Service"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-power_medium-Hills"),TEXT("Power"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Power_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-power_high-Hills"),TEXT("Power"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Power_High_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-water_medium-Hills"),TEXT("Service"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_Medium_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-water_high-Hills"),TEXT("Service"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_High_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-water_tower-Hills"),TEXT("WaterTower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_WaterTower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-wind_turbine-Hills"),TEXT("WindPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/Wind/SM_Wind_Stationary_v1"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-solar_farm-Hills"),TEXT("SolarPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SolarPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-PowerHigh-Damaged"),TEXT("Power"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Power_High_A_Damaged"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Power-Construction"),TEXT("Power"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Water-Construction"),TEXT("Service"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Water-Rotation1"),TEXT("Service"),25,20,1,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Water-Rotation2"),TEXT("Service"),25,20,2,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Water-Rotation3"),TEXT("Service"),25,20,3,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-solar_farm-Flat"),TEXT("SolarPower"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SolarPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-water_tower-Flat"),TEXT("WaterTower"),0,0,0,0,TEXT("/Game/CitySim/Art/Meshes/SM_Building_WaterTower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-wind_turbine-Flat"),TEXT("WindPower"),0,0,0,0,TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/Wind/SM_Wind_Stationary_v1"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Coal-FrontageRestored"),TEXT("CoalPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_CoalPower_A"),4,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Solar-Partial"),TEXT("SolarPower"),25,20,0,3,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SolarPower_A"),2,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Solar-LOD1"),TEXT("SolarPower"),0,0,0,5,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SolarPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-Solar-LOD2"),TEXT("SolarPower"),25,20,0,6,TEXT("/Game/CitySim/Art/Meshes/SM_Building_SolarPower_A"),0,TEXT("LocalRoad")},
        {TEXT("UtilityBatch-PlayableHills"),TEXT("CoalPower"),0,0,0,9,TEXT("/Game/CitySim/Art/Meshes/SM_Building_CoalPower_A"),0,TEXT("LocalRoad")},
    };
	const TCHAR* Modes[3] = {TEXT("Current"), TEXT("PlotOnly"), TEXT("SurfaceJoinStudy")};
	const TCHAR* ModeName() const { return bUtilityBatchStudy ? TEXT("UtilityBatchGround") : bUtilityStudy ? TEXT("UtilityGround") : bCivicStudy ? TEXT("CivicGround") : bIndustrialStudy ? TEXT("IndustrialGround") : bCommercialStudy ? TEXT("CommercialGround") : bStagesStudy ? TEXT("ResidentialStages") : bResidentialStudy ? TEXT("ResidentialGround") : bHomePilot ? TEXT("HomeGroundPilot") : bFoundationStudy ? (Mode == 0 ? TEXT("WholePlotReference") : TEXT("BuildingSupportOnly")) : Modes[Mode]; }
	bool Prepare()
	{
		const auto& C = Cases[CaseIndex];
		auto Parameters = Sim->Terrain.GetParameters();
		Parameters.GeneratorVersion = Parameters.WaterRulesVersion = Parameters.SceneryVersion = 0;
		Parameters.BroadElevationAmplitudeCm = Parameters.HillAmplitudeCm = Parameters.RidgeAmplitudeCm = 0;
		if (bHomePilot && C.ExtraCells == 9) {
			Parameters.BroadElevationAmplitudeCm = bCivicStudy?450:900; Parameters.HillAmplitudeCm = bCivicStudy?700:1400; Parameters.RidgeAmplitudeCm = bCivicStudy?350:700;
			Parameters.SmoothingPasses = 1; Parameters.SeaLevelCm = -2400; Parameters.StarterDepthCells = Parameters.StarterHalfWidthCells = 2;
            if(bUtilityBatchStudy) {Parameters.BroadElevationAmplitudeCm=300;Parameters.HillAmplitudeCm=450;Parameters.RidgeAmplitudeCm=225;Parameters.SmoothingPasses=2;}
		}
		Sim->Terrain.Generate(Parameters); Sim->ResetCity(false);
        if (bResidentialStudy)
        {
            // Each synthetic catalog deliberately reuses Home with a different mesh.
            // Production appearance keys are distinct; discard fixture-only cached
            // pools so a later capture cannot keep the first case's building body.
            for (auto& Pair : Board->BuildingInstances)
                if (Pair.Value) Pair.Value->DestroyComponent();
            Board->BuildingInstances.Reset();
            Board->InstanceIndex.Reset();
        }
		// Controlled plane in a fresh synthetic city. Original cities and terrain are never read.
		auto& Heights = const_cast<TArray<int32>&>(Sim->Terrain.GetQuantizedHeightsCm());
		for (int32 Y = 0; Y < Sim->Terrain.GetSampleHeight(); ++Y)
			for (int32 X = 0; X < Sim->Terrain.GetSampleWidth(); ++X)
				if (!(bHomePilot && C.ExtraCells == 9)) {
					const double U = X * .5 - 19, V = Y * .5 - 16;
					double Wave = 0;
					if (bHomePilot && C.ExtraCells == 1) Wave = 100 * FMath::Cos(V * 2.2);
					if (bHomePilot && C.ExtraCells == 2) Wave = -100 * FMath::Cos(V * 2.2);
					if (bHomePilot && C.ExtraCells == 3) Wave = 65 * FMath::Sin(U * 2.1) + 60 * FMath::Cos(V * 2.3);
					Heights[Y * Sim->Terrain.GetSampleWidth() + X] = 200 + FMath::Clamp(FMath::RoundToInt((X * .5 - 18) * C.SlopeX + (Y * .5 - 16) * C.SlopeY + Wave), -150, 150);
				}
		auto* Catalog = NewObject<UCityBuildingCatalog>(Sim);
		Catalog->Definitions = OriginalCatalog->Definitions;
		auto* Definition = Catalog->Definitions.FindByPredicate([&](const auto& D) { return D.DefinitionId == FName(C.Id); });
		if (!Test->TestNotNull(TEXT("Study definition exists"), Definition)) return false;
		CameraSpanCm = FMath::Max(Definition->Footprint.Width + (bFoundationStudy ? 0 : C.ExtraCells), Definition->Footprint.Height + (bFoundationStudy ? 0 : C.ExtraCells)) * 800.0;
        // Wind presentation requires its operational development record. Keep
        // its normal construction metadata so InitializeExistingAsOperational
        // creates that record instead of omitting it as an instant decoration.
        if(Definition->DefinitionId!=TEXT("WindPower")) Definition->ConstructionSteps=0;
        Definition->ConstructionMesh.Reset();Definition->DensityLevels.Reset();
        if (C.Mesh) Definition->PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString(C.Mesh)+TEXT(".")+FPaths::GetBaseFilename(C.Mesh)));
        if (bResidentialStudy && C.Mesh)
        {
            // An operational Home without an appearance uses the seasonal A
            // fallback. Select the explicit study body through the real variant
            // path instead of relying on the definition's default mesh override.
            FCityLowDensityPresentationVariant Variant;
            Variant.DefinitionId = FName(C.Id);
            Variant.AppearanceId = FName(C.Name);
            Variant.PresentationMesh = Definition->PresentationMesh;
            Variant.DamagedMesh = Definition->PresentationMesh;
            Catalog->LowDensityPresentationVariants.Add(Variant);
        }
		Definition->bPlayerPurchasedLevels = false; Definition->RequiredOperatingFacilities.Reset(); Definition->bRequiresAlternativePower = false;
		if (Mode > 0 && !bFoundationStudy) { Definition->Footprint.Width += C.ExtraCells; Definition->Footprint.Height += C.ExtraCells; }
		Sim->RegisterBuildingCatalog(Catalog); Board->BuildingCatalog = Catalog;
		auto& City = Sim->GetPlacementModel(); City.ApplyMonthlyEconomy(100000, 0);
		ParcelSize = Definition->Footprint.GetRotatedSize(C.Turns);
		FCityCellCoord Anchor(18, 16);
		const bool bHorizontal = C.Turns % 2 == 0;
		const int32 RoadX = C.Turns == 1 ? 18 + ParcelSize.X : 17;
		const int32 RoadY = C.Turns == 0 ? 15 : 16 + ParcelSize.Y;
		const FCityCellCoord First = bHorizontal ? FCityCellCoord(14, RoadY) : FCityCellCoord(RoadX, 12);
		const FCityCellCoord Last = bHorizontal ? FCityCellCoord(bCivicStudy && C.ExtraCells==9?57:(bCommercialStudy || bIndustrialStudy) && C.ExtraCells==9?40:bStagesStudy && C.ExtraCells==9?30:26, RoadY) : FCityCellCoord(RoadX, 24);
		auto AddRoad = [&](FCityCellCoord A, FCityCellCoord B) {
			auto Preview = City.BeginRoadPreview(A, FName(C.RoadId)); City.UpdateRoadPreviewEnd(Preview, B);
            const auto Result=City.CommitRoad(Preview);
            return Test->TestTrue(*FString::Printf(TEXT("Study road (%d,%d)-(%d,%d), failure%d"),A.X,A.Y,B.X,B.Y,static_cast<int32>(Result.Failure)),Result.IsSuccess()); };
		if (!AddRoad(City.GetEntranceCell(), {12, 8}) || !AddRoad({12, 8}, {12, 12}) || !AddRoad({12, 12}, First) || !AddRoad(First, Last)) return false;
        if(C.PartialFrontage)
        {
            // Keep both surviving halves connected to the entrance via a
            // parallel street, so this isolates frontage rather than access.
            if(!AddRoad({16,15},{16,14}) || !AddRoad({16,14},{22,14}) || !AddRoad({22,14},{22,15})) return false;
            if(bCivicStudy) {
                for(int32 X=0;X<Definition->Footprint.Width;++X)
                    if(X!=C.PartialFrontage-1) Test->TestTrue(TEXT("Civic test keeps only the selected frontage tile"),City.RemoveRoad({18+X,15}));
            } else Test->TestTrue(TEXT("Partial frontage removes one road tile"),City.RemoveRoad({C.PartialFrontage==1?18:19,15}));
        }
        if(bUtilityStudy && C.ExtraCells==9) {
            bool Valid=false;
            for(int32 X=18;X<=52;++X) {
                auto Candidate=City.BeginPreview(*Definition,{X,16},C.Turns);
                if(City.ValidatePlacement(Candidate,*Definition).IsSuccess()) {Anchor={X,16};Valid=true;break;}
            }
            if(!Test->TestTrue(TEXT("Natural utility pilot keeps actual320cm placement relief limit"),Valid)) return false;
        }
        if(C.PartialFrontage && FCityPlacementModel::RequiresFullRoadFrontage(Definition->DefinitionId)) {
            Test->TestEqual(TEXT("Wide utility entrance rejects incomplete frontage"),City.ValidatePlacement(City.BeginPreview(*Definition,Anchor,C.Turns),*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
            if(!AddRoad(First,Last)) return false;
        }
		auto Preview = City.BeginPreview(*Definition, Anchor, C.Turns);
		auto Placed = City.CommitPlacement(Preview, *Definition);
		if (!Test->TestTrue(TEXT("Study parcel passes actual placement constraints"), Placed.IsSuccess())) return false;
		if (bHomePilot && C.ExtraCells == 9 && !bStagesStudy) {
			// Appearance is selected from entity identity when a normal catalog
			// reloads this save. Use real placement/demolition commands until the
			// retained home selects A, rather than saving a transient mesh override.
			FName Appearance;
			for (int32 Attempt = 0; Attempt < 32; ++Attempt) {
				const auto NativeMesh = OriginalCatalog->FindLowDensityPresentation(Placed.EntityId,TEXT("Home"),ECityDensityLevel::Low,false,Appearance);
				if (NativeMesh.GetAssetName() == TEXT("SM_LowDensity_Home_A")) break;
				if (!Test->TestTrue(TEXT("Synthetic appearance retry demolishes"),City.DemolishEntity(Placed.EntityId).IsSuccess())) return false;
				Preview = City.BeginPreview(*Definition,Anchor,C.Turns); Placed = City.CommitPlacement(Preview,*Definition);
				if (!Test->TestTrue(TEXT("Synthetic appearance retry places"),Placed.IsSuccess())) return false;
			}
			Test->TestEqual(TEXT("Playable hilly home selects the pilot under the normal catalog"),Appearance,FName(TEXT("A_Brick")));
		}
		Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
		if (bFoundationStudy)
		{
			auto* Pool = Board->ResidentialFoundationInstances.Get();
			auto* Mesh = LoadObject<UStaticMesh>(nullptr, Mode == 0
			    ? TEXT("/Game/CitySim/Art/Meshes/SM_Building_LotFoundation_A.SM_Building_LotFoundation_A")
			    : TEXT("/Engine/BasicShapes/Cube.Cube"));
			Pool->SetStaticMesh(Mesh);
			Pool->SetMaterial(0, Mode == 0 ? Mesh->GetMaterial(0) : Board->FoundationInstances->GetMaterial(0));
		}
        // Direct fixture placements/development edits must invalidate both caches.
        if(bUtilityBatchStudy) ++Sim->PresentationGeneration;
		Board->PrepareCityView();
        if(bUtilityBatchStudy) {
            Board->UpdateWindRotors(0);
            if(Definition->DefinitionId==TEXT("WindPower")) {
                Test->TestNotNull(TEXT("Captured wind has a real operational development record"),Sim->GetDevelopmentSnapshot().Find(Placed.EntityId));
                Test->TestEqual(TEXT("Captured turbine has its yaw nacelle"),Board->WindNacelleInstances->GetInstanceCount(),1);
                Test->TestEqual(TEXT("Captured turbine has its spinning rotor"),Board->WindRotorInstances->GetInstanceCount(),1);
            }
        }
		if (bResidentialStudy && C.Mesh)
		{
			const auto Presentation = Board->ResolvePresentationDefinition(Placed.EntityId, *Definition);
			const auto* Entrance = CityPrivateEntrances::Find(FPaths::GetBaseFilename(C.Mesh));
			if (!Test->TestNotNull(TEXT("Capture has an explicit residential target"), Entrance)) return false;
			auto* Expected = LoadObject<UStaticMesh>(nullptr, *CityResidentialEntrances::ObjectPath(*Entrance));
			auto* Pool = Board->GetOrCreateInstances(Presentation);
			if (!Test->TestTrue(TEXT("Captured pool actually draws this healthy/damaged residential body"),
				Expected && Pool->GetStaticMesh() == Expected && Pool->GetInstanceCount() == 1)) return false;
            if(bUtilityStudy && (C.ExtraCells==5 || C.ExtraCells==6)) Pool->SetForcedLodModel(C.ExtraCells==5?2:3);
            if(bStagesStudy) CameraSpanCm=FMath::Max(bCivicStudy?FMath::Max(Definition->Footprint.Width,Definition->Footprint.Height)*800.:1600.,Expected->GetBoundingBox().GetSize().Z*.9);
		}
		if (bFoundationStudy && Mode == 0)
		{
			// Test-only reconstruction of the previous full-lot slab. Production
			// uses localized supports in both modes until this reference override.
			Board->FoundationInstances->ClearInstances(); Board->ResidentialFoundationInstances->ClearInstances();
			auto* Pool = Definition->DefinitionId == TEXT("Home") ? Board->ResidentialFoundationInstances.Get() : Board->FoundationInstances.Get();
			double Low = MAX_dbl;
			for (int32 Y = Anchor.Y * 2; Y <= (Anchor.Y + ParcelSize.Y) * 2; ++Y)
				for (int32 X = Anchor.X * 2; X <= (Anchor.X + ParcelSize.X) * 2; ++X)
					Low = FMath::Min(Low, static_cast<double>(Sim->Terrain.GetSampleHeightCm(X, Y)));
			const double Top = Board->GetBuildingBaseHeight(Anchor, *Definition, C.Turns) + 20;
			const double Bottom = FMath::Min(Top - 40, Low - 20);
			Pool->AddInstance(FTransform(FRotator::ZeroRotator,
			    FVector((Anchor.X + ParcelSize.X * .5) * 800, (Anchor.Y + ParcelSize.Y * .5) * 800, (Top + Bottom) * .5),
			    FVector(ParcelSize.X * 8, ParcelSize.Y * 8, (Top - Bottom) / 100)));
		}
		if (!Test->TestTrue(TEXT("Actual frontage connection exists"), Board->FindEntranceConnection(Anchor, *Definition, C.Turns, Door, Road))) return false;
		if (bHomePilot) {
			const auto& Entity = City.GetEntities().CreateConstIterator().Value();
			const auto Presentation = Board->ResolvePresentationDefinition(Entity.EntityId, *Definition);
			if (!Test->TestTrue(TEXT("Pilot targets actual door and sidewalk"), Board->FindHomeWalkConnection(Entity, Presentation, Door, Road))) return false;
            if(C.PartialFrontage && !FCityPlacementModel::RequiresFullRoadFrontage(Definition->DefinitionId)) Test->TestTrue(TEXT("Flight chooses the surviving frontage half"),bCivicStudy?FMath::FloorToInt(Road.X/800.)==18+C.PartialFrontage-1:bIndustrialStudy?FMath::FloorToInt(Road.X/800.)==(C.PartialFrontage==1?19:18):C.PartialFrontage==1?Road.X>Door.X:Road.X<Door.X);
		}
		Center = FVector((Anchor.X + ParcelSize.X * .5) * 800, (Anchor.Y + ParcelSize.Y * .5) * 800, Board->GetBuildingBaseHeight(Anchor, *Definition, C.Turns));
		if (!bHomePilot) Door.Z = Center.Z + (Definition->DefinitionId == TEXT("Home") ? 36 : 42);
		const auto* Section = Board->BuildingEntranceProfiles->GetProcMeshSection(bHomePilot ? 1 : 0);
		if (!Test->TestNotNull(TEXT("Production connector is rendered"), Section)) return false;
		const auto OldVertices = Section->ProcVertexBuffer;
		if (bHomePilot) {
			double ContactError = 0; int32 Ends = 0;
			const auto* Entry=CityPrivateEntrances::Find(C.Mesh?FPaths::GetBaseFilename(C.Mesh):FString(TEXT("SM_CompactHome_GroundPilot")));
            const FVector Forward = Entry && (Entry->bFrontageFlight || bIndustrialStudy || bCivicStudy) ? FRotator(0,C.Turns*90.,0).RotateVector(FVector(0,-1,0)) : (Road-Door).GetSafeNormal2D();
            const double Length=FVector::DotProduct(Road-Door,Forward);
            const FVector TerminalAlong=FVector::CrossProduct(FVector::UpVector,Forward);
            double TerminalMin=MAX_dbl,TerminalMax=-MAX_dbl;
			for (const auto& V : OldVertices) if (V.Normal.Z > .5 && FMath::Abs(FVector::DotProduct(V.Position - Door,Forward)-Length)<.01 &&
                (!Entry || (!Entry->bFrontageFlight && !bIndustrialStudy && !bCivicStudy) || FMath::Abs(FVector::DotProduct(V.Position-Road,TerminalAlong))<=Entry->WidthCm*.5+.01)) {
				const double AlongAt=FVector::DotProduct(V.Position-Road,TerminalAlong);
                TerminalMin=FMath::Min(TerminalMin,AlongAt);TerminalMax=FMath::Max(TerminalMax,AlongAt);
				++Ends; ContactError = FMath::Max(ContactError,FMath::Abs(V.Position.Z-Board->SampleSidewalkSurface(V.Position)));
                if(bStagesStudy)
                {
                    bool OnPaving=false;
                    if(const auto* Public=Board->SidewalkSurface->GetProcMeshSection(0))
                        for(int32 I=0;I<Public->ProcIndexBuffer.Num();I+=3)
                        {
                            const FVector A=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I]].Position,B=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+1]].Position,C0=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+2]].Position;
                            if(FMath::Abs(FVector::CrossProduct(B-A,C0-A).Z)<.001) continue;
                            const FVector W=FMath::ComputeBaryCentric2D(V.Position,A,B,C0);
                            OnPaving |= W.X>=-.001 && W.Y>=-.001 && W.Z>=-.001 && FMath::Abs(V.Position.Z-(W.X*A.Z+W.Y*B.Z+W.Z*C0.Z))<.1;
                        }
                    Test->TestTrue(*FString::Printf(TEXT("%s apron endpoint %s touches real public paving"),C.Name,*V.Position.ToString()),OnPaving);
                }
			}
			Test->TestTrue(TEXT("Full-width path meets rendered public surface"), Ends >= 2 && ContactError <= .1);
            // When an avenue leaves exactly one flight-width of private
            // depth, the raised stair side also shares the public boundary.
            // Only the ground apron is a pedestrian join; verify both its
            // corners rather than mistaking every raised side vertex for it.
            if(Entry && (Entry->bFrontageFlight || bIndustrialStudy || bCivicStudy))
                Test->TestTrue(TEXT("Actual terminal apron spans the full reserved width"),TerminalMax-TerminalMin>=Entry->WidthCm-.1);
            const auto* Entrance=CityPrivateEntrances::Find(C.Mesh?FPaths::GetBaseFilename(C.Mesh):FString(TEXT("SM_CompactHome_GroundPilot")));
            FCityHomeWalk Flight;
            if(Entrance && Entrance->bFrontageFlight) Flight.BuildFrontage(Door,Road,Forward,Sim->Terrain,[&](const FVector& P){return Board->SampleSidewalkSurface(P);},Entrance->WidthCm,Entrance->DoorWidthCm,Entrance->LandingCm,Entrance->GroundReserveCm);
            else if((bIndustrialStudy || bCivicStudy) && Entrance && C.PartialFrontage && !FCityPlacementModel::RequiresFullRoadFrontage(Definition->DefinitionId)) {
                const auto Pose=Board->MakeBuildingTransform(Anchor,Board->ResolvePresentationDefinition(Placed.EntityId,*Definition),C.Turns);
                FVector Foot=Pose.TransformPosition(FVector(Entrance->EntryCm.X,Definition->Footprint.Height*400.,0))-Forward*(bCivicStudy?150.:Entrance->WidthCm);
                Foot.Z=FCityHomeWalk::GroundHeight(Sim->Terrain,Foot)+6.5;
                Flight.Build(Door,Foot,Sim->Terrain,[&](const FVector& P){return FCityHomeWalk::GroundHeight(Sim->Terrain,P)+6.5;},Entrance->LandingCm,Entrance->GroundReserveCm,Entrance->WidthCm);
            }
            else Flight.Build(Door,Road,Sim->Terrain,[&](const FVector& P){return Board->SampleSidewalkSurface(P);},Entrance?Entrance->LandingCm:50.,Entrance?Entrance->GroundReserveCm:100.,Entrance?Entrance->WidthCm:130.);
            if(bCommercialStudy && Entrance && Entrance->FrontageHalfSpanCm<800.) {
                const FTransform Pose=Board->MakeBuildingTransform(Anchor,Board->ResolvePresentationDefinition(Placed.EntityId,*Definition),C.Turns);
                for(const int32 SectionId:{1,2}) if(const auto* Private=Board->BuildingEntranceProfiles->GetProcMeshSection(SectionId))
                    for(const auto& V:Private->ProcVertexBuffer) {
                        const FVector Local=Pose.InverseTransformPosition(V.Position);
                        Test->TestTrue(TEXT("Stairs/rails clear both canopy posts"),FMath::Abs(Local.X)<700.);
                    }
            }
            const auto* Rails=Board->BuildingEntranceProfiles->GetProcMeshSection(2);
            Test->TestEqual(TEXT("Production rail section follows actual riser threshold"),Rails!=nullptr,Flight.ActualStepCount>4);
            double PavingError=0;
            for(int32 Class=1;Class<=4;++Class) if(const auto* Paving=Board->RoadSurfaceProfile->GetProcMeshSection(Class))
                for(int32 I=0;I<Paving->ProcIndexBuffer.Num();I+=3) {
                    const FVector P=(Paving->ProcVertexBuffer[Paving->ProcIndexBuffer[I]].Position+Paving->ProcVertexBuffer[Paving->ProcIndexBuffer[I+1]].Position+Paving->ProcVertexBuffer[Paving->ProcIndexBuffer[I+2]].Position)/3.;
                    PavingError=FMath::Max(PavingError,FMath::Abs(P.Z-FCityHomeWalk::GroundHeight(Sim->Terrain,P)-FCityRoadRibbonMesh::SurfaceLiftCm));
                }
            Test->TestTrue(TEXT("Native road interiors conform to actual terrain"),PavingError<.02);
            if(bIndustrialStudy || bCivicStudy) {
                double PrivatePenetration=0;
                for(const auto& V:OldVertices)
                    if(V.Normal.Z>.001) PrivatePenetration=FMath::Max(PrivatePenetration,FCityHomeWalk::GroundHeight(Sim->Terrain,V.Position)-V.Position.Z);
                Test->TestTrue(TEXT("All actual personnel paving clears the terrain including crossing corners"),PrivatePenetration<.02);
                const FString PrivateFile=Directory/TEXT("PrivatePaving.csv");
                if(CaseIndex==(bUtilityBatchStudy?152:bUtilityStudy?141:bCivicStudy?102:76)) FFileHelper::SaveStringToFile(TEXT("case,terrain_penetration_cm\n"),*PrivateFile);
                FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s,%.4f\n"),C.Name,PrivatePenetration),*PrivateFile,FFileHelper::EEncodingOptions::AutoDetect,&IFileManager::Get(),FILEWRITE_Append);
                const auto& Entity=City.GetEntities().FindChecked(Placed.EntityId);
                const auto Presentation=Board->ResolvePresentationDefinition(Placed.EntityId,*Definition);
                const auto Bays=CityPrivateEntrances::Bays(Presentation.PresentationMesh.GetAssetName());
                const auto* Freight=Board->BuildingEntranceProfiles->GetProcMeshSection(0);
                const FString BayFile=Directory/TEXT("Bays.csv");
                if(CaseIndex==(bUtilityBatchStudy?152:bUtilityStudy?141:bCivicStudy?102:76)) FFileHelper::SaveStringToFile(TEXT("case,bay,width_cm,public_error_cm,grade_percent,raised_dock\n"),*BayFile);
                for(int32 BayIndex=0;BayIndex<Bays.Num();++BayIndex) {
                    FVector BayStart,PublicEnd;
                    if(!Test->TestTrue(TEXT("Each actual industrial bay reaches a supported road frontage"),Board->FindVehicleBayConnection(Entity,Presentation,Bays[BayIndex],BayStart,PublicEnd))) continue;
                    if(!Test->TestNotNull(TEXT("Freight uses continuous vehicle paving"),Freight)) continue;
                    const FVector F=FRotator(0,C.Turns*90.,0).RotateVector(FVector(0,-1,0)),R=FVector::CrossProduct(FVector::UpVector,F);
                    double Error=0,Min=MAX_dbl,Max=-MAX_dbl,Grade=0;int32 BayEnds=0;
                    for(const auto& V:Freight->ProcVertexBuffer) {
                        if(V.Normal.Z<=.001) continue;
                        const auto BayPose=Board->MakeBuildingTransform(Anchor,Presentation,C.Turns);
                        const FVector Local=BayPose.InverseTransformPosition(V.Position);
                        Test->TestTrue(TEXT("Raised freight paving stays clear of the personnel flight"),Local.Y>=Definition->Footprint.Height*400.-Entrance->WidthCm-.1 || Local.X<Entrance->EntryCm.X-Entrance->WidthCm*.5-.1 || Local.X>Entrance->EntryCm.X+Entrance->WidthCm*.5+.1);
                        Grade=FMath::Max(Grade,100.*FVector2D(V.Normal.X,V.Normal.Y).Size()/V.Normal.Z);
                        Test->TestTrue(TEXT("Actual vehicle paving clears clipped terrain"),V.Position.Z>=FCityHomeWalk::GroundHeight(Sim->Terrain,V.Position)-.02);
                        const double Across=FVector::DotProduct(V.Position-PublicEnd,R);
                        if(FMath::Abs(FVector::DotProduct(V.Position-PublicEnd,F))>.01 || FMath::Abs(Across)>Bays[BayIndex].WidthCm*.5+.01) continue;
                        if(bCivicStudy) {
                            bool OnPaving=false;
                            if(const auto* Public=Board->SidewalkSurface->GetProcMeshSection(0))
                                for(int32 I=0;I<Public->ProcIndexBuffer.Num();I+=3) {
                                    const FVector A=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I]].Position,
                                        B=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+1]].Position,
                                        C0=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+2]].Position;
                                    if(FMath::Abs(FVector::CrossProduct(B-A,C0-A).Z)<.001) continue;
                                    const auto W=FMath::ComputeBaryCentric2D(V.Position,A,B,C0);
                                    OnPaving |= W.X>=-.001 && W.Y>=-.001 && W.Z>=-.001 &&
                                        FMath::Abs(V.Position.Z-(W.X*A.Z+W.Y*B.Z+W.Z*C0.Z))<.1;
                                }
                            Test->TestTrue(*FString::Printf(TEXT("%s bay%d endpoint%s touches actual public paving"),C.Name,BayIndex,*V.Position.ToString()),OnPaving);
                        }
                        ++BayEnds;Min=FMath::Min(Min,Across);Max=FMath::Max(Max,Across);
                        Error=FMath::Max(Error,FMath::Abs(V.Position.Z-Board->SampleSidewalkSurface(V.Position)));
                    }
                    Test->TestTrue(TEXT("Full vehicle width meets real public paving without a step"),BayEnds>=2 && Max-Min>=Bays[BayIndex].WidthCm-.1 && Error<.1);
                    if(Bays[BayIndex].bRaisedDock) Test->TestTrue(TEXT("Truck approach stays below the raised loading ledge"),Bays[BayIndex].EntryCm.Z<20.);
                    FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s,%d,%.2f,%.3f,%.2f,%d\n"),C.Name,BayIndex,Bays[BayIndex].WidthCm,Error,Grade,Bays[BayIndex].bRaisedDock?1:0),*BayFile,FFileHelper::EEncodingOptions::AutoDetect,&IFileManager::Get(),FILEWRITE_Append);
                }
                if(!Bays.IsEmpty() && Rails) {
                    const FTransform Pose=Board->MakeBuildingTransform(Anchor,Presentation,C.Turns);
                    for(const auto& V:Rails->ProcVertexBuffer) {
                        const FVector Local=Pose.InverseTransformPosition(V.Position);
                        Test->TestTrue(TEXT("Personnel stair rails never enter a loading lane"),Local.X>=Entrance->EntryCm.X-Entrance->WidthCm*.5-5. && Local.X<=Entrance->EntryCm.X+Entrance->WidthCm*.5+5.);
                    }
                }
            }
            Test->TestTrue(*FString::Printf(TEXT("%s has a complete threshold-to-ground flight"),C.Name),!Flight.Vertices.IsEmpty());
            const FString RailFile=Directory/TEXT("RoadRails.csv");
            if(CaseIndex==(bUtilityBatchStudy?152:bUtilityStudy?141:bCivicStudy?102:bIndustrialStudy?76:bCommercialStudy?54:bStagesStudy?37:bResidentialStudy?25:15)) FFileHelper::SaveStringToFile(TEXT("case,actual_risers,rail_vertices,road_terrain_error_cm\n"),*RailFile);
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s,%d,%d,%.4f\n"),C.Name,Flight.ActualStepCount,Rails?Rails->ProcVertexBuffer.Num():0,PavingError),*RailFile,FFileHelper::EEncodingOptions::AutoDetect,&IFileManager::Get(),FILEWRITE_Append);
			int32 Low = MAX_int32, High = MIN_int32;
			for (int32 Y = Anchor.Y*2; Y <= (Anchor.Y+ParcelSize.Y)*2; ++Y)
				for (int32 X = Anchor.X*2; X <= (Anchor.X+ParcelSize.X)*2; ++X) {
					const int32 H = Sim->Terrain.GetSampleHeightCm(X,Y); Low = FMath::Min(Low,H); High = FMath::Max(High,H);
				}
			Metrics += FString::Printf(TEXT("%s,%s,%d,%d,%d,%d,0,%.3f,0\n"), C.Name, ModeName(),ParcelSize.X,ParcelSize.Y,C.Turns,High-Low,ContactError);
			FFileHelper::SaveStringToFile(Metrics, *(Directory / TEXT("Contacts.csv")));
            if ((bCommercialStudy || bIndustrialStudy) && C.ExtraCells == 9) {
                Catalog->Definitions=OriginalCatalog->Definitions;
                Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                Catalog->CommercialPresentationVariants=OriginalCatalog->CommercialPresentationVariants;
                Sim->RegisterBuildingCatalog(Catalog);
                const auto* Shop=Catalog->FindDefinition(bIndustrialStudy?TEXT("Factory"):TEXT("Shop"));
                const FName Desired[]={TEXT("A_Brick"),TEXT("B_Timber"),TEXT("C_Clapboard"),TEXT("D_Corrugated")};
                const int32 Sites[]={14,21,24,27,18,30,33,36};
                TArray<uint64> Shops;
                for(int32 I=0;I<8;++I) {
                    // The existing entity already owns synthetic subsystem
                    // records. Retain it as the medium shop rather than
                    // orphaning those records through direct fixture demolition.
                    if(I==4) {Shops.Add(Placed.EntityId);continue;}
                    bool Matched=false;
                    for(int32 Attempt=0;Attempt<64 && !Matched;++Attempt) {
                        auto P=City.BeginPreview(*Shop,{Sites[I],16}); const auto H=City.CommitPlacement(P,*Shop);
                        if(!Test->TestTrue(TEXT("Normal-catalog commercial lot places"),H.IsSuccess())) return false;
                        FName Appearance; OriginalCatalog->FindLowDensityPresentation(H.EntityId,FName(C.Id),ECityDensityLevel::Low,false,Appearance);
                        Matched=I>=4 || Appearance==Desired[I];
                        if(Matched) Shops.Add(H.EntityId); else City.DemolishEntity(H.EntityId);
                    }
                    if(!Test->TestTrue(TEXT("Playable shops cover four normal appearances"),Matched)) return false;
                }
                Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                Sim->CityIdentity.Id=bIndustrialStudy?TEXT("00000000000000000000000000000195"):TEXT("00000000000000000000000000000194");
                Sim->CityIdentity.Name=bIndustrialStudy?TEXT("Industrial hills pilot"):TEXT("Commercial hills pilot");
                Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
                const FString Save=Directory/(bIndustrialStudy?TEXT("Industrial-Ground-Playtest.json"):TEXT("Commercial-Ground-Playtest.json"));
                FString Error,Json;FCitySaveDocument Doc;
                if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
                Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;Doc.bSimulationPaused=true;
                for(int32 I=0;I<Shops.Num();++I) {
                    auto& State=Doc.Development.States.FindChecked(Shops[I]);
                    State.DensityLevel=I<4?ECityDensityLevel::Low:I==5?ECityDensityLevel::High:ECityDensityLevel::Medium;
                    if(I==6) Doc.Fire.DamagedBuildings.Add(Shops[I],FCityFireDamage{50,-1,0});
                    if(I==7) {State.Stage=ECityDevelopmentStage::Construction;State.ConstructionCompleteStep=Doc.Economy.FixedStepId+Shop->ConstructionSteps;}
                }
                if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
                Test->TestEqual(TEXT("Eight normal-catalog shops survive current save/load"),Sim->GetPlacementModel().GetEntities().Num(),8);
                for(int32 I=0;I<Shops.Num();++I) {
                    const auto Resolved=Board->ResolvePresentationDefinition(Shops[I],*Shop);
                    const auto* SavedEntrance=CityPrivateEntrances::Find(Resolved.PresentationMesh.GetAssetName());
                    Test->TestNotNull(TEXT("Saved commercial density/fire/construction resolves a private approach"),SavedEntrance);
                    if(I<4) {
                        FName Appearance; OriginalCatalog->FindLowDensityPresentation(Shops[I],FName(C.Id),ECityDensityLevel::Low,false,Appearance);
                        Test->TestEqual(TEXT("Saved IDs retain intended commercial appearance"),Appearance,Desired[I]);
                    }
                }
                Board->PrepareCityView();CameraSpanCm=14500;
            }
            if(bUtilityStudy && !bUtilityBatchStudy && C.ExtraCells==9) {
                Catalog->Definitions=OriginalCatalog->Definitions;
                Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                Sim->RegisterBuildingCatalog(Catalog);
                const auto* Native=OriginalCatalog->FindDefinition(TEXT("NuclearPower"));
                if(!Test->TestNotNull(TEXT("Native nuclear catalog exists"),Native)) return false;
                TArray<uint64> Entities={Placed.EntityId};TArray<int32> Sites={Anchor.X};
                for(int32 X=Anchor.X+5;X<=52 && Entities.Num()<3;++X) {
                    auto Candidate=City.BeginPreview(*Native,{X,16});
                    if(!City.ValidatePlacement(Candidate,*Native).IsSuccess()) continue;
                    const auto Result=City.CommitPlacement(Candidate,*Native);
                    if(!Test->TestTrue(TEXT("Normal nuclear pilot placement commits"),Result.IsSuccess())) return false;
                    Entities.Add(Result.EntityId);Sites.Add(X);X+=4;
                }
                if(!Test->TestEqual(TEXT("Three nuclear lifecycle sites fit actual terrain constraints"),Entities.Num(),3)) return false;
                Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                Sim->SetSimulationPaused(true);
                Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
                Sim->CityIdentity.Id=TEXT("00000000000000000000000000000218");
                Sim->CityIdentity.Name=TEXT("Utility hills pilot");
                FString Json,Error;FCitySaveDocument Doc;
                const FString Save=Directory/TEXT("Utility-Ground-Playtest.json");
                if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
                Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;Doc.bSimulationPaused=true;
                Doc.Fire.DamagedBuildings.Add(Entities[1],FCityFireDamage{50,-1,0});
                auto& Construction=Doc.Development.States.FindChecked(Entities[2]);
                Construction.Stage=ECityDevelopmentStage::Construction;
                Construction.ConstructionCompleteStep=Doc.Economy.FixedStepId+Native->ConstructionSteps;

                if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
                Test->TestTrue(TEXT("Native utility catalog reserves4x4"),Native && Native->Footprint.Width==4 && Native->Footprint.Height==4);
                for(const auto& Pair:Sim->GetPlacementModel().GetEntities()) {
                    const auto Resolved=Board->ResolvePresentationDefinition(Pair.Key,*Native);
                    Test->TestNotNull(TEXT("Saved nuclear resolves actual utility entrance"),CityPrivateEntrances::Find(Resolved.PresentationMesh.GetAssetName()));
                }
                Test->TestTrue(TEXT("Saved fire damage selects the matching new nuclear shape"),Board->ResolvePresentationDefinition(Entities[1],*Native).PresentationMesh.GetAssetName()==TEXT("SM_NuclearFacility_v6_Damaged"));
                Test->TestTrue(TEXT("Saved construction selects a measured temporary deck"),Board->ResolvePresentationDefinition(Entities[2],*Native).PresentationMesh.GetAssetName()==TEXT("SM_ResidentialGround_Construction"));
                Board->PrepareCityView();
                CameraSpanCm=(Sites.Last()-Sites[0]+6)*800.;Center.X=(Sites[0]+Sites.Last()+4)*400.;
            }
            if(bUtilityBatchStudy && C.ExtraCells==9) {
                Catalog->Definitions=OriginalCatalog->Definitions;
                Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                Sim->RegisterBuildingCatalog(Catalog);
                const FName Ids[]={TEXT("CoalPower"),TEXT("OilPower"),TEXT("Power"),TEXT("Power"),TEXT("Power"),
                    TEXT("Service"),TEXT("Service"),TEXT("Service"),TEXT("WaterTower"),TEXT("WindPower"),TEXT("SolarPower"),TEXT("OilPower"),TEXT("CoalPower"),TEXT("NuclearPower")};
                TArray<uint64> Entities;Entities.SetNumZeroed(UE_ARRAY_COUNT(Ids));Entities[0]=Placed.EntityId;
                FBox CityBounds(ForceInit);
                CityBounds+=FVector(Anchor.X*800.,Anchor.Y*800.,0);
                CityBounds+=FVector((Anchor.X+4)*800.,(Anchor.Y+4)*800.,0);
                // Fit large reservations first on both sides of the existing valid
                // hillside avenue, without forcing new roads across the river.
                for(const int32 I:{5,6,7,1,2,3,4,10,11,12,13,9,8}) {
                    const auto* Native=Catalog->FindDefinition(Ids[I]);
                    if(!Test->TestNotNull(TEXT("Normal utility catalog definition exists"),Native)) return false;
                    bool Valid=false;
                    for(const int32 Turns:{0,2}) {
                        const int32 Y=Turns==0?16:15-Native->Footprint.Height;
                        for(int32 X=14;X<=58-Native->Footprint.Width;++X) {
                            auto Candidate=City.BeginPreview(*Native,{X,Y},Turns);
                            if(!City.ValidatePlacement(Candidate,*Native).IsSuccess()) continue;
                            const auto R=City.CommitPlacement(Candidate,*Native);
                            if(!Test->TestTrue(TEXT("Normal utility placement commits without terrain bypass"),R.IsSuccess())) return false;
                            Entities[I]=R.EntityId;Valid=true;
                            CityBounds+=FVector(X*800.,Y*800.,0);
                            CityBounds+=FVector((X+Native->Footprint.Width)*800.,(Y+Native->Footprint.Height)*800.,0);
                            break;
                        }
                        if(Valid) break;
                    }
                    if(!Test->TestTrue(TEXT("Utility finds a real road/terrain reservation"),Valid)) return false;
                }
                Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                Sim->SetSimulationPaused(true);
                Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
                Sim->CityIdentity.Id=TEXT("00000000000000000000000000000272");
                Sim->CityIdentity.Name=TEXT("Utility batch hills");
                const FString Save=Directory/TEXT("Utility-Batch-Playtest.json");
                FString Json,Error;FCitySaveDocument Doc;
                const auto ExpectedTerrain=Sim->Terrain.GetQuantizedHeightsCm();
                if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
                Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;Doc.bSimulationPaused=true;
                for(int32 I=0;I<Entities.Num();++I) {
                    auto& State=Doc.Development.States.FindChecked(Entities[I]);
                    State.DensityLevel=I==4 || I==7?ECityDensityLevel::High:I==3 || I==6?ECityDensityLevel::Medium:ECityDensityLevel::Low;
                }
                Doc.Fire.DamagedBuildings.Add(Entities[11],FCityFireDamage{50,-1,0});
                auto& Construction=Doc.Development.States.FindChecked(Entities[12]);
                Construction.Stage=ECityDevelopmentStage::Construction;
                Construction.ConstructionCompleteStep=Doc.Economy.FixedStepId+Catalog->FindDefinition(TEXT("CoalPower"))->ConstructionSteps;
                if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
                Test->TestEqual(TEXT("All eleven versions, nuclear and damage/construction survive normal save/load"),Sim->GetPlacementModel().GetEntities().Num(),14);
                Test->TestTrue(TEXT("Saved utility city retains every terrain sample"),ExpectedTerrain==Sim->Terrain.GetQuantizedHeightsCm());
                for(int32 I=0;I<Entities.Num();++I) {
                    const auto Resolved=Board->ResolvePresentationDefinition(Entities[I],*Catalog->FindDefinition(Ids[I]));
                    Test->TestNotNull(TEXT("Saved utility lifecycle resolves its actual entrance"),CityPrivateEntrances::Find(Resolved.PresentationMesh.GetAssetName()));
                }
                Test->TestTrue(TEXT("Saved high power selects reviewed high body"),Board->ResolvePresentationDefinition(Entities[4],*Catalog->FindDefinition(TEXT("Power"))).PresentationMesh.GetAssetName()==TEXT("SM_power_high_v3"));
                Test->TestTrue(TEXT("Saved high water selects reviewed high body"),Board->ResolvePresentationDefinition(Entities[7],*Catalog->FindDefinition(TEXT("Service"))).PresentationMesh.GetAssetName()==TEXT("SM_water_high_v2"));
                Test->TestTrue(TEXT("Saved damage selects matching approved oil body"),Board->ResolvePresentationDefinition(Entities[11],*Catalog->FindDefinition(TEXT("OilPower"))).PresentationMesh.GetAssetName()==TEXT("SM_oil_v3_Damaged"));
                ++Sim->PresentationGeneration;Board->PrepareCityView();Board->UpdateWindRotors(0);
                Test->TestEqual(TEXT("Saved normal city renders its wind nacelle"),Board->WindNacelleInstances->GetInstanceCount(),1);
                Test->TestEqual(TEXT("Saved normal city renders its wind rotor"),Board->WindRotorInstances->GetInstanceCount(),1);
                Center=CityBounds.GetCenter();CameraSpanCm=CityBounds.GetSize().GetMax()+3200.;
            }
            if(bCivicStudy && !bUtilityStudy && C.ExtraCells==9) {
                Catalog->Definitions=OriginalCatalog->Definitions;
                Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                Catalog->CommercialPresentationVariants=OriginalCatalog->CommercialPresentationVariants;
                Sim->RegisterBuildingCatalog(Catalog);
                City.ApplyMonthlyEconomy(1000000,0);
                const FName Ids[]={TEXT("Police"),TEXT("Fire"),TEXT("School"),TEXT("PrimarySchool"),TEXT("SecondarySchool"),TEXT("College"),TEXT("Hospital"),TEXT("Hospital"),TEXT("Police"),TEXT("Fire")};
                const int32 Sites[]={18,14,23,27,30,34,38,43,48,52};
                TArray<uint64> Entities={Placed.EntityId};
                for(int32 I=1;I<UE_ARRAY_COUNT(Ids);++I) {
                    const auto* Civic=Catalog->FindDefinition(Ids[I]);
                    if(!Test->TestNotNull(TEXT("Playable civic definition exists"),Civic)) return false;
                    auto CivicPreview=City.BeginPreview(*Civic,{Sites[I],16});
                    const auto H=City.CommitPlacement(CivicPreview,*Civic);
                    if(!Test->TestTrue(*FString::Printf(TEXT("Normal-catalog %s at%d places (failure%d)"),*Ids[I].ToString(),Sites[I],static_cast<int32>(H.Failure)),H.IsSuccess())) return false;
                    Entities.Add(H.EntityId);
                }
                Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                Sim->CityIdentity.Id=TEXT("00000000000000000000000000000198");
                Sim->CityIdentity.Name=TEXT("Civic hills pilot");
                Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
                const FString Save=Directory/TEXT("Civic-Ground-Playtest.json");
                FString Error,Json;FCitySaveDocument Doc;
                if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
                Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;Doc.bSimulationPaused=true;
                for(int32 I=0;I<Entities.Num();++I) {
                    auto& State=Doc.Development.States.FindChecked(Entities[I]);
                    State.DensityLevel=I==1 || I==8?ECityDensityLevel::High:I==6 || I==9?ECityDensityLevel::Medium:ECityDensityLevel::Low;
                    if(I==7) Doc.Fire.DamagedBuildings.Add(Entities[I],FCityFireDamage{50,-1,0});
                }
                if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
                Test->TestEqual(TEXT("Ten normal-catalog civic buildings survive save/load"),Sim->GetPlacementModel().GetEntities().Num(),10);
                for(int32 I=0;I<Entities.Num();++I) {
                    const auto Resolved=Board->ResolvePresentationDefinition(Entities[I],*Catalog->FindDefinition(Ids[I]));
                    Test->TestNotNull(TEXT("Saved civic state resolves its measured entrance"),CityCivicEntrances::Find(Resolved.PresentationMesh.GetAssetName()));
                }
                Board->PrepareCityView();CameraSpanCm=33000;Center.X=27600;
            }
			if (C.ExtraCells == 9 && bStagesStudy && !bCommercialStudy && !bIndustrialStudy && !bCivicStudy) {
                // Save normal-catalog density/stage states, never the transient
                // appearance override used for isolated geometry captures.
                Catalog->Definitions=OriginalCatalog->Definitions;
                Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                Sim->RegisterBuildingCatalog(Catalog);
                const auto* Home=Catalog->FindDefinition(TEXT("Home"));
                TArray<uint64> Homes={Placed.EntityId};
                for(const int32 X:{14,21,24,27}) {
                    auto P=City.BeginPreview(*Home,{X,16});const auto H=City.CommitPlacement(P,*Home);
                    if(!Test->TestTrue(TEXT("Normal-catalog residential stage lot places"),H.IsSuccess())) return false;
                    Homes.Add(H.EntityId);
                }
                Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                Sim->CityIdentity.Id=TEXT("00000000000000000000000000000190");
                Sim->CityIdentity.Name=TEXT("Apartment hills pilot");
                Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
                const FString Save=Directory/TEXT("Residential-Stages-Playtest.json");
                FString Error,Json;FCitySaveDocument Doc;
                if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
                // This synthetic city explicitly contains pre-existing fire
                // damage, as in the lifecycle fixture; end its new-city grace.
                Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;
                for(int32 I=0;I<Homes.Num();++I) {
                    auto& State=Doc.Development.States.FindChecked(Homes[I]);
                    State.DensityLevel=I==1 || I==4?ECityDensityLevel::High:ECityDensityLevel::Medium;
                    if(I==2) {State.Stage=ECityDevelopmentStage::Construction;State.ConstructionCompleteStep=Doc.Economy.FixedStepId+Home->ConstructionSteps;}
                    if(I>=3) Doc.Fire.DamagedBuildings.Add(Homes[I],FCityFireDamage{50,-1,0});
                }
                Doc.bSimulationPaused=true;
                if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
                Test->TestEqual(TEXT("Five actual homes survive stage save/load"),Sim->GetPlacementModel().GetEntities().Num(),5);
                const TCHAR* Names[]={TEXT("SM_ResidentialGround_Medium"),TEXT("SM_ResidentialGround_High"),TEXT("SM_ResidentialGround_Construction"),TEXT("SM_ResidentialGround_Medium_Damaged"),TEXT("SM_ResidentialGround_High_Damaged")};
                for(int32 I=0;I<Homes.Num();++I)
                    Test->TestEqual(TEXT("Playable normal catalog reproduces actual density/fire/construction"),Board->ResolvePresentationDefinition(Homes[I],*Home).PresentationMesh.GetAssetName(),FString(Names[I]));
                Board->PrepareCityView();CameraSpanCm=11000;
            }
			if (C.ExtraCells == 9 && !bStagesStudy) {
                if(bResidentialStudy)
                {
                    Catalog->LowDensityPresentationVariants=OriginalCatalog->LowDensityPresentationVariants;
                    Sim->RegisterBuildingCatalog(Catalog);
                    const FName Desired[3]={TEXT("B_Timber"),TEXT("C_Clapboard"),TEXT("D_Corrugated")};
                    const FCityCellCoord Sites[3]={{14,16},{21,16},{24,16}};
                    for(int32 House=0;House<3;++House)
                    {
                        bool Matched=false;
                        for(int32 Attempt=0;Attempt<32 && !Matched;++Attempt)
                        {
                            auto NewPreview=City.BeginPreview(*Definition,Sites[House]);
                            const auto NewHouse=City.CommitPlacement(NewPreview,*Definition);
                            if(!Test->TestTrue(TEXT("Residential playable lot places"),NewHouse.IsSuccess())) return false;
                            FName Appearance;
                            OriginalCatalog->FindLowDensityPresentation(NewHouse.EntityId,TEXT("Home"),ECityDensityLevel::Low,false,Appearance);
                            Matched=Appearance==Desired[House];
                            if(!Matched) City.DemolishEntity(NewHouse.EntityId);
                        }
                        if(!Test->TestTrue(TEXT("Playable city selects each remaining healthy appearance normally"),Matched)) return false;
                    }
                    Sim->Development.InitializeExistingAsOperational(City,Catalog,0);
                    Test->TestEqual(TEXT("Playable residential city has A/B/C/D homes"),City.GetEntities().Num(),4);
                    CameraSpanCm=9000;
                }
				const auto ExpectedTerrain = Sim->Terrain.GetQuantizedHeightsCm(); FString Error;
				Sim->CityIdentity.Id = bResidentialStudy?TEXT("00000000000000000000000000000170"):TEXT("00000000000000000000000000000125");
                Sim->CityIdentity.Name = bResidentialStudy?TEXT("Residential hills pilot"):TEXT("Home hills pilot");
				Sim->CityIdentity.DisasterFrequency = ECityDisasterFrequency::Off;
				const FString Save = Directory / (bResidentialStudy?TEXT("Residential-Hilly-Playtest.json"):TEXT("Home-Hilly-Playtest.json"));
				Test->TestTrue(TEXT("Hilly fixture saves current terrain parameters"), Sim->SaveCityToPath(Save,Error));
				Test->TestTrue(TEXT("Hilly fixture reloads"), Sim->LoadCityFromPath(Save,Error,false));
				Test->TestTrue(TEXT("Hilly fixture reproduces every terrain sample on load"), ExpectedTerrain==Sim->Terrain.GetQuantizedHeightsCm());
                if(bResidentialStudy)
                {
                    const auto& Reloaded=Sim->GetPlacementModel();
                    Test->TestEqual(TEXT("Four residential homes survive the actual save/load"),Reloaded.GetEntities().Num(),4);
                    TSet<FName> Appearances;
                    for(const auto& Pair:Reloaded.GetEntities())
                    {
                        FName Appearance;
                        OriginalCatalog->FindLowDensityPresentation(Pair.Key,TEXT("Home"),ECityDensityLevel::Low,false,Appearance);
                        Appearances.Add(Appearance);
                    }
                    Test->TestEqual(TEXT("Saved IDs reproduce all four normal-catalog appearances"),Appearances.Num(),4);
                }
				Board->PrepareCityView();
			}
			auto* Capture = Camera->GetCaptureComponent2D(); Capture->ClearShowOnlyComponents();
			TArray<UPrimitiveComponent*> Components; Board->GetComponents(Components);
			for (auto* Component : Components) Capture->ShowOnlyComponent(Component);
			return true;
		}
		double OldError = 0;
		for (int32 I = OldVertices.Num() - 2; I < OldVertices.Num(); ++I)
		{
			const auto& P = OldVertices[I].Position;
			OldError = FMath::Max(OldError, FMath::Abs(P.Z - (Board->GetRoadProfileHeight(P.X / 800, P.Y / 800) + FCityRoadRibbonMesh::SurfaceLiftCm)));
		}
		if (Mode == 2 && !bFoundationStudy)
		{
			TArray<FVector> V, N; TArray<FVector2D> UV; TArray<FProcMeshTangent> T; TArray<int32> Indices;
			for (const uint32 Index : Section->ProcIndexBuffer) Indices.Add(static_cast<int32>(Index));
			// Match the clockwise upper faces of the actual terrain/road mesh.
			// Production entrance triangles currently face the opposite way.
			for (int32 I = 0; I < Indices.Num(); I += 3) Swap(Indices[I + 1], Indices[I + 2]);
			for (int32 I = 0; I < OldVertices.Num(); ++I)
			{
				FVector P = OldVertices[I].Position;
				const double Along = static_cast<double>(I / 2) / 4;
				const FVector End = OldVertices[OldVertices.Num() - 2 + I % 2].Position;
				const double RoadTop = Board->GetRoadProfileHeight(End.X / 800, End.Y / 800) + FCityRoadRibbonMesh::SurfaceLiftCm + .5;
				P.Z = FMath::Lerp(Door.Z, RoadTop, Along);
				if (I > 1 && I < OldVertices.Num() - 2) P.Z = FMath::Max(P.Z, Sim->Terrain.SampleHeightCm(P.X / 800, P.Y / 800) + 42);
				V.Add(P); N.Add(FVector::UpVector); UV.Add(OldVertices[I].UV0);
			}
			Board->BuildingEntranceProfiles->ClearAllMeshSections();
			Board->BuildingEntranceProfiles->CreateMeshSection(0, V, Indices, N, UV, TArray<FColor>(), T, false);
			for (int32 I = 0; I < Indices.Num(); I += 3)
				Test->TestTrue(TEXT("Study upper faces use the same winding as visible terrain/road paving"), FVector::CrossProduct(V[Indices[I + 1]] - V[Indices[I]], V[Indices[I + 2]] - V[Indices[I]]).Z < 0);
		}
		Section = Board->BuildingEntranceProfiles->GetProcMeshSection(0);
		double ContactError = 0;
		for (int32 I = Section->ProcVertexBuffer.Num() - 2; I < Section->ProcVertexBuffer.Num(); ++I)
		{
			const auto& P = Section->ProcVertexBuffer[I].Position;
			ContactError = FMath::Max(ContactError, FMath::Abs(P.Z - (Board->GetRoadProfileHeight(P.X / 800, P.Y / 800) + FCityRoadRibbonMesh::SurfaceLiftCm)));
		}
		if (Mode == 2 && !bFoundationStudy) Test->TestTrue(TEXT("Study connector meets the road surface across its full endpoint width"), ContactError <= .51);
		const double Offset = C.Turns == 0 ? FMath::Abs(Door.X - Center.X) : FMath::Abs(Door.Y - Center.Y);
		int32 Low = MAX_int32, High = MIN_int32;
		for (int32 Y = 32; Y <= (16 + ParcelSize.Y) * 2; ++Y)
			for (int32 X = 36; X <= (18 + ParcelSize.X) * 2; ++X) { const int32 H = Sim->Terrain.GetSampleHeightCm(X, Y); Low = FMath::Min(Low, H); High = FMath::Max(High, H); }
		Metrics += FString::Printf(TEXT("%s,%s,%d,%d,%d,%d,%.3f,%.3f,%.3f\n"), C.Name, ModeName(), ParcelSize.X, ParcelSize.Y, C.Turns, High - Low, OldError, ContactError, Offset);
		Test->TestTrue(TEXT("Incremental study metrics saved"), FFileHelper::SaveStringToFile(Metrics, *(Directory / TEXT("Contacts.csv"))));
		UE_LOG(LogTemp, Display, TEXT("GROUNDPLOTS_STUDY %s %s ContactError=%.3fcm Old=%.3fcm"), C.Name, ModeName(), ContactError, OldError);
		Test->AddInfo(FString::Printf(TEXT("GroundPlots %s %s roadContactError=%.3fcm old=%.3fcm connectorLateralOffset=%.1fcm"), C.Name, ModeName(), ContactError, OldError, Offset));
		auto* Capture = Camera->GetCaptureComponent2D(); Capture->ClearShowOnlyComponents();
		TArray<UPrimitiveComponent*> Components; Board->GetComponents(Components);
		for (auto* Component : Components) Capture->ShowOnlyComponent(Component);
		return true;
	}
	bool Finish(bool Success)
	{
		Test->TestTrue(TEXT("Study metrics saved"), FFileHelper::SaveStringToFile(Metrics, *(Directory / TEXT("Contacts.csv"))));
		if (Camera) Camera->Destroy();
		if (Sim && OriginalCatalog.IsValid()) { Sim->RegisterBuildingCatalog(OriginalCatalog.Get()); Sim->ResetCity(false); Board->BuildingCatalog = OriginalCatalog.Get(); Board->PrepareCityView(); Board->SetActorTickEnabled(bOriginalBoardTick); }
		if (Success) Test->AddInfo(FString::Printf(TEXT("CITYSIM_GROUNDPLOTS_STUDY_COMPLETE Cases=%d Captures=%d Directory=%s NuclearFootprint4x4=%d"), bUtilityBatchStudy ? 25 : bUtilityStudy ? 11 : bCivicStudy ? 39 : bIndustrialStudy ? 26 : bCommercialStudy ? 22 : bStagesStudy ? 17 : bResidentialStudy ? 12 : bHomePilot ? 10 : bFoundationStudy ? 25 : 8, Captures, *Directory,bUtilityStudy?1:0));
		return true;
	}
	FAutomationTestBase* Test; ACityBoardActor* Board = nullptr; UCitySimulationSubsystem* Sim = nullptr;
	TStrongObjectPtr<UCityBuildingCatalog> OriginalCatalog; ASceneCapture2D* Camera = nullptr;
	FString Directory, Metrics; FIntPoint ParcelSize; FVector Door, Road, Center;
    bool bResidentialStudy = false;
    bool bStagesStudy = false;
    bool bCommercialStudy = false;
    bool bIndustrialStudy = false;
    bool bCivicStudy = false;
    bool bUtilityStudy = false;
    bool bUtilityBatchStudy = false;
	int32 CaseIndex = 0, Mode = 0, Stage = 0, Attempts = 0, Captures = 0; double Next = 0, CameraSpanCm = 0;
	bool bOriginalBoardTick = true, bFoundationStudy = false, bHomePilot = false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGroundPlotStudyTest, "CitySim.Desktop.GroundPlotStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityGroundPlotStudyTest::RunTest(const FString&)
{
	FString Profile;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityGroundPlotStudy")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
	{ AddError(TEXT("Requires explicit CityGroundPlotStudy flag and isolated EditorVisualTests profile")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityResidentialGroundStudyTest, "CitySim.Desktop.ResidentialGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityResidentialGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if (!FParse::Param(FCommandLine::Get(), TEXT("CityGroundPlotStudy")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    { AddError(TEXT("Residential study needs its isolated CI visual profile")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityResidentialStagesStudyTest,"CitySim.Desktop.ResidentialStagesStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityResidentialStagesStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Stage study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCommercialGroundStudyTest,"CitySim.Desktop.CommercialGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityCommercialGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Commercial study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true,true));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialGroundStudyTest,"CitySim.Desktop.IndustrialGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityIndustrialGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Industrial study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true,false,true));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCivicGroundStudyTest,"CitySim.Desktop.CivicGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityCivicGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Civic study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true,false,false,true));return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityGroundStudyTest,"CitySim.Desktop.UtilityGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Utility study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true,false,false,false,true));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedUtilityGroundStudyTest,"CitySim.Desktop.ApprovedUtilityGroundStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedUtilityGroundStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Approved utility study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityGroundPlotStudyCommand(this,true,true,false,false,false,false,true));return true;
}

#endif
