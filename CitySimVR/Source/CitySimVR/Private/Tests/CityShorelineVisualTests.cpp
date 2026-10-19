#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Interaction/CityBoardPicking.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "ProceduralMeshComponent.h"
#include "Presentation/CityWaterMesh.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityShorelineCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FCityShorelineCaptureCommand(FAutomationTestBase* InTest) : Test(InTest) {}
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
		if (!UI || !UI->Board || !UI->DesktopHud || !World->GetFirstPlayerController())
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Shoreline viewport did not initialize"));
			return true;
		}
		auto* Board = UI->Board.Get();
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Player = World->GetFirstPlayerController();
		auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn());
		if (!Test->TestNotNull(TEXT("Desktop camera"), Pawn))
			return true;
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/Issue146-Shoreline");
		IFileManager::Get().MakeDirectory(*Directory, true);
		const int32 Fixture = Phase / 20, Local = Phase % 20;
		const int32 Seeds[] = {7429, 1, 5, 7429};
		const TCHAR* Names[] = {TEXT("DiagonalLake"), TEXT("ConcaveLake"), TEXT("NarrowRiver"), TEXT("SteepRiver")};
		if (Fixture == 4)
		{
			Board->RebuildWaterSurface();
			for (const auto& Pair : HiddenMarkers)
				if (Pair.Key.IsValid())
					Pair.Key->SetHiddenInGame(Pair.Value);
			Pawn->SetActorTickEnabled(true);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_SHORE_CAPTURE_COMPLETE Captures=40 Fixtures=4 SaveReload=1"));
			return true;
		}
		for (USceneComponent* Marker :
		     {Board->HoverHighlight.Get(), Board->EntranceMarker.Get(), Board->WindVanePost.Get(),
		      Board->WindVaneArrow.Get(), Board->WindVaneHead.Get(), Board->WindCardinalEast.Get(),
		      Board->WindCardinalSouth.Get(), Board->WindCardinalWest.Get(), Board->WindCardinalNorthHead.Get(),
		      Board->WindCardinalEastHead.Get(), Board->WindCardinalSouthHead.Get(), Board->WindCardinalWestHead.Get()})
			if (Marker)
			{
				if (!HiddenMarkers.Contains(Marker))
					HiddenMarkers.Add(Marker, Marker->bHiddenInGame);
				Marker->SetHiddenInGame(true);
			}
		FString Error;
		if (Local == 0)
		{
			FCityIdentity Identity;
			Identity.Id = FString::Printf(TEXT("0000000000000000000000000000014%d"), Fixture);
			Identity.Name = FString::Printf(TEXT("Shoreline %s"), Names[Fixture]);
			FCityTerrainGenerationParameters P;
			P.Seed = Seeds[Fixture];
			P.GeneratorVersion = Fixture == 0 || Fixture == 3 ? 0 : 2;
			P.WaterRulesVersion = 1;
			P.SceneryVersion = 1;
			if (!Sim->CreateCity(Identity, P, Directory / TEXT("Cities"), Error))
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
			const auto& Terrain = Sim->GetTerrain();
			// Stable target: diagonal shoreline nearest the lake body, or a
			// mid-channel river cell for the river-only generator2 seed5.
			FVector2D Desired(44, 44);
			if (Fixture == 1)
			{
				Desired = FVector2D::ZeroVector;
				int32 Deep = 0;
				for (int32 Y = 0; Y < 64; ++Y)
					for (int32 X = 0; X < 64; ++X)
						if (Terrain.GetWaterClass({X, Y}) == ECityWaterClass::DeepWater)
						{
							Desired += FVector2D(X + .5, Y + .5);
							++Deep;
						}
				Desired /= FMath::Max(1, Deep);
			}
			if (Fixture >= 2 && !Terrain.GetRiverCells().IsEmpty())
			{
				const auto Cell = Terrain.GetRiverCells()[Terrain.GetRiverCells().Num() / 2];
				Desired = FVector2D(Cell.X + .5, Cell.Y + .5);
			}
			double Best = DBL_MAX;
			// Aim at a real terrain/water intersection, not a logical shoreline
			// cell center that can be buried in a steep concave bank.
			for (int32 Y = 0; Y + 1 < Terrain.GetSampleHeight(); ++Y)
				for (int32 X = 0; X + 1 < Terrain.GetSampleWidth(); ++X)
					for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(0, 1)})
					{
						const FVector A(X * 400., Y * 400., Terrain.GetSampleHeightCm(X, Y));
						const FVector B((X + Offset.X) * 400., (Y + Offset.Y) * 400.,
						                Terrain.GetSampleHeightCm(X + Offset.X, Y + Offset.Y));
						const double Plane = P.SeaLevelCm + 2.;
						if ((A.Z < Plane) == (B.Z < Plane))
							continue;
						const FVector Point = FMath::Lerp(A, B, (Plane - A.Z) / (B.Z - A.Z));
						const double Score = FVector2D::DistSquared(Desired, FVector2D(Point.X, Point.Y) / 800.);
						if (Score < Best)
						{
							Best = Score;
							Target = Point;
						}
					}
			if (!Test->TestTrue(TEXT("Fixture has a submerged shoreline target"), Best < DBL_MAX))
				return true;
			const FString Save = Directory / (FString(Names[Fixture]) + TEXT(".json"));
			if (!Sim->SaveCityToPath(Save, Error))
			{
				Test->AddError(Error);
				return true;
			}
			const auto Heights = Terrain.GetQuantizedHeightsCm();
			const auto Classes = Terrain.GetWaterClasses();
			if (!Sim->LoadCityFromPath(Save, Error))
			{
				Test->AddError(Error);
				return true;
			}
			Test->TestTrue(TEXT("Saved fixture regenerates unchanged terrain and classification"),
			               Heights == Sim->GetTerrain().GetQuantizedHeightsCm() &&
			                   Classes == Sim->GetTerrain().GetWaterClasses());
			Board->PrepareCityView();
			Pawn->SetActorTickEnabled(false);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_SHORE_FIXTURE Name=%s Seed=%d Target=%s Save=%s"), Names[Fixture],
			       P.Seed, *Target.ToString(), *Save);
		}
		if (Local % 2 == 0)
		{
			const int32 View = Local / 4;
			if (Local % 4 == 0)
				Legacy(Board, Sim);
			else
			{
				Board->RebuildTerrainSurface();
				Board->RebuildWaterSurface();
			}
			Board->SetLightingPreset(View == 4 ? ECityLightingPreset::Night : ECityLightingPreset::Day);
			const FVector WorldTarget = Board->GetActorTransform().TransformPosition(Target);
			Pawn->CameraState.Focus = WorldTarget;
			Pawn->CameraState.Yaw = View == 3 ? -135 : 45;
			Pawn->CameraState.Pitch = View == 0 ? -62 : View == 2 || (Fixture == 1 && View == 3) ? -72 : -45;
			Pawn->CameraState.Distance = View == 0 ? 4. : View == 2 ? 30. : 14.;
			const FVector Location = Pawn->CameraState.Location();
			Pawn->SetActorLocation(Location);
			Player->SetControlRotation((WorldTarget - Location).Rotation());
		}
		else
		{
			const TCHAR* Views[] = {TEXT("Close"), TEXT("Medium"), TEXT("Far"), TEXT("Opposite"), TEXT("Night")};
			const FString Filename = FString::Printf(TEXT("%s-%s-%s.png"), Names[Fixture], Views[Local / 4],
			                                         Local % 4 == 1 ? TEXT("Before") : TEXT("After"));
			FScreenshotRequest::RequestScreenshot(Directory / Filename, true, false);
		}
		++Phase;
		Next = FPlatformTime::Seconds() + .75;
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0;
	double Next = 0;
	FVector Target = FVector::ZeroVector;
	TMap<TWeakObjectPtr<USceneComponent>, bool> HiddenMarkers;
	void Legacy(ACityBoardActor* Board, UCitySimulationSubsystem* Sim)
	{
		// Compare this pass with Windows232's refined geometry and original shading.
		Board->RebuildTerrainSurface(true, false);
		const auto Mesh = FCityWaterMesh::Build(Sim->GetTerrain(), true);
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Mesh.Vertices.Num());
		Board->WaterSurface->ClearAllMeshSections();
		Board->WaterSurface->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Normals, Mesh.UVs, Mesh.Colors,
		                                       TArray<FProcMeshTangent>(), false);
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShorelineVisualTest, "CitySim.Desktop.ShorelineContours",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityShorelineVisualTest::RunTest(const FString&)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) ||
	    (!UserDir.Contains(TEXT("Client-")) && !UserDir.Contains(TEXT("EditorVisualTests-"))))
	{
		AddError(TEXT("Shoreline captures require an isolated CI visual/client profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityShorelineCaptureCommand(this));
	return true;
}
#endif
