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
#include "Presentation/CityPrivateEntrances.h"
#include "ProceduralMeshComponent.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CitySimulationSubsystem.h"

class FCityApprovedCivicStudyCommand : public IAutomationLatentCommand
{
    struct FCase {const TCHAR* Key;const TCHAR* Id;const TCHAR* Mesh;int32 Level,Condition;};
    const TArray<FCase> Cases={
#include "CityApprovedCivicStudyCases.inl"
    };
public:
    explicit FCityApprovedCivicStudyCommand(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Next) return false;
        auto* World=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
        if(!World) {if(++Attempts<120) {Next=FPlatformTime::Seconds()+.5;return false;} Test->AddError(TEXT("Civic study world missing"));return true;}
        if(!Board)
        {
            for(TActorIterator<ACityBoardActor> It(World);It;++It) {Board=*It;break;}
            if(!Test->TestNotNull(TEXT("Real city board"),Board)) return true;
            for(TActorIterator<ACityVRToolController> It(World);It;++It) It->ReturnToCity();
            for(TActorIterator<ACitySandboxEnvironment> It(World);It;++It) It->SetLightingPreset(ECityLightingPreset::Day);
            Sim=World->GetSubsystem<UCitySimulationSubsystem>();
            FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Directory);Directory/=TEXT("ApprovedCivicBatch");
            IFileManager::Get().MakeDirectory(*Directory,true);
            OriginalTick=Board->IsActorTickEnabled();Board->SetActorTickEnabled(false);
            if(!Prepare()) return Finish();
            Camera=World->SpawnActor<ASceneCapture2D>();auto* Capture=Camera->GetCaptureComponent2D();
            Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=SCS_FinalColorLDR;Capture->FOVAngle=48;
            Capture->bOverride_CustomNearClippingPlane=true;Capture->CustomNearClippingPlane=.1f;
            Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
            auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->InitCustomFormat(1200,900,PF_B8G8R8A8,false);Capture->TextureTarget=Target;
            TInlineComponentArray<UPrimitiveComponent*> Components(Board);
            for(auto* Component:Components) if(Component->IsVisible()) Capture->ShowOnlyComponent(Component);
        }
        if(Stage==0)
        {
            const int32 Index=Case%Cases.Num();const bool Rear=Case>=Cases.Num();
            const auto& Entity=Sim->GetPlacementModel().GetEntities().FindChecked(Entities[Index]);
            const auto* Definition=Board->BuildingCatalog->FindDefinition(Entity.DefinitionId);
            const auto Presentation=Board->ResolvePresentationDefinition(Entity.EntityId,*Definition);
            const auto Pose=Board->MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise);
            const FVector Aim=Pose.GetLocation()+FVector(0,0,220);
            const double Span=FMath::Max(Entity.FootprintSize.X,Entity.FootprintSize.Y)*800.;
            const FVector Offset=Rear?FVector(-Span*.80,Span*.90,Span*.72):FVector(Span*.72,-Span*.95,Span*.64);
            const auto BoardPose=Board->GetActorTransform();
            const FVector WorldAim=BoardPose.TransformPosition(Aim),Position=BoardPose.TransformPosition(Aim+Offset);
            Camera->SetActorLocation(Position);Camera->SetActorRotation((WorldAim-Position).Rotation());
            if(Rear) Board->GetOrCreateInstances(Presentation)->SetForcedLodModel(3);
            Stage=1;Next=FPlatformTime::Seconds()+1.5;return false;
        }
        if(Stage==1) {Camera->GetCaptureComponent2D()->CaptureScene();Stage=2;Next=FPlatformTime::Seconds()+.5;return false;}
        TArray<FColor> Pixels;Camera->GetCaptureComponent2D()->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        if(!Test->TestEqual(TEXT("Native capture pixels"),Pixels.Num(),1200*900)) return Finish();
        int32 Visible=0;for(const auto& P:Pixels) if(P.R>25 || P.G>25 || P.B>25) ++Visible;
        Test->TestTrue(TEXT("Civic/terrain capture contains rendered geometry"),Visible>20000);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(1200,900,Pixels,Png);
        const FString File=FString::Printf(TEXT("%02d-%s-%s.png"),Case,Cases[Case%Cases.Num()].Key,Case>=Cases.Num()?TEXT("Rear-LOD2"):TEXT("Front"));
        Test->TestTrue(TEXT("Native review capture saved"),FFileHelper::SaveArrayToFile(Png,*(Directory/File)));
        if(++Case==Cases.Num()*2) {Test->AddInfo(TEXT("APPROVED_CIVIC_VISUAL_PASS 17 normal-catalog sites,34 front/rear-LOD2 captures,all15 versions,damage/construction,current save reload and road joins"));return Finish();}
        Stage=0;return false;
    }
