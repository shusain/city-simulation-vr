#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Interaction/CityVRToolController.h"
#include "Sandbox/CitySandboxEnvironment.h"

class FCityCitizenWalkVisualCommand : public IAutomationLatentCommand
{
public:
	explicit FCityCitizenWalkVisualCommand(FAutomationTestBase* In):Test(In){}
	bool Update() override
	{
		if(FPlatformTime::Seconds()<Next)return false;
		auto* World=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		if(!World)return false;
		if(!Board)
		{
			for(TActorIterator<ACityBoardActor> It(World);It;++It){Board=*It;break;}
			if(!Board){Test->AddError(TEXT("Flow board missing"));return true;}
			for(TActorIterator<ACityVRToolController> It(World);It;++It)It->ReturnToCity();
			for(TActorIterator<ACitySandboxEnvironment> It(World);It;++It)It->SetLightingPreset(ECityLightingPreset::Day);
			Sim=World->GetSubsystem<UCitySimulationSubsystem>();Sim->SetSimulationPaused(true);
			Directory=FPaths::ProjectSavedDir()/TEXT("Automation/CitizenWalk");IFileManager::Get().MakeDirectory(*Directory,true);
			auto* Camera=World->SpawnActor<ASceneCapture2D>();Capture=Camera->GetCaptureComponent2D();
			Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->FOVAngle=40;
			Capture->bOverride_CustomNearClippingPlane=true;Capture->CustomNearClippingPlane=.1f;
			Capture->CaptureSource=SCS_FinalColorLDR;Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
			Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
			Target=NewObject<UTextureRenderTarget2D>(Camera);Target->InitCustomFormat(1280,720,PF_B8G8R8A8,false);Capture->TextureTarget=Target;
			TArray<UPrimitiveComponent*> Components;Board->GetComponents(Components);
			for(auto* Component:Components)Capture->ShowOnlyComponent(Component);
			Next=FPlatformTime::Seconds()+2;return false;
		}
		const int32 Scenario=Phase/7, Frame=Phase%7;
		if(Frame==0 && !bPrepared)
		{
			FCitySaveDocument Doc;Doc.bSimulationPaused=true;
			Doc.Terrain.BroadElevationAmplitudeCm=Doc.Terrain.HillAmplitudeCm=Doc.Terrain.RidgeAmplitudeCm=0;
			FCityPlacementModel City(100000);
			auto Road=City.BeginRoadPreview({0,8},FName(TEXT("LocalRoad")));
			City.UpdateRoadPreviewEnd(Road,{20,8});Test->TestTrue(TEXT("visual fixture road"),City.CommitRoad(Road).IsSuccess());
			auto Branch=City.BeginRoadPreview({10,8});City.UpdateRoadPreviewEnd(Branch,{10,18});Test->TestTrue(TEXT("corner road"),City.CommitRoad(Branch).IsSuccess());
			Doc.Placement=City.ExportState();FString Json,Error;
			if(!FCityPersistence::Serialize(Doc,Json,Error)||!FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("Fixture.json")))||
			   !Sim->LoadCityFromPath(Directory/TEXT("Fixture.json"),Error,false)){Test->AddError(Error);return true;}
			Sim->Traffic.Reset();
			auto Vehicle=[&](uint64 Id,int32 Start,bool Walk=false,int32 Track=0){
				FCityTrafficVehicleState V;V.Id=Id;V.VehicleType=Walk?ECityTripVehicleType::Walk:ECityTripVehicleType::Car;
				V.LengthUnits=Walk?80:350;V.PedestrianTrack=Track;
				FCityTrafficSimulation::BuildRoute(Sim->GetRoadNetworkSnapshot(),{0,8},{20,8},V);V.DistanceUnits=Start;return V;};
			for(int32 I=0;I<3;++I)
			{
				auto V=Vehicle(I+1,Scenario==2 ? 9700-I*200 : 6000+I*200,true,I%2);
				if(Scenario>0)
				{
					V.Route.Reset();V.RouteLegIndex=0;
					FCityTrafficSimulation::BuildRoute(Sim->RoadNetworkSnapshot,Scenario==1 ? FCityCellCoord(20,8) : FCityCellCoord(0,8),Scenario==1 ? FCityCellCoord(0,8) : FCityCellCoord(10,18),V);
					V.DistanceUnits=Scenario==2 ? 9700-I*200 : 6000+I*200;
				}
				Test->TestTrue(TEXT("real walker fixture admitted"),Sim->Traffic.AddVehicle(V,Sim->RoadNetworkSnapshot));
			}

