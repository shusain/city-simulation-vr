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

class FCityFlowVisualCommand : public IAutomationLatentCommand
{
public:
	explicit FCityFlowVisualCommand(FAutomationTestBase* In):Test(In){}
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
			Directory=FPaths::ProjectSavedDir()/TEXT("Automation/Flow");IFileManager::Get().MakeDirectory(*Directory,true);
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
		const int32 Scenario=Phase/3, Frame=Phase%3;
		if(Frame==0 && !bPrepared)
		{
			FCitySaveDocument Doc;Doc.bSimulationPaused=true;
			Doc.Terrain.BroadElevationAmplitudeCm=Doc.Terrain.HillAmplitudeCm=Doc.Terrain.RidgeAmplitudeCm=0;
			FCityPlacementModel City(100000);
			auto Road=City.BeginRoadPreview({0,8},Scenario==3 ? FName(TEXT("Highway")) : Scenario==4 ? FName(TEXT("Avenue")) : FName(TEXT("LocalRoad")));
			City.UpdateRoadPreviewEnd(Road,{20,8});Test->TestTrue(TEXT("visual fixture road"),City.CommitRoad(Road).IsSuccess());
			Doc.Placement=City.ExportState();FString Json,Error;
			if(!FCityPersistence::Serialize(Doc,Json,Error)||!FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("Fixture.json")))||
			   !Sim->LoadCityFromPath(Directory/TEXT("Fixture.json"),Error,false)){Test->AddError(Error);return true;}
			Sim->Traffic.Reset();
			auto Vehicle=[&](uint64 Id,int32 Start,bool Walk=false,int32 Track=0){
				FCityTrafficVehicleState V;V.Id=Id;V.VehicleType=Walk?ECityTripVehicleType::Walk:ECityTripVehicleType::Car;
				V.LengthUnits=Walk?80:350;V.PedestrianTrack=Track;
				FCityTrafficSimulation::BuildRoute(Sim->GetRoadNetworkSnapshot(),{0,8},{20,8},V);V.DistanceUnits=Start;return V;};
			if(Scenario==0)
			{
				auto Car=Vehicle(1,6000);auto E=Vehicle(99,4000);E.VehicleType=ECityTripVehicleType::FireEngine;E.bEmergencyResponding=true;E.LengthUnits=550;
				Sim->Traffic.AddVehicle(Car,Sim->RoadNetworkSnapshot);Sim->Traffic.AddVehicle(E,Sim->RoadNetworkSnapshot);
			}
			else if(Scenario==1){auto Car=Vehicle(1,6000);Car.bYielding=true;Sim->Traffic.AddVehicle(Car,Sim->RoadNetworkSnapshot);}
			else if(Scenario==2 || Scenario==4)
			{
				for(auto V : {Vehicle(1,6000,true),Vehicle(2,5800,true),Vehicle(3,6150,true),Vehicle(4,6500,true,1)})Sim->Traffic.AddVehicle(V,Sim->RoadNetworkSnapshot);
			}
			else
			{
				auto Car=Vehicle(2,6000);Car.SpeedUnitsPerStep=2000;
				auto E=Vehicle(98,4000);E.VehicleType=ECityTripVehicleType::FireEngine;E.bEmergencyResponding=true;E.LengthUnits=550;E.Route[0].LaneIndex=0;
				Sim->Traffic.AddVehicle(Car,Sim->RoadNetworkSnapshot);Sim->Traffic.AddVehicle(E,Sim->RoadNetworkSnapshot);
			}
			Before=Sim->Traffic.GetSnapshot();Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot,1);After=Sim->Traffic.GetSnapshot();
			bPrepared=true;Next=FPlatformTime::Seconds()+1;return false;
		}
		Sim->PreviousTraffic=Before;FString Error;
		Test->TestTrue(TEXT("visual moving state restores safely"),Sim->Traffic.RestoreState(After,Sim->RoadNetworkSnapshot,1,Error));
		const double Alpha=Frame==0?0.0:Frame==1?.5:.9999;
		Sim->Economy.RestoreState(Sim->Economy.GetSnapshot(),Alpha*Sim->EconomyTuning.FixedStepSeconds,true,1,Sim->EconomyTuning,Error);
		Board->UpdatePopulationTraffic();
		const bool Walk=Scenario==2 || Scenario==4;
		if(Walk)
		{
			int32 WalkerCount=Board->CitizenInstances->GetInstanceCount()+Board->OfficeCitizenInstances->GetInstanceCount()+Board->WorkerCitizenInstances->GetInstanceCount();
			for(auto Component:Board->StreetAnimatedWalkers)
				if(Component->IsVisible()){++WalkerCount;Capture->ShowOnlyComponent(Component);}
			Test->TestEqual(TEXT("all four walking meshes are presented exactly once"),WalkerCount,4);
		}
		else if(Scenario==0 || Scenario==3)
			Test->TestEqual(TEXT("responder is actually presented"),Board->FireResponseInstances->GetInstanceCount(),1);
		Capture->FOVAngle=Walk?40:60;
		const FVector LocalTarget((Walk?6.7:7.0)*800,(Walk?8.9:8.5)*800,100);
		const auto Transform=Board->GetActorTransform();const FVector Aim=Transform.TransformPosition(LocalTarget);
		const FVector CameraPosition=Aim+Transform.TransformVector(Walk?FVector(0,-1000,1300):FVector(0,-3500,3500));
		Capture->GetOwner()->SetActorLocation(CameraPosition);Capture->GetOwner()->SetActorRotation((Aim-CameraPosition).Rotation());
		Capture->CaptureScene();TArray<FColor> Pixels;
		if(!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)){Test->AddError(TEXT("Flow render readback failed"));return true;}
		int32 VisiblePixels=0;for(const auto& Pixel:Pixels)if(Pixel.R>30 || Pixel.G>30 || Pixel.B>30)++VisiblePixels;
		Test->TestTrue(TEXT("movement capture contains rendered scene"),VisiblePixels>10000);
		const TCHAR* Names[]={TEXT("LocalPullOver"),TEXT("LocalRejoin"),TEXT("PedestrianPass"),TEXT("HighwayRightLane"),TEXT("AvenuePassing")};
		TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);
		Test->TestTrue(TEXT("flow capture saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("%s-%d.png"),Names[Scenario],Frame))));
		bPrepared=false;
		if(++Phase>=15){Test->AddInfo(TEXT("CITYSIM_FLOW_VISUAL_COMPLETE"));return true;}
		Next=FPlatformTime::Seconds()+.3;return false;
	}
private:
	FAutomationTestBase* Test;ACityBoardActor* Board=nullptr;UCitySimulationSubsystem* Sim=nullptr;
	USceneCaptureComponent2D* Capture=nullptr;UTextureRenderTarget2D* Target=nullptr;
	FCityTrafficSnapshot Before,After;FString Directory;int32 Phase=0;double Next=0;bool bPrepared=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFlowVisualTest,"CitySim.Desktop.FlowVisual",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityFlowVisualTest::RunTest(const FString& Parameters)
{
	FString User;
	if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("Flow")))
	{AddError(TEXT("Requires isolated Flow profile"));return false;}
	ADD_LATENT_AUTOMATION_COMMAND(FCityFlowVisualCommand(this));return true;
}
#endif