private:
    bool Prepare()
    {
        auto Parameters=Sim->Terrain.GetParameters();Parameters.GeneratorVersion=Parameters.WaterRulesVersion=Parameters.SceneryVersion=0;
        Parameters.BroadElevationAmplitudeCm=200;Parameters.HillAmplitudeCm=300;Parameters.RidgeAmplitudeCm=150;
        Parameters.SmoothingPasses=2;Parameters.StarterDepthCells=Parameters.StarterHalfWidthCells=2;Parameters.SeaLevelCm=-2400;
        Sim->Terrain.Generate(Parameters);Sim->ResetCity(false);
        const auto ExpectedTerrain=Sim->Terrain.GetQuantizedHeightsCm();
        int32 Low=MAX_int32,High=MIN_int32;
        for(const int32 H:ExpectedTerrain) {Low=FMath::Min(Low,H);High=FMath::Max(High,H);}
        if(!Test->TestTrue(TEXT("Review terrain has actual seeded hills"),High-Low>200)) return false;
        Sim->RegisterBuildingCatalog(Board->BuildingCatalog);auto& City=Sim->GetPlacementModel();City.ApplyMonthlyEconomy(1000000,0);
        const auto ValidRoad=[&](FCityCellCoord A,FCityCellCoord B) {
            auto R=City.BeginRoadPreview(A);City.UpdateRoadPreviewEnd(R,B);return City.ValidateRoad(R).IsSuccess();
        };
        const auto Road=[&](FCityCellCoord A,FCityCellCoord B) {
            auto R=City.BeginRoadPreview(A);City.UpdateRoadPreviewEnd(R,B);
            const auto Result=City.CommitRoad(R);
            return Test->TestTrue(*FString::Printf(TEXT("Review road %d,%d to%d,%d respects terrain, failure%d"),A.X,A.Y,B.X,B.Y,static_cast<int32>(Result.Failure)),Result.IsSuccess());
        };
        TMap<FCityCellCoord,bool> Eligible;
        const auto CanRoad=[&](FCityCellCoord Cell) {
            if(Cell.X<0 || Cell.Y<0 || Cell.X>=Sim->Terrain.GetDimensions().GetWidth() || Cell.Y>=Sim->Terrain.GetDimensions().GetHeight()) return false;
            if(const bool* Found=Eligible.Find(Cell)) return *Found;
            const bool Valid=ValidRoad(Cell,Cell);Eligible.Add(Cell,Valid);return Valid;
        };
        // Route this isolated exhibition's streets around real grade/water
        // restrictions. Every node/edge uses normal road validation and paid
        // commands; no synthetic flat heights or terrain/road bypass.
        const auto Connect=[&](FCityCellCoord Target) {
            TArray<FCityCellCoord> Queue;TMap<FCityCellCoord,FCityCellCoord> Parents;
            for(const auto& Cell:City.GetRoadCells()) if(City.IsRoadConnected(Cell)) {Queue.Add(Cell);Parents.Add(Cell,Cell);}
            if(Queue.IsEmpty()) {Queue.Add(City.GetEntranceCell());Parents.Add(Queue[0],Queue[0]);}
            for(int32 Head=0;Head<Queue.Num() && !Parents.Contains(Target);++Head)
            {
                const auto A=Queue[Head];
                for(const auto Delta:{FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)})
                {
                    const FCityCellCoord B(A.X+Delta.X,A.Y+Delta.Y);
                    if(Parents.Contains(B) || !CanRoad(B) || !ValidRoad(A,B)) continue;
                    Parents.Add(B,A);Queue.Add(B);
                }
            }
            if(!Parents.Contains(Target)) return false;
            TArray<FCityCellCoord> Path;auto Cell=Target;
            while(Parents.FindChecked(Cell)!=Cell) {Path.Add(Cell);Cell=Parents.FindChecked(Cell);}
            for(int32 I=Path.Num()-1;I>=0;--I) {if(!Road(Cell,Path[I])) return false;Cell=Path[I];}
            return true;
        };
        for(const int32 Y:{8,20,32,44,56})
            for(int32 X=5;X<=60;++X)
            {
                if(!CanRoad({X,Y})) continue;
                const int32 Start=X;
                while(X<60 && CanRoad({X+1,Y}) && ValidRoad({X,Y},{X+1,Y})) ++X;
                if(X-Start+1<8 || !Connect({Start,Y})) continue;
                for(int32 C=Start;C<X;++C) if(!Road({C,Y},{C+1,Y})) return false;
            }
        for(int32 I=0;I<Cases.Num();++I)
        {
            const auto* D=Board->BuildingCatalog->FindDefinition(Cases[I].Id);if(!Test->TestNotNull(TEXT("Normal review facility"),D)) return false;
            bool Placed=false;
            // Source reservations and the deterministic hills differ by family.
            // Find a real valid plot along the review roads rather than making
            // a fixed exhibition slot bypass relief, water or occupancy rules.
            for(int32 Row=0;Row<5 && !Placed;++Row)
            for(int32 X=5;X<=60-D->Footprint.Width;++X)
            {
                const int32 Y=9+((I/4+Row)%5)*12;
                auto P=City.BeginPreview(*D,{X,Y});if(!City.ValidatePlacement(P,*D).IsSuccess()) continue;
                const auto R=City.CommitPlacement(P,*D);if(!Test->TestTrue(TEXT("Review uses actual paid placement"),R.IsSuccess())) return false;
                Entities.Add(R.EntityId);Placed=true;break;
            }
            if(!Test->TestTrue(*FString::Printf(TEXT("%s finds a valid hilly plot"),Cases[I].Key),Placed)) return false;
        }
        Sim->Development.InitializeExistingAsOperational(City,Board->BuildingCatalog,0);Sim->SetSimulationPaused(true);
        Sim->CityIdentity.Id=TEXT("00000000000000000000000000000157");Sim->CityIdentity.Name=TEXT("Approved civic batch hills");Sim->CityIdentity.DisasterFrequency=ECityDisasterFrequency::Off;
        const FString Save=Directory/TEXT("Approved-Civic-Playtest.json");FString Error,Json;FCitySaveDocument Doc;
        if(!Sim->SaveCityToPath(Save,Error) || !FFileHelper::LoadFileToString(Json,*Save) || !FCityPersistence::Deserialize(Json,Doc,Error)) {Test->AddError(Error);return false;}
        Doc.Fire.ActivationStep=Doc.Fire.GraceEndStep=0;Doc.bSimulationPaused=true;
        for(int32 I=0;I<Cases.Num();++I)
        {
            auto& State=Doc.Development.States.FindChecked(Entities[I]);State.DensityLevel=Cases[I].Level==2?ECityDensityLevel::High:Cases[I].Level==1?ECityDensityLevel::Medium:ECityDensityLevel::Low;
            if(Cases[I].Condition==1) Doc.Fire.DamagedBuildings.Add(Entities[I],FCityFireDamage{50,-1,0});
            if(Cases[I].Condition==2) {State.Stage=ECityDevelopmentStage::Construction;State.ConstructionCompleteStep=Doc.Economy.FixedStepId+Board->BuildingCatalog->FindDefinition(Cases[I].Id)->ConstructionSteps;}
        }
        if(!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Save,Json,Error) || !Sim->LoadCityFromPath(Save,Error,false)) {Test->AddError(Error);return false;}
        if(!Test->TestTrue(TEXT("Every hilly terrain sample survives the actual save/reload"),ExpectedTerrain==Sim->Terrain.GetQuantizedHeightsCm())) return false;
        ++Sim->PresentationGeneration;Board->PrepareCityView();Test->TestEqual(TEXT("All17 actual facility states survive reload"),Sim->GetPlacementModel().GetEntities().Num(),17);
        FString Reliefs=TEXT("case,plot_relief_cm\n");int32 MaximumRelief=0;
        for(int32 I=0;I<Cases.Num();++I)
        {
            const auto& E=Sim->GetPlacementModel().GetEntities().FindChecked(Entities[I]);
            int32 Minimum=MAX_int32,Maximum=MIN_int32;
            for(int32 Y=E.Anchor.Y*2;Y<=(E.Anchor.Y+E.FootprintSize.Y)*2;++Y)
                for(int32 X=E.Anchor.X*2;X<=(E.Anchor.X+E.FootprintSize.X)*2;++X)
                {const int32 H=Sim->Terrain.GetSampleHeightCm(X,Y);Minimum=FMath::Min(Minimum,H);Maximum=FMath::Max(Maximum,H);}
            MaximumRelief=FMath::Max(MaximumRelief,Maximum-Minimum);
            Reliefs+=FString::Printf(TEXT("%s,%d\n"),Cases[I].Key,Maximum-Minimum);
            const auto P=Board->ResolvePresentationDefinition(E.EntityId,*Board->BuildingCatalog->FindDefinition(E.DefinitionId));
            Test->TestEqual(TEXT("Normal catalog chooses the intended approved upgrade/damage/construction"),P.PresentationMesh.GetAssetName(),FString(Cases[I].Mesh));
            if(Cases[I].Condition==2) continue;
            FVector Door,End;Test->TestTrue(TEXT("Measured public door has an actual sidewalk connection"),Board->FindHomeWalkConnection(E,P,Door,End));
            for(const auto& Bay:CityPrivateEntrances::Bays(P.PresentationMesh.GetAssetName()))
                Test->TestTrue(TEXT("Each staff door and service bay has direct road access"),Board->FindVehicleBayConnection(E,P,Bay,Door,End));
        }
        Test->TestTrue(TEXT("Reviewed facility plots exercise meaningful foundation relief"),MaximumRelief>100);
        Test->TestTrue(TEXT("Measured plot relief saved"),FFileHelper::SaveStringToFile(Reliefs,*(Directory/TEXT("Relief.csv"))));
        return true;
    }
    bool Finish() {if(Camera) Camera->Destroy();if(Board) Board->SetActorTickEnabled(OriginalTick);return true;}
    FAutomationTestBase* Test;ACityBoardActor* Board=nullptr;UCitySimulationSubsystem* Sim=nullptr;ASceneCapture2D* Camera=nullptr;
    TArray<uint64> Entities;FString Directory;bool OriginalTick=true;int32 Case=0,Stage=0,Attempts=0;double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedCivicStudyTest,"CitySim.Desktop.ApprovedCivicBatchStudy",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedCivicStudyTest::RunTest(const FString&)
{
    FString Profile;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests"))) {AddError(TEXT("Civic batch study needs isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityApprovedCivicStudyCommand(this));return true;
}
#endif