			Before=Sim->Traffic.GetSnapshot();Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot,1);After=Sim->Traffic.GetSnapshot();
			bPrepared=true;Next=FPlatformTime::Seconds()+1;return false;
		}
		Sim->PreviousTraffic=Before;FString Error;
		Test->TestTrue(TEXT("visual moving state restores safely"),Sim->Traffic.RestoreState(After,Sim->RoadNetworkSnapshot,1,Error));
		const double Alpha=.10+Frame*.03;
		Sim->Economy.RestoreState(Sim->Economy.GetSnapshot(),Alpha*Sim->EconomyTuning.FixedStepSeconds,true,1,Sim->EconomyTuning,Error);
		Sim->SetSimulationPaused(false);Board->UpdatePopulationTraffic();Sim->SetSimulationPaused(true);
		int32 Walkers=0;USkeletalMeshComponent* Hero=nullptr;
		for(auto Component:Board->StreetAnimatedWalkers)if(Component->IsVisible()){++Walkers;if(!Hero)Hero=Component;Capture->ShowOnlyComponent(Component);}
		Test->TestEqual(TEXT("three outfits use actual pooled skeletal citizens"),Walkers,3);
		if(!Hero){Test->AddError(TEXT("animated street hero missing"));return true;}
		Test->TestEqual(TEXT("no static duplicate walkers"),Board->CitizenInstances->GetInstanceCount()+Board->OfficeCitizenInstances->GetInstanceCount()+Board->WorkerCitizenInstances->GetInstanceCount(),0);
		Capture->FOVAngle=40;
		const FVector LocalTarget=Hero->GetRelativeLocation()+FVector(0,0,65);

		const auto Transform=Board->GetActorTransform();const FVector Aim=Transform.TransformPosition(LocalTarget);
		const FVector CameraPosition=Aim+Transform.TransformVector(FVector(80,-480,230));
		Capture->GetOwner()->SetActorLocation(CameraPosition);Capture->GetOwner()->SetActorRotation((Aim-CameraPosition).Rotation());
		Capture->CaptureScene();TArray<FColor> Pixels;
		if(!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)){Test->AddError(TEXT("Flow render readback failed"));return true;}
		int32 VisiblePixels=0;for(const auto& Pixel:Pixels)if(Pixel.R>30 || Pixel.G>30 || Pixel.B>30)++VisiblePixels;
		Test->TestTrue(TEXT("movement capture contains rendered scene"),VisiblePixels>10000);
		const TCHAR* Names[]={TEXT("HomeToShop"),TEXT("ReturnHome"),TEXT("CornerToWork")};
		TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);
		Test->TestTrue(TEXT("flow capture saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("%s-%d.png"),Names[Scenario],Frame))));
		if(Frame==6)bPrepared=false;
		if(++Phase>=21){Test->AddInfo(TEXT("CITYSIM_CITIZENWALK_WALK_VISUAL_COMPLETE"));return true;}
		Next=FPlatformTime::Seconds()+.3;return false;
	}
private:
	FAutomationTestBase* Test;ACityBoardActor* Board=nullptr;UCitySimulationSubsystem* Sim=nullptr;
	USceneCaptureComponent2D* Capture=nullptr;UTextureRenderTarget2D* Target=nullptr;
	FCityTrafficSnapshot Before,After;FString Directory;int32 Phase=0;double Next=0;bool bPrepared=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenWalkVisualTest,"CitySim.Desktop.CitizenWalkVisual",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityCitizenWalkVisualTest::RunTest(const FString& Parameters)
{
	FString User;
	if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User) ||
	   !(User.Contains(TEXT("CitizenWalk")) || User.Contains(TEXT("CitizenWalk")) || FParse::Param(FCommandLine::Get(),TEXT("CityPackagedAutomation"))))
	{AddError(TEXT("Requires an isolated citizen-walk or CI automation profile"));return false;}
	ADD_LATENT_AUTOMATION_COMMAND(FCityCitizenWalkVisualCommand(this));return true;
}
#endif
