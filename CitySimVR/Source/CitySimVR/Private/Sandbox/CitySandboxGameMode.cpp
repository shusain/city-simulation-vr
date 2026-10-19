#include "Sandbox/CitySandboxGameMode.h"

#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Interaction/CityVRToolController.h"
#include "NavigationSystem.h"
#include "Performance/CityBenchmarkProbe.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ACitySandboxGameMode::ACitySandboxGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> XrPawn(TEXT("/Game/CitySim/XR/BP_CityXRPawn"));
	XrPawnClass = XrPawn.Class;
	DefaultPawnClass = ACitySandboxDesktopPawn::StaticClass();
	bStartPlayersAsSpectators = false;
}

void ACitySandboxGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld())
	{
		GetWorld()->SpawnActor<ACityVRToolController>();
		GetWorld()->SpawnActor<ACityBenchmarkProbe>();
		FTimerHandle NavigationDiagnosticTimer;
		GetWorldTimerManager().SetTimer(NavigationDiagnosticTimer, this, &ACitySandboxGameMode::LogNavigationCoverage,
		                                2.0f, false);
	}
}

void ACitySandboxGameMode::LogNavigationCoverage() const
{
	UWorld* World = GetWorld();
	const UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!NavigationSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("CITYSIM_NAV_COVERAGE NavigationSystem=Missing"));
		return;
	}

	const FVector ProbeLocations[] = {FVector(0.0, -250.0, 0.0),      FVector(500.0, 0.0, 0.0),
	                                  FVector(-500.0, 0.0, 0.0),      FVector(0.0, 500.0, 0.0),
	                                  FVector(0.0, -500.0, 0.0),      FVector(1500.0, 1500.0, 0.0),
	                                  FVector(-1500.0, -1500.0, 0.0), FVector(0.0, 100.0, 100.0)};
	for (const FVector& Probe : ProbeLocations)
	{
		FNavLocation Projected;
		const bool bReachable =
		    NavigationSystem->ProjectPointToNavigation(Probe, Projected, FVector(100.0, 100.0, 150.0));
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_NAV_COVERAGE Probe=(%.0f,%.0f,%.0f) Valid=%d Projected=(%.0f,%.0f,%.0f)"), Probe.X,
		       Probe.Y, Probe.Z, bReachable, Projected.Location.X, Projected.Location.Y, Projected.Location.Z);
	}
}

UClass* ACitySandboxGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const bool bUseXrPawn = !FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) && XrPawnClass && GEngine &&
	                        GEngine->XRSystem.IsValid() &&
	                        GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*GetWorld());
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_SANDBOX_PAWN_MODE=%s"), bUseXrPawn ? TEXT("XR") : TEXT("Desktop"));
	for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
		It->SetHeadTrackedShadowCoverage(bUseXrPawn);

	return bUseXrPawn ? XrPawnClass.Get() : ACitySandboxDesktopPawn::StaticClass();
}
