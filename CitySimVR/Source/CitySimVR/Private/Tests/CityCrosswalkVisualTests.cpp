#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityPedestrianPaths.h"
#include "Persistence/CityPersistence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

// Controlled trip through production scheduling/presentation, in a CI-only city/profile.
class FCityCrosswalkCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FCityCrosswalkCaptureCommand(FAutomationTestBase* In,bool Continuity=false):bContinuity(Continuity),Test(In){}
	bool Update() override
	{
		if(FPlatformTime::Seconds()<Next) return false;
		auto* World=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
		ACityVRToolController* UI=nullptr;
		if(World) for(TActorIterator<ACityVRToolController> It(World);It;++It){UI=*It;break;}
		if(!UI || !UI->Board || !UI->DesktopHud || !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn())
		{
			if(++Attempts<120){Next=FPlatformTime::Seconds()+.5;return false;}
			Test->AddError(TEXT("Crosswalk desktop viewport did not initialize"));return true;
		}
		auto* Board=UI->Board.Get();auto* Sim=World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Player=World->GetFirstPlayerController();
		const FString Directory=FPaths::ProjectSavedDir()/(bContinuity?TEXT("Automation/TrafficPlacementContinuity"):TEXT("Automation/Crosswalks"));
		IFileManager::Get().MakeDirectory(*Directory,true);
		FString Error;
		if(Phase==0)
		{
			UI->bDesktopFocused=true;UI->LifecycleInputGate.Update(false);UI->ReturnToCity();
			UI->SetObjectivesVisible(false);UI->SetCitySummaryVisible(false);UI->SetPaletteOpen(false);
			FCitySaveDocument Doc;Doc.bSimulationPaused=true;
			Doc.Terrain.BroadElevationAmplitudeCm=Doc.Terrain.HillAmplitudeCm=Doc.Terrain.RidgeAmplitudeCm=0;
			FCityTerrainHeightfield Terrain;Terrain.Generate(Doc.Terrain);
			FCityPlacementModel City(100000);City.SetTerrain(&Terrain);
			auto Road=City.BeginRoadPreview(City.GetEntranceCell());City.UpdateRoadPreviewEnd(Road,{20,8});
			if(!City.CommitRoad(Road).IsSuccess()){Test->AddError(TEXT("Crosswalk fixture street failed"));return true;}
			Road=City.BeginRoadPreview({12,8},TEXT("Avenue"));City.UpdateRoadPreviewEnd(Road,{20,8});
			if(!City.CommitRoad(Road).IsSuccess()){Test->AddError(TEXT("Crosswalk fixture avenue failed"));return true;}
			Road=City.BeginRoadPreview({12,8});City.UpdateRoadPreviewEnd(Road,{12,18});
			if(!City.CommitRoad(Road).IsSuccess()){Test->AddError(TEXT("Crosswalk fixture junction failed"));return true;}
			auto Place=[&](FName Type,FCityCellCoord Cell,int32 Turn)
			{
				const auto* D=Board->BuildingCatalog->FindDefinition(Type);
				if(!D) return uint64(0);
				auto Preview=City.BeginPreview(*D,Cell,Turn);const auto R=City.CommitPlacement(Preview,*D);
				if(!R.IsSuccess()) return uint64(0);
				FCityDevelopmentState State;State.EntityId=R.EntityId;State.Stage=ECityDevelopmentStage::Operational;
				Doc.Development.States.Add(R.EntityId,State);return R.EntityId;
			};
			Home=Place(TEXT("Home"),{6,9},0);Shop=Place(TEXT("Shop"),{6,6},2);
			const uint64 Park=Place(TEXT("Park"),{16,9},0);
			if(!Home || !Shop || !Park){Test->AddError(TEXT("Crosswalk fixture entrances failed"));return true;}
			Doc.Placement=City.ExportState();FString Json;const FString Path=Directory/TEXT("Fixture.json");
			if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Path,Json,Error) || !Sim->LoadCityFromPath(Path,Error,false))
			{Test->AddError(Error);return true;}
			Sim->SetEntryMenuSuspended(false);Sim->SetSimulationPaused(true);Board->PrepareCityView();
			Board->SetToolMode(ECityToolMode::Inspect);Board->SetLightingPreset(ECityLightingPreset::Day);
			FCityPopulationSnapshot Population;FCityCitizenRecord C;C.Id=11;C.HomeEntityId=C.CurrentLocationEntityId=Home;
			C.TripDestinationEntityId=Shop;C.TripPurpose=ECityTripPurpose::Purchase;C.TripVehicleType=ECityTripVehicleType::Walk;Population.Citizens.Add(C);
			Sim->Traffic.Reset();Sim->Traffic.UpdateTrips(*Sim->PlacementModel,Board->BuildingCatalog,Population,Sim->RoadNetworkSnapshot,0);
			if(!Test->TestEqual(TEXT("Controlled cooked trip schedules"),Sim->Traffic.GetSnapshot().Vehicles.Num(),1)) return true;
			Initial=Sim->Traffic.GetSnapshot().Vehicles[0];
			Test->TestTrue(TEXT("Both actual authored entrance paths bind"),Board->CitizenEntrancePaths.Contains(Home) && Board->CitizenEntrancePaths.Contains(Shop));
			FVector Position,Direction;
			if(Board->CitizenEntrancePaths.Contains(Home))
			{
				Test->TestTrue(TEXT("Actual door start samples"),Board->SampleEntranceAwareWalker(Initial,nullptr,1.,Position,Direction));
				Test->TestTrue(TEXT("Walker starts at authored doorway XY"),FVector::Dist2D(Position,Board->CitizenEntrancePaths[Home][0])<.1);
			}
			Player->GetPawn()->SetActorTickEnabled(false);
			if(auto* Camera=Player->GetPawn()->FindComponentByClass<UCameraComponent>()) Camera->SetFieldOfView(45);
			Aim(Player,Board,FVector(5600,6800,100),FVector(900,-1800,2400));
			if(bContinuity)
            {
                FCityTrafficVehicleState Car;Car.Id=900;
                if(!FCityTrafficSimulation::BuildRoute(Sim->RoadNetworkSnapshot,{2,8},{18,8},Car) ||
                    !Sim->Traffic.AddVehicle(Car,Sim->RoadNetworkSnapshot))
                {Test->AddError(TEXT("Continuity fixture car failed"));return true;}
                Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot,1);
                Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot,2);
                Sim->PreviousTraffic=Sim->Traffic.GetSnapshot();Board->UpdatePopulationTraffic();
                ContinuityState=Sim->Traffic.GetSnapshot();
                Board->TrafficVehicleInstances->GetInstanceTransform(0,ContinuityCarTransform);
                for(const auto& V:ContinuityState.Vehicles)
                {
                    FVector P,D;FCityTrafficSimulation::SamplePosition(V,nullptr,Sim->RoadNetworkSnapshot,1.,P,D);
                    ContinuityPositions.Add(V.Id,P);
                }
                Test->TestEqual(TEXT("Moving continuity fixture has car and walker"),ContinuityState.Vehicles.Num(),2);
                Aim(Player,Board,FVector(8000,6800,40),FVector(0,-5000,10000));
            }
            Phase=1;Next=FPlatformTime::Seconds()+2.;return false;
		}
		if(bContinuity) return UpdateContinuity(Board,Sim,Player,Directory);
		if(Phase<=8)
		{
			if(Phase%2==1)
			{
				const int32 Sample=(Phase-1)/2;
				if(Sample<3)
				{
					auto State=Initial;State.WalkDistanceUnits=FMath::RoundToInt(CityPedestrianPaths::Length(State.WalkPath)*(Sample==0?0.:Sample==1?.5:.94));
					FCityTrafficSnapshot Traffic;Traffic.NetworkFingerprint=FCityTrafficSimulation::GetNetworkFingerprint(Sim->RoadNetworkSnapshot);Traffic.Vehicles.Add(State);
					if(!Sim->Traffic.RestoreState(Traffic,Sim->RoadNetworkSnapshot,0,Error)){Test->AddError(Error);return true;}
					Sim->PreviousTraffic=Sim->Traffic.GetSnapshot();Board->UpdatePopulationTraffic();
					if(Sample>0) Aim(Player,Board,FVector(Sample==1?9600:5400,6800,40),FVector(0,0,6000));
				}
				else Aim(Player,Board,FVector(11200,6800,40),FVector(0,-1800,6000));
			}
			else FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Crosswalk-%02d.png"),Phase/2),true,false);
			++Phase;Next=FPlatformTime::Seconds()+1.;return false;
		}
		Player->GetPawn()->SetActorTickEnabled(true);
		UE_LOG(LogTemp,Display,TEXT("CITYSIM_CROSSWALKS_CAPTURE_COMPLETE Captures=4 ControlledTrip=1 ManualAcceptancePending=1"));
		return true;
	}
