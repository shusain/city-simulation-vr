#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityVRToolController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "Engine/TextureRenderTargetCube.h"

class FCityDynamicReflectionCommand : public IAutomationLatentCommand
{
public:
	explicit FCityDynamicReflectionCommand(FAutomationTestBase* In) : Test(In) {}
	bool Update() override
	{
		const double Now=FPlatformTime::Seconds();
		if (Now<Next) return false;
		UWorld* World=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		if (!World) return false;
		if (!Env)
		{
			for (TActorIterator<ACitySandboxEnvironment> It(World);It;++It) { Env=*It;break; }
			if (!Env) { Test->AddError(TEXT("Reflection environment missing"));return true; }
			for (TActorIterator<ACityVRToolController> It(World);It;++It) { It->ReturnToCity();break; }
			World->GetSubsystem<UCitySimulationSubsystem>()->SetEntryMenuSuspended(false);
			World->GetSubsystem<UCitySimulationSubsystem>()->SetSimulationPaused(true);
			Env->SetLightingPreset(ECityLightingPreset::Day);
			Test->TestTrue(TEXT("Windows live reflection enabled"),Env->AreDynamicReflectionsEnabled());
			ACityBoardActor* Board=nullptr;
			for (TActorIterator<ACityBoardActor> It(World);It;++It) { Board=*It;break; }
			const FVector Base=(Board ? Board->GetActorLocation() : FVector::ZeroVector)+FVector(0,0,30);
			auto Spawn=[&](const TCHAR* MeshPath,FVector Position,FVector Scale,UMaterialInterface* Material)
			{
				auto* A=World->SpawnActor<AStaticMeshActor>();
				A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				A->SetActorLocation(Base+Position);
				A->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPath));
				A->SetActorScale3D(Scale);A->GetStaticMeshComponent()->SetMaterial(0,Material);Actors.Add(A);return A;
			};
			auto* Color=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CitySim/Art/Materials/M_CityColor.M_CityColor"));
			auto* Red=UMaterialInstanceDynamic::Create(Color,Env);Red->SetVectorParameterValue(TEXT("CityColor"),FLinearColor(.9,.012,.005));
			auto* Green=UMaterialInstanceDynamic::Create(Color,Env);Green->SetVectorParameterValue(TEXT("CityColor"),FLinearColor(.005,.85,.018));
			Panel=Spawn(TEXT("/Engine/BasicShapes/Plane.Plane"),FVector::ZeroVector,FVector(6,6,1),LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CitySim/Art/LowDensity/Materials/M_Factory_Corrugated.M_Factory_Corrugated")));
			RedActor=Spawn(TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(-120,180,100),FVector(1),Red);
			Spawn(TEXT("/Engine/BasicShapes/Sphere.Sphere"),FVector(120,180,100),FVector(1.5),Green);
			auto* Camera=World->SpawnActor<ASceneCapture2D>();Actors.Add(Camera);
			Camera->SetActorLocation(Base+FVector(0,-900,550));Camera->SetActorRotation((Base+FVector(0,0,20)-Camera->GetActorLocation()).Rotation());
			Capture=Camera->GetCaptureComponent2D();Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->FOVAngle=50;
			Capture->CaptureSource=SCS_FinalColorLDR;Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;Capture->ShowOnlyComponent(Panel->GetStaticMeshComponent());
			Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
			Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
			Target=NewObject<UTextureRenderTarget2D>(Camera);Target->InitCustomFormat(800,800,PF_B8G8R8A8,false);Capture->TextureTarget=Target;
			Directory=FPaths::ProjectSavedDir()/TEXT("Automation/DynamicReflections");IFileManager::Get().MakeDirectory(*Directory,true);
			Next=Now+4;return false;
		}
		if (Phase==0)
		{
			if (const FTexture* Sky=Env->SkyLight->GetProcessedSkyTexture())
			{
				for (int32 Face=0;Face<6;++Face)
				{
					TArray<FColor> Pixels;const FTextureRHIRef Tex=Sky->TextureRHI;
					ENQUEUE_RENDER_COMMAND(CityReflectionDebugRead)([Tex,Face,&Pixels](FRHICommandListImmediate& Cmd){FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetCubeFace(static_cast<ECubeFace>(Face));Cmd.ReadSurfaceData(Tex,FIntRect(0,0,64,64),Pixels,Flags);});FlushRenderingCommands();
					TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(64,64,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("SkyCube%d.png"),Face)));
				}
			}
			Before=Read(TEXT("LiveRedGreen"));Test->TestTrue(TEXT("Actual metallic material reflects camera-hidden red cue"),Before>100);
			CaptureCount=Env->DynamicReflectionCaptureCount;Env->bDynamicReflectionsEnabled=false;
			RedActor->GetStaticMeshComponent()->SetVisibility(false);Next=Now+1;Phase=1;return false;
		}
		if (Phase==1)
		{
			const int32 Stale=Read(TEXT("RedRemoved_Stale"));Test->TestTrue(TEXT("Old cube retains removed cue until refreshed"),Stale>Before*.7);
			Env->bDynamicReflectionsEnabled=true;Env->NextReflectionCaptureSeconds=0;Next=Now+2;Phase=2;return false;
		}
		if (Phase==2)
		{
			After=Read(TEXT("RedRemoved_Updated"));Test->TestTrue(TEXT("Recurring capture removes obsolete reflected geometry"),After<Before*.2);
			Test->TestTrue(TEXT("Paused city still updates local reflections"),Env->DynamicReflectionCaptureCount>CaptureCount);
			const FString Report=FString::Printf(TEXT("{\"before_red_pixels\":%d,\"after_red_pixels\":%d,\"capture_count\":%llu,\"resolution\":64,\"cadence_seconds\":0.5,\"camera_hidden_cue\":true,\"actual_factory_material\":true,\"gpu_only\":true}\n"),Before,After,Env->DynamicReflectionCaptureCount);
			FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("Proof.json")));
			for (auto* A:Actors) if (A) A->Destroy();
			const FString Fixture=Directory/TEXT("FixtureInput.json");
			FString Error;
			if (!World->GetSubsystem<UCitySimulationSubsystem>()->LoadCityFromPath(Fixture,Error,false)) { Test->AddError(Error);return true; }
			World->GetSubsystem<UCitySimulationSubsystem>()->SetEntryMenuSuspended(false);
			World->GetSubsystem<UCitySimulationSubsystem>()->SetSimulationPaused(true);
			Env->bDynamicReflectionsEnabled=false;Phase=3;Warm=Now+2;End=Now+8;Next=0;Last=Now;return false;
		}
		if (Phase==3 || Phase==4)
		{
			if (Now>=Warm)
			{
				(Phase==3 ? OffFrames : OnFrames).Add((Now-Last)*1000);
				(Phase==3 ? OffGPU : OnGPU).Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
			}
			Last=Now;
			if (Now<End) return false;
			if (Phase==3) { Env->bDynamicReflectionsEnabled=true;Phase=4;Warm=Now+2;End=Now+8;return false; }
			auto Summary=[](TArray<double> Values)
			{
				if (Values.IsEmpty()) return FString(TEXT("null"));
				double Sum=0;for (double V:Values) Sum+=V;Values.Sort();
				return FString::Printf(TEXT("{\"samples\":%d,\"mean_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f,\"maximum_ms\":%.4f}"),Values.Num(),Sum/Values.Num(),Values[FMath::Min(Values.Num()-1,FMath::FloorToInt(Values.Num()*.95))],Values[FMath::Min(Values.Num()-1,FMath::FloorToInt(Values.Num()*.99))],Values.Last());
			};
			const FString Report=TEXT("{\"scene\":\"12 actual low-density buildings, paused city\",\"off_frame\":")+Summary(OffFrames)+TEXT(",\"on_frame\":")+Summary(OnFrames)+TEXT(",\"off_gpu\":")+Summary(OffGPU)+TEXT(",\"on_gpu\":")+Summary(OnGPU)+TEXT(",\"headset_measured\":false}\n");
			FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("BoundedCost.json")));
			UE_LOG(LogTemp,Display,TEXT("CITYSIM_DYNAMIC_REFLECTION_PROOF_COMPLETE"));return true;
		}
		return true;
	}
