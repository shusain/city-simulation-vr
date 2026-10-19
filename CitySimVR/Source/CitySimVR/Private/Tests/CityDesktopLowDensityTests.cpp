#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Presentation/CityDesktopHud.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

class FCityDesktopLowDensityCommand : public IAutomationLatentCommand
{
public:
	explicit FCityDesktopLowDensityCommand(FAutomationTestBase* In) : Test(In) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next) return false;
		auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (World) for (TActorIterator<ACityVRToolController> It(World); It; ++It) { UI = *It; break; }
		if (!UI || !UI->Board || !UI->DesktopHud)
		{
			if (++Attempts < 120) { Next = FPlatformTime::Seconds()+.5; return false; }
			Test->AddError(TEXT("Desktop city did not initialize")); return true;
		}
		auto* Board = UI->Board.Get(); auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		UI->bDesktopFocused = true; UI->LifecycleInputGate.Update(false);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/LowDensity");
		IFileManager::Get().MakeDirectory(*Directory, true); const FString Path = Directory / TEXT("Fixture.json");
		FString Error;
		if (Phase == 0)
		{
			UI->ReturnToCity(); UI->SetObjectivesVisible(false); UI->SetCitySummaryVisible(false);
			FCitySaveDocument Doc; Doc.bSimulationPaused = true; Doc.Fire.ActivationStep = Doc.Fire.GraceEndStep = 0;
			Doc.Terrain.BroadElevationAmplitudeCm = Doc.Terrain.HillAmplitudeCm = Doc.Terrain.RidgeAmplitudeCm = 0;
			FCityTerrainHeightfield Terrain; Terrain.Generate(Doc.Terrain); FCityPlacementModel City(100000); City.SetTerrain(&Terrain);
			auto Road = City.BeginRoadPreview(City.GetEntranceCell()); City.UpdateRoadPreviewEnd(Road,{60,8});
			if (!City.CommitRoad(Road).IsSuccess()) { Test->AddError(TEXT("Fixture road failed")); return true; }
			Road = City.BeginRoadPreview({1,8}); City.UpdateRoadPreviewEnd(Road,{1,28});
			if (!City.CommitRoad(Road).IsSuccess()) { Test->AddError(TEXT("Fixture road branch failed")); return true; }
			int32 Row = 9;
			for (const FName Type : {FName(TEXT("Home")),FName(TEXT("Shop")),FName(TEXT("Factory"))})
			{
				if (Row != 9)
				{
					Road = City.BeginRoadPreview({1,Row-1}); City.UpdateRoadPreviewEnd(Road,{60,Row-1});
					if (!City.CommitRoad(Road).IsSuccess()) { Test->AddError(TEXT("Fixture row road failed")); return true; }
				}
				const auto* D = Board->BuildingCatalog->FindDefinition(Type); TSet<FName> Appearances;
				for (int32 X=4; X<=13 && Appearances.Num()<4; X+=3)
				{
					for (int32 Attempt=0;Attempt<128;++Attempt)
					{
					auto P = City.BeginPreview(*D,{X,Row}); auto R = City.CommitPlacement(P,*D);
					if (!R.IsSuccess()) { Test->AddError(TEXT("Fixture building failed")); return true; }
					FName Appearance; Board->BuildingCatalog->FindLowDensityPresentation(R.EntityId,Type,ECityDensityLevel::Low,false,Appearance);
					if (Appearances.Contains(Appearance)) { City.DemolishEntity(R.EntityId); continue; }
					FCityDevelopmentState S; S.EntityId=R.EntityId; S.Stage=ECityDevelopmentStage::Operational;
					Doc.Development.States.Add(R.EntityId,S);
					if (!Appearances.Contains(Appearance))
					{
						Appearances.Add(Appearance); Samples.Add({R.EntityId,Type,Appearance,{X,Row}});
					}
					break;
					}
				}
				if (!Test->TestEqual(TEXT("Actual city reaches every shape"),Appearances.Num(),4)) return true;
				Row+=10;
			}
			Doc.Placement=City.ExportState(); FString Json;
			if (!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Path,Json,Error) || !Sim->LoadCityFromPath(Path,Error,false))
			{ Test->AddError(Error); return true; }
			Board->PrepareCityView(); Samples.Sort([](const FSample& A,const FSample& B){ return A.Type==B.Type ? A.Appearance.LexicalLess(B.Appearance) : A.Type.LexicalLess(B.Type); });
			auto* Player=World->GetFirstPlayerController(); if (!Player || !Player->GetPawn()) { Test->AddError(TEXT("Desktop camera missing")); return true; }
			Player->GetPawn()->SetActorTickEnabled(false);
			if (auto* Camera=Player->GetPawn()->FindComponentByClass<UCameraComponent>()) Camera->SetFieldOfView(45);
			Board->SetLightingPreset(ECityLightingPreset::Day);
			Phase=1; Next=FPlatformTime::Seconds()+2; return false;
		}
		if (Phase<=12)
		{
			const auto& Sample=Samples[Phase-1]; const auto& D=*Board->BuildingCatalog->FindDefinition(Sample.Type);
			const auto Resolved=Board->ResolvePresentationDefinition(Sample.Id,D); auto* Pool=Board->GetOrCreateInstances(Resolved);
			Test->TestTrue(TEXT("Rendered pool includes actual placed entity"),Pool->GetInstanceCount()>0);
			Aim(World,Board,Sample,Resolved.PresentationMesh.LoadSynchronous()->GetBoundingBox().GetSize().Z);
			if (!bAimed) { bAimed=true; Next=FPlatformTime::Seconds()+1; return false; }
			FScreenshotRequest::RequestScreenshot(Directory/(Sample.Type.ToString()+TEXT("_")+Sample.Appearance.ToString()+TEXT("_Day.png")),true,false);
			bAimed=false;
		}
		else if (Phase==13)
		{
			if (!Sim->SaveCityToPath(Path,Error) || !Sim->LoadCityFromPath(Path,Error,false)) { Test->AddError(Error); return true; }
			Board->PrepareCityView(); Board->SetLightingPreset(ECityLightingPreset::Night);
		}
		else if (Phase==14 || Phase==15)
		{
			const FName Type=Phase==14 ? FName(TEXT("Home")) : FName(TEXT("Factory"));
			const auto& S=*Samples.FindByPredicate([&](const FSample& P){return P.Type==Type && P.Appearance.ToString().StartsWith(TEXT("D_"));});
			const auto R=Board->ResolvePresentationDefinition(S.Id,*Board->BuildingCatalog->FindDefinition(Type));
			Aim(World,Board,S,R.PresentationMesh.LoadSynchronous()->GetBoundingBox().GetSize().Z);
			if (!bAimed) { bAimed=true; Next=FPlatformTime::Seconds()+1; return false; }
			FScreenshotRequest::RequestScreenshot(Directory/(Type.ToString()+TEXT("_D_Night.png")),true,false);bAimed=false;
		}
		else if (Phase==16 || Phase==18)
		{
			FString Json; FCitySaveDocument Doc;
			if (!FFileHelper::LoadFileToString(Json,*Path) || !FCityPersistence::Deserialize(Json,Doc,Error)) { Test->AddError(Error); return true; }
			const auto& S=*Samples.FindByPredicate([](const FSample& P){return P.Type==TEXT("Factory") && P.Appearance.ToString().StartsWith(TEXT("D_"));});
			if (Phase==16) Doc.Fire.DamagedBuildings.Add(S.Id,FCityFireDamage{50,-1,0}); else Doc.Fire.DamagedBuildings.Remove(S.Id);
			if (!FCityPersistence::Serialize(Doc,Json,Error) || !FCityPersistence::WriteAtomic(Path,Json,Error) || !Sim->LoadCityFromPath(Path,Error,false)) { Test->AddError(Error); return true; }
			Board->SetLightingPreset(ECityLightingPreset::Day);Board->PrepareCityView();
		}
		else if (Phase==17 || Phase==19)
		{
			const auto& S=*Samples.FindByPredicate([](const FSample& P){return P.Type==TEXT("Factory") && P.Appearance.ToString().StartsWith(TEXT("D_"));});
			const auto R=Board->ResolvePresentationDefinition(S.Id,*Board->BuildingCatalog->FindDefinition(S.Type));
			Test->TestEqual(TEXT("Actual damage and repair resolve paired mesh"),R.PresentationMesh.ToString().Contains(TEXT("_Damaged")),Phase==17);
			Aim(World,Board,S,R.PresentationMesh.LoadSynchronous()->GetBoundingBox().GetSize().Z);
			if (!bAimed) { bAimed=true; Next=FPlatformTime::Seconds()+1; return false; }
			FScreenshotRequest::RequestScreenshot(Directory/(Phase==17 ? TEXT("Factory_D_Damaged.png") : TEXT("Factory_D_Repaired.png")),true,false);bAimed=false;
		}
		else if (Phase==20)
		{
			const auto& S=*Samples.FindByPredicate([](const FSample& P){return P.Type==TEXT("Factory") && P.Appearance.ToString().StartsWith(TEXT("D_"));});
			UI->InspectBuilding(static_cast<int64>(S.Id));
		}
		else if (Phase==21)
		{
			auto* Preview=Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			FCityDesktopModelSource Source;
			Test->TestTrue(TEXT("Inspector resolves actual steel appearance"),UI->GetDesktopModelSource(Source));
			Test->TestTrue(TEXT("Inspector uses actual selected mesh"),Preview && Preview->HasModel() && Preview->GetPreviewMesh()==Source.Mesh);
			if (Preview && Preview->GetPreviewTarget())
			{
				TArray<FColor> Pixels;auto* Resource=Preview->GetPreviewTarget()->GameThread_GetRenderTargetResource();
				Test->TestTrue(TEXT("Inspector render readback"),Resource && Resource->ReadPixels(Pixels));
				TSet<FColor> Colors;Colors.Append(Pixels);Test->TestTrue(TEXT("Inspector renders nonuniform material"),Colors.Num()>32);
			}
			FScreenshotRequest::RequestScreenshot(Directory/TEXT("Factory_D_Inspector.png"),true,false);
		}
		else { UE_LOG(LogTemp,Display,TEXT("CITYSIM_LOWDENSITY_DESKTOP_COMPLETE Captures=17 SaveReload=1 MatchedDamage=1 Inspector=1 PhysicalInputPending=1")); return true; }
		++Phase;Next=FPlatformTime::Seconds()+1;return false;
	}
private:
	struct FSample { uint64 Id; FName Type,Appearance; FCityCellCoord Anchor; };
	void Aim(UWorld* World,ACityBoardActor* Board,const FSample& S,double Height)
	{
		const auto Pose=Board->GetActorTransform();const auto Target=Pose.TransformPosition(FVector((S.Anchor.X+1)*800.,(S.Anchor.Y+1)*800.,40+Height*.4));
		const double Distance=FMath::Max(Height,1800.) * 3.9;
		const auto Location=Target+Pose.TransformVector(FVector(1,-1,.85).GetSafeNormal()*Distance);
		auto* Player=World->GetFirstPlayerController();Player->GetPawn()->SetActorLocation(Location);Player->SetControlRotation((Target-Location).Rotation());
	}
	FAutomationTestBase* Test;TArray<FSample> Samples;int32 Phase=0,Attempts=0;bool bAimed=false;double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopLowDensityTest,"CitySim.Desktop.LowDensity",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDesktopLowDensityTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(),TEXT("CityDesktop")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Replace(TEXT("\\"),TEXT("/")).Contains(TEXT("LowDensity/Integration")))
	{ AddError(TEXT("Low-density check requires its isolated LowDensity integration profile")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopLowDensityCommand(this));return true;
}
#endif