private:
    bool UpdateContinuity(ACityBoardActor* Board,UCitySimulationSubsystem* Sim,APlayerController* Player,const FString& Directory)
    {
        if(Phase==1 || Phase==3 || Phase==5 || Phase==7)
        {
            const TCHAR* Name=Phase==1?TEXT("Before"):Phase==3?TEXT("Building"):Phase==5?TEXT("Road"):TEXT("Continued");
            FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Continuity-%s.png"),Name),true,false);
            ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
        }
        if(Phase==2 || Phase==4)
        {
            if(Phase==2)
            {
                const auto* D=Board->BuildingCatalog->FindDefinition(TEXT("Home"));
                if(!D) {Test->AddError(TEXT("Continuity Home definition missing"));return true;}
                auto Preview=Sim->PlacementModel->BeginPreview(*D,{16,6},2);
                if(!Sim->PlacementModel->CommitPlacement(Preview,*D).IsSuccess())
                {Test->AddError(TEXT("Continuity unrelated building placement failed"));return true;}
            }
            else
            {
                auto Road=Sim->PlacementModel->BeginRoadPreview({20,8});Sim->PlacementModel->UpdateRoadPreviewEnd(Road,{25,8});
                if(!Sim->PlacementModel->CommitRoad(Road).IsSuccess())
                {Test->AddError(TEXT("Continuity road extension failed"));return true;}
            }
            Sim->RefreshRoadNetwork();Board->RebuildAllPresentation();Board->UpdatePopulationTraffic();
            const auto& Current=Sim->Traffic.GetSnapshot();
            Test->TestEqual(TEXT("Placement retains active car and pedestrian count"),Current.Vehicles.Num(),ContinuityState.Vehicles.Num());
            for(const auto& Before:ContinuityState.Vehicles)
            {
                const auto* V=Current.Vehicles.FindByPredicate([&](const auto& Item){return Item.Id==Before.Id;});
                if(!Test->TestNotNull(TEXT("Placement retains exact agent ID"),V)) continue;
                FVector P,D;Test->TestTrue(TEXT("Placement samples retained agent"),FCityTrafficSimulation::SamplePosition(*V,nullptr,Sim->RoadNetworkSnapshot,1.,P,D));
                Test->TestTrue(TEXT("Placement retains exact visible route position"),P.Equals(ContinuityPositions.FindChecked(V->Id),1.e-8));
                Test->TestEqual(TEXT("Placement retains pedestrian distance"),V->WalkDistanceUnits,Before.WalkDistanceUnits);
                Test->TestEqual(TEXT("Placement retains source identity"),V->SourceId,Before.SourceId);
                Test->TestEqual(TEXT("Placement retains completion state"),V->bCompleted,Before.bCompleted);
            }
            Test->TestEqual(TEXT("Car proxy stays visible"),Board->TrafficVehicleInstances->GetInstanceCount(),1);
            FTransform CarTransform;Board->TrafficVehicleInstances->GetInstanceTransform(0,CarTransform);
            Test->TestTrue(TEXT("Actual pooled car transform survives placement"),CarTransform.Equals(ContinuityCarTransform,.01));
            const int32 Walkers=Board->CitizenInstances->GetInstanceCount()+Board->OfficeCitizenInstances->GetInstanceCount()+Board->WorkerCitizenInstances->GetInstanceCount();
            int32 Animated=0;for(const auto& Walker:Board->StreetAnimatedWalkers) if(Walker && Walker->IsVisible()) ++Animated;
            Test->TestEqual(TEXT("Pedestrian proxy stays visible"),Walkers+Animated,1);
            ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
        }
        if(Phase==6)
        {
            Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot,3);Sim->PreviousTraffic=Sim->Traffic.GetSnapshot();Board->UpdatePopulationTraffic();
            for(const auto& V:Sim->Traffic.GetSnapshot().Vehicles)
            {
                FVector P,D;FCityTrafficSimulation::SamplePosition(V,nullptr,Sim->RoadNetworkSnapshot,1.,P,D);
                Test->TestTrue(TEXT("Retained traffic continues moving after placements"),!P.Equals(ContinuityPositions.FindChecked(V.Id),.001));
            }
            ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
        }
        Player->GetPawn()->SetActorTickEnabled(true);
        UE_LOG(LogTemp,Display,TEXT("CITYSIM_TRAFFIC_CONTINUITY_CAPTURE_COMPLETE Captures=4 ControlledMovingAgents=2 ManualAcceptancePending=1"));
        return true;
    }
    bool bContinuity=false;
    FCityTrafficSnapshot ContinuityState;
    FTransform ContinuityCarTransform;
    TMap<uint64,FVector> ContinuityPositions;
	void Aim(APlayerController* Player,ACityBoardActor* Board,const FVector& Point,const FVector& Offset)
	{
		const auto Pose=Board->GetActorTransform();const auto Target=Pose.TransformPosition(Point);
		const auto Location=Target+Pose.TransformVector(Offset);Player->GetPawn()->SetActorLocation(Location);Player->SetControlRotation((Target-Location).Rotation());
	}
	FAutomationTestBase* Test;FCityTrafficVehicleState Initial;uint64 Home=0,Shop=0;int32 Phase=0,Attempts=0;double Next=0.;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCrosswalkVisualTest,"CitySim.Desktop.Crosswalks",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityCrosswalkVisualTest::RunTest(const FString&)
{
	FString UserDir;
	if(!FParse::Param(FCommandLine::Get(),TEXT("CityDesktop")) || !FParse::Param(FCommandLine::Get(),TEXT("CityPackagedAutomation")) ||
	    !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("Client-")))
	{AddError(TEXT("Crosswalk review requires a CI-owned packaged client profile"));return false;}
	ADD_LATENT_AUTOMATION_COMMAND(FCityCrosswalkCaptureCommand(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficPlacementVisualTest,"CitySim.Desktop.TrafficPlacementContinuity",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityTrafficPlacementVisualTest::RunTest(const FString&)
{
    FString UserDir;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityDesktop")) || !FParse::Param(FCommandLine::Get(),TEXT("CityPackagedAutomation")) ||
        !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("Client-")))
    {AddError(TEXT("Traffic continuity review requires a CI-owned packaged client profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityCrosswalkCaptureCommand(this,true));return true;
}
#endif