private:
	int32 Read(const TCHAR* Name)
	{
		Capture->CaptureScene();TArray<FColor> Pixels;auto* Resource=Target->GameThread_GetRenderTargetResource();
		if (!Resource || !Resource->ReadPixels(Pixels)) { Test->AddError(TEXT("Metal render readback failed"));return 0; }
		TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(800,800,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Directory/(FString(Name)+TEXT(".png"))));
		int32 Count=0;for (const auto& P:Pixels) if (P.R>55 && P.R>P.G*1.6 && P.R>P.B*1.6) ++Count;return Count;
	}
	FAutomationTestBase* Test;ACitySandboxEnvironment* Env=nullptr;AStaticMeshActor* Panel=nullptr;AStaticMeshActor* RedActor=nullptr;
	USceneCaptureComponent2D* Capture=nullptr;UTextureRenderTarget2D* Target=nullptr;TArray<AActor*> Actors;FString Directory;
	double Next=0;int32 Phase=0,Before=0,After=0;uint64 CaptureCount=0;
	double Warm=0,End=0,Last=0;TArray<double> OffFrames,OnFrames,OffGPU,OnGPU;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDynamicReflectionTest,"CitySim.Desktop.DynamicReflections",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDynamicReflectionTest::RunTest(const FString&)
{
	const FString Profile=FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir()).Replace(TEXT("\\"),TEXT("/"));
	if (!Profile.Contains(TEXT("/LowDensity/DynamicReflections/"))) { AddError(TEXT("Use an isolated LowDensity/DynamicReflections profile"));return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FCityDynamicReflectionCommand(this));return true;
}
#endif
