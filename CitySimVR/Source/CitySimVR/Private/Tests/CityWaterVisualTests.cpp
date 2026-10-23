#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityWaterMesh.h"
#include "ProceduralMeshComponent.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"
#include "Materials/MaterialInstanceDynamic.h"
class FCityWaterCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FCityWaterCaptureCommand(FAutomationTestBase* In) : Test(In) {}
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
		if (!UI || !UI->Board || !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn())
		{
			if (++Attempts > 120)
			{
				Test->AddError(TEXT("Water viewport unavailable"));
				return true;
			}
			Next = FPlatformTime::Seconds() + .5;
			return false;
		}
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Board = UI->Board.Get();
		auto* Atmosphere = Board->FindComponentByClass<UCityAtmosphereComponent>();
		auto* Player = World->GetFirstPlayerController();
		auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn());
		if (!Test->TestNotNull(TEXT("desktop pawn"), Pawn))
			return true;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/Water331");
		FString Error;
		if (Phase == 0)
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			FCityIdentity Identity;
			Identity.Id = TEXT("00000000000000000000000000000331");
			Identity.Name = TEXT("Shoreline waves - calm");
			FCityTerrainGenerationParameters Terrain;
			Terrain.GeneratorVersion = 2;
			Terrain.WaterRulesVersion = 1;
			Terrain.SceneryVersion = 1;
			Terrain.Seed = 6;
			Terrain.BroadElevationAmplitudeCm = Terrain.HillAmplitudeCm = Terrain.RidgeAmplitudeCm = 0;
			Terrain.SeaLevelCm = -160;
			Terrain.QuantizationCm = 1;
			if (!Sim->CreateCity(Identity, Terrain, Directory / TEXT("Cities"), Error))
			{
				Test->AddError(Error);
				return true;
			}
			UI->bDesktopFocused = true;
			UI->LifecycleInputGate.Update(false);
			UI->ReturnToCity();
			Sim->SetEntryMenuSuspended(false);
			Sim->SetSimulationPaused(true);
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->SetPaletteOpen(false);
			Board->PrepareCityView();
			Board->SetToolMode(ECityToolMode::Inspect);
			Board->SetLightingPreset(ECityLightingPreset::Day);
			Atmosphere->Update(Sim, 0);
			FCityEnvironmentSnapshot Weather = Sim->GetEnvironmentSnapshot();
			Weather.WindStrength = 1;
			Weather.bPrecipitation = false;
			if (!Sim->Environment.RestoreState(Weather, Weather.LastFixedStepId, Error) ||
			    !Sim->SaveCityToPath(Directory / TEXT("Calm.json"), Error))
			{
				Test->AddError(Error);
				return true;
			}
			Sim->CityIdentity.Id = TEXT("00000000000000000000000000000332");
			Sim->CityIdentity.Name = TEXT("Shoreline waves - storm");
			Weather.WindStrength = 2;
			Weather.bPrecipitation = true;
			if (!Sim->Environment.RestoreState(Weather, Weather.LastFixedStepId, Error) ||
			    !Sim->SaveCityToPath(Directory / TEXT("Storm.json"), Error))
			{
				Test->AddError(Error);
				return true;
			}
			const auto Water = FCityWaterMesh::Build(Sim->GetTerrain());
			if (!Test->TestTrue(TEXT("real shoreline present"), !Water.ShoreSegments.IsEmpty()))
				return true;
			// Select a bank toward the middle of the map, retaining water beyond it.
			const FVector Center(Sim->GetTerrain().GetDimensions().GetWidth() * 400.0,
			                     Sim->GetTerrain().GetDimensions().GetHeight() * 400.0, 0);
			double Best = MAX_dbl;
			for (const auto& Segment : Water.ShoreSegments)
			{
				const FVector P = (Segment.Key + Segment.Value) * .5;
				const double D = FVector::DistSquared2D(P, Center);
				if (D < Best)
				{
					Best = D;
					TargetLocal = P;
				}
			}
			Pawn->SetActorTickEnabled(false);
			InitialWater = Board->WaterSurface->GetProcMeshSection(0)->ProcVertexBuffer.Num();
			if (!Test->TestTrue(TEXT("cooked foam and splashes"), Board->ShoreFoamSurface->GetNumSections() > 0 &&
			                                                          Board->ShoreSplashSurface->GetNumSections() > 0))
				return true;
		}
		if (Phase < 12)
		{
			const int32 Shot = Phase / 2;
			if (Phase % 2 == 0)
			{
				const bool Storm = Shot >= 3;
				FCityEnvironmentSnapshot Weather = Sim->GetEnvironmentSnapshot();
				Weather.WindStrength = Storm ? 2 : 1;
				Weather.bPrecipitation = Storm;
				if (!Sim->Environment.RestoreState(Weather, Weather.LastFixedStepId, Error))
				{
					Test->AddError(Error);
					return true;
				}
				Atmosphere->Motion.Reset(Weather);
				Atmosphere->Motion.PhaseSeconds = 1.5;
				Board->UpdateWaterEffects();
				for (const auto& Material : Board->WaterEffectsMaterials)
				{
					if (!Test->TestNotNull(TEXT("cooked dynamic effect material"), Material.Get()))
						return true;
					if (Material != Board->WaterEffectsMaterials.Last())
						Test->TestEqual(TEXT("reviewed weather wave endpoint"),
						                Material->K2_GetScalarParameterValue(TEXT("WaveHeightCm")),
						                Storm ? 34.f : 14.f);
					Test->TestEqual(TEXT("shared controlled arrival phase"),
					                Material->K2_GetScalarParameterValue(TEXT("PreviewTime")), 1.5f);
					const auto Up = Material->K2_GetVectorParameterValue(TEXT("EffectAxisZ"));
					Test->TestTrue(
					    TEXT("WPO follows board scale"),
					    FMath::IsNearlyEqual(
					        double(Up.B),
					        Board->WaterSurface->GetComponentTransform().TransformVector(FVector::UpVector).Z, 1.e-6));
				}
				const double Distance = Shot % 3 == 0 ? 25.0 : Shot % 3 == 1 ? 65.0 : 150.0;
				const FVector Target = Board->GetActorTransform().TransformPosition(TargetLocal);
				Pawn->CameraState.Focus = Target;
				Pawn->CameraState.Yaw = 90;
				Pawn->CameraState.Pitch = -50;
				Pawn->CameraState.Distance = Distance;
				Pawn->bCameraInitialized = true;
				const FVector Location = Pawn->CameraState.Location();
				Pawn->SetActorLocation(Location);
				Player->SetControlRotation((Target - Location).Rotation());
			}
			else
				FScreenshotRequest::RequestScreenshot(
				    Directory / FString::Printf(TEXT("%s-%d.png"), Shot >= 3 ? TEXT("Storm") : TEXT("Calm"), Shot % 3),
				    true, false);
		}
		else
		{
			if (!Sim->LoadCityFromPath(Directory / TEXT("Calm.json"), Error, false))
			{
				Test->AddError(Error);
				return true;
			}
			Sim->SetEntryMenuSuspended(false);
			Sim->SetSimulationPaused(true);
			Board->PrepareCityView();
			Atmosphere->Update(Sim, 0);
			Board->UpdateWaterEffects();
			Test->TestEqual(TEXT("reload preserves water geometry"),
			                Board->WaterSurface->GetProcMeshSection(0)->ProcVertexBuffer.Num(), InitialWater);
			Board->SetHeatMapLayers(true);
			Test->TestFalse(TEXT("heat map hides foam"), Board->ShoreFoamSurface->IsVisible());
			Test->TestFalse(TEXT("heat map hides splashes"), Board->ShoreSplashSurface->IsVisible());
			Board->SetHeatMapLayers(false);
			const double Clock = Atmosphere->GetMotion().PhaseSeconds;
			Atmosphere->Update(Sim, .1);
			Test->TestEqual(TEXT("pause freezes shared wave clock"), Atmosphere->GetMotion().PhaseSeconds, Clock);
			Pawn->SetActorTickEnabled(true);
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_WATER331_CAPTURE_COMPLETE Captures=6 Samples=Calm,Storm PhysicalReviewPending=1"));
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + 1;
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0, InitialWater = 0;
	double Next = 0;
	FVector TargetLocal = FVector::ZeroVector;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterVisualTest, "CitySim.Desktop.WaterEffects",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterVisualTest::RunTest(const FString&)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Client-")))
	{
		AddError(TEXT("Requires isolated CI packaged profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityWaterCaptureCommand(this));
	return true;
}
#endif
