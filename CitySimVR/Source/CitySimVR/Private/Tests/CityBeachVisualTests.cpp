#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Camera/CameraTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

// Actual cooked viewport, normal source sizes, and a CI-owned save/profile only.
class FCityBeachCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FCityBeachCaptureCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (World)
			for (TActorIterator<ACityVRToolController> It(World); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->Board || !UI->DesktopHud || !World->GetFirstPlayerController() ||
		    !World->GetFirstPlayerController()->GetPawn())
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Beach viewport did not initialize"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Board = UI->Board.Get();
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Atmosphere = Board->FindComponentByClass<UCityAtmosphereComponent>();
		auto* Player = World->GetFirstPlayerController();
		auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn());
		if (!Test->TestNotNull(TEXT("Regular desktop camera pawn"), Pawn))
			return true;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/Beach");
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = Directory / TEXT("BeachReview.json");
		FString Error;
		if (Phase == 0)
		{
			FCityIdentity Identity;
			Identity.Id = TEXT("00000000000000000000000000000108");
			Identity.Name = TEXT("Beach Beach Review");
			FCityTerrainGenerationParameters Terrain;
			Terrain.GeneratorVersion = 2;
			Terrain.WaterRulesVersion = 1;
			Terrain.SceneryVersion = 1;
			// Choose from the three focused wet fixtures without requiring all
			// five kinds to share one cluster. The default generator2 seed is dry.
			bool bFixtureFound = false;
			for (const int32 Seed : {5, 1, 6})
			{
				Terrain.Seed = Seed;
				FCityTerrainHeightfield Probe;
				Probe.Generate(Terrain);
				int32 Mask = 0;
				for (const auto& C : FCityBeachScenery::SelectCandidates(Probe))
					Mask |= 1 << static_cast<int32>(C.Kind);
				if (Mask == 31)
				{
					bFixtureFound = true;
					break;
				}
			}
			if (!Test->TestTrue(TEXT("Wet review fixture includes every kind"), bFixtureFound))
				return true;
			if (!Sim->CreateCity(Identity, Terrain, Directory / TEXT("Cities"), Error))
			{
				Test->AddError(Error);
				return true;
			}
			UI->ReturnToCity();
			Sim->SetEntryMenuSuspended(false);
			Sim->SetSimulationPaused(true);
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->SetPaletteOpen(false);
			Board->PrepareCityView();
			Board->SetToolMode(ECityToolMode::Inspect);
			Board->SetLightingPreset(ECityLightingPreset::Day);
			if (!Test->TestNotNull(TEXT("Cooked beach presenter"), Atmosphere))
				return true;
			Atmosphere->Update(Sim, 0);
			Test->TestEqual(TEXT("Five cooked instanced pools"), Atmosphere->BeachPools.Num(), 5);
			InitialCount = Atmosphere->GetBeachDecorationCount();
			Test->TestTrue(TEXT("Cooked source-sized beach props"),
			               InitialCount > 0 && InitialCount <= FCityBeachScenery::MaxProps);
			uint32 BestCluster = 0;
			int32 BestCount = 0;
			for (const auto& Candidate : Atmosphere->BeachCandidates)
			{
				const int32 Count = Atmosphere->BeachCandidates
				                        .FilterByPredicate(
				                            [&](const FCityBeachCandidate& C)
				                            {
					                            return C.ClusterHash == Candidate.ClusterHash &&
					                                   FCityBeachScenery::IsClear(Sim->GetPlacementModel(), C.Cell);
				                            })
				                        .Num();
				if (Count > BestCount)
				{
					BestCount = Count;
					BestCluster = Candidate.ClusterHash;
				}
			}
			if (!Test->TestTrue(TEXT("Real shoreline cluster available for context"), BestCount > 0))
				return true;
			TargetLocal = FVector::ZeroVector;
			for (const auto& Candidate : Atmosphere->BeachCandidates)
				if (Candidate.ClusterHash == BestCluster &&
				    FCityBeachScenery::IsClear(Sim->GetPlacementModel(), Candidate.Cell))
					TargetLocal += Candidate.Transform.GetLocation() / BestCount;
			for (const auto& Pool : Atmosphere->BeachPools)
			{
				if (!Test->TestNotNull(TEXT("Native beach mesh survives cook"), Pool->GetStaticMesh().Get()))
					return true;
				UE_LOG(LogTemp, Display, TEXT("CITYSIM_BEACH_BEACH_POOL Seed=%d Mesh=%s Count=%d"), Terrain.Seed,
				       *Pool->GetStaticMesh()->GetName(), Pool->GetInstanceCount());
				if (!Test->TestTrue(TEXT("Each source kind appears"), Pool->GetInstanceCount() > 0))
					return true;
				FTransform FirstPose;
				Pool->GetInstanceTransform(0, FirstPose);
				DetailTargets.Add(FirstPose.TransformPosition(Pool->GetStaticMesh()->GetBoundingBox().GetCenter()));
				for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
				{
					FTransform Pose;
					Pool->GetInstanceTransform(I, Pose);
					Original.Add(Pose);
					Test->TestTrue(TEXT("Cook preserves approved scale"), Pose.GetScale3D().Equals(FVector::OneVector));
				}
			}
			Player->GetPawn()->SetActorTickEnabled(false);
			if (!Sim->SaveCityToPath(Path, Error))
			{
				Test->AddError(Error);
				return true;
			}
			// Exercise the same zoom clamp and per-view clipping as wheel input.
			Pawn->CameraState.Reset(Board->GetActorLocation(), 200);
			Pawn->CameraState.Zoom(10000);
			Pawn->bCameraInitialized = true;
			Test->TestEqual(TEXT("Regular close zoom reaches detail distance"), Pawn->CameraState.Distance, 1.);
			FMinimalViewInfo View;
			Pawn->CalcCamera(0, View);
			Test->TestEqual(TEXT("Regular desktop view uses close clipping"), View.PerspectiveNearClipPlane,
			                Pawn->CameraState.NearClipPlane());
			OriginalNearClip = GNearClippingPlane;
		}
		if (Phase == 8)
		{
			if (!Sim->LoadCityFromPath(Path, Error, false))
			{
				Test->AddError(Error);
				return true;
			}
			Sim->SetSimulationPaused(true);
			Sim->SetEntryMenuSuspended(false);
			Board->PrepareCityView();
			Atmosphere->Update(Sim, 0);
			Test->TestEqual(TEXT("Cooked save/reload restores beach count"), Atmosphere->GetBeachDecorationCount(),
			                InitialCount);
			int32 Index = 0;
			for (const auto& Pool : Atmosphere->BeachPools)
				for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
				{
					FTransform Pose;
					Pool->GetInstanceTransform(I, Pose);
					if (Original.IsValidIndex(Index))
						Test->TestTrue(TEXT("Save/reload preserves candidate pose/order"),
						               Pose.Equals(Original[Index], 1.e-6));
					++Index;
				}
		}
		if (Phase % 2 == 0 && Phase <= 8)
		{
			Board->SetLightingPreset(Phase == 6 ? ECityLightingPreset::Night : ECityLightingPreset::Day);
			const auto BoardPose = Board->GetActorTransform();
			const auto Target = BoardPose.TransformPosition(Phase == 2 || Phase == 4 ? TargetLocal : DetailTargets[0]);
			Pawn->CameraState.Focus = Target;
			Pawn->CameraState.Yaw = Phase == 4 ? -135 : 90;
			Pawn->CameraState.Pitch = -55;
			Pawn->CameraState.Distance = Phase == 2 ? 15. : Phase == 4 ? 5. : 1.;
			const auto Location = Pawn->CameraState.Location();
			Player->GetPawn()->SetActorLocation(Location);
			Player->SetControlRotation((Target - Location).Rotation());
		}
		else if (Phase < 10)
		{
			const TCHAR* Name = Phase == 1   ? TEXT("CloseDay")
			                    : Phase == 3 ? TEXT("ShoreContext")
			                    : Phase == 5 ? TEXT("Opposite")
			                    : Phase == 7 ? TEXT("CloseNight")
			                                 : TEXT("AfterReload");
			FScreenshotRequest::RequestScreenshot(Directory / (FString(Name) + TEXT(".png")), true, false);
		}
		else if (Phase < 20)
		{
			const int32 Kind = (Phase - 10) / 2;
			const TCHAR* BeachPropNames[] = {TEXT("FanShell"), TEXT("SpiralShell"), TEXT("Starfish"), TEXT("Driftwood"),
			                        TEXT("DuneGrass")};
			const double Sizes[] = {22, 16, 30, 120, 45};
			if (Phase % 2 == 0)
			{
				Board->SetLightingPreset(ECityLightingPreset::Day);
				const auto Pose = Board->GetActorTransform();
				const auto Target = Pose.TransformPosition(DetailTargets[Kind]);
				const auto Location = Target + Pose.TransformVector(FVector(0, -Sizes[Kind] * 3, Sizes[Kind] * 2));
				Player->GetPawn()->SetActorLocation(Location);
				Player->SetControlRotation((Target - Location).Rotation());
			}
			else
				FScreenshotRequest::RequestScreenshot(Directory / (FString(BeachPropNames[Kind]) + TEXT(".png")), true, false);
		}
		else
		{
			Test->TestEqual(TEXT("Desktop detail view leaves the global/VR near plane intact"), GNearClippingPlane,
			                OriginalNearClip);
			Player->GetPawn()->SetActorTickEnabled(true);
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_BEACH_BEACH_CAPTURE_COMPLETE Captures=10 Instances=%d PhysicalReviewPending=1"),
			       InitialCount);
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + 1.;
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0, InitialCount = 0;
	double Next = 0;
	float OriginalNearClip = 10;
	FVector TargetLocal = FVector::ZeroVector;
	TArray<FVector> DetailTargets;
	TArray<FTransform> Original;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBeachVisualTest, "CitySim.Desktop.BeachScenery",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityBeachVisualTest::RunTest(const FString&)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Client-")))
	{
		AddError(TEXT("Beach captures require the CI-owned packaged client profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityBeachCaptureCommand(this));
	return true;
}
#endif
