#include "Sandbox/CitySandboxDesktopPawn.h"

#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CityVRToolController.h"
#include "Interaction/CityBoardPicking.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityBeachScenery.h"
#include "Simulation/CitySimulationSubsystem.h"

ACitySandboxDesktopPawn::ACitySandboxDesktopPawn()
{
	BaseEyeHeight = 250.0f;
	GetMeshComponent()->SetVisibility(false);
	bDesktopGameplay = FParse::Param(FCommandLine::Get(), TEXT("CityDesktop"));
	if (bDesktopGameplay)
		bAddDefaultMovementBindings = false;
	// Use the same eye position and control rotation as the original default
	// pawn view, while exposing a camera to shared inspection/badge presentation.
	auto* Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("DesktopCamera"));
	DesktopCamera = Camera;
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
}

bool ACitySandboxDesktopPawn::IsCameraGestureActive() const
{
	const auto* Player = Cast<APlayerController>(GetController());
	return bDesktopGameplay && Player &&
	       (Player->IsInputKeyDown(EKeys::MiddleMouseButton) || Player->IsInputKeyDown(EKeys::RightMouseButton));
}

void ACitySandboxDesktopPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDesktopGameplay)
		return;
	auto* Player = Cast<APlayerController>(GetController());
	ACityVRToolController* Session = nullptr;
	for (TActorIterator<ACityVRToolController> It(GetWorld()); It; ++It)
	{
		Session = *It;
		break;
	}
	if (!Player || !Session)
		return;
	const auto CitySession = GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCitySessionGeneration();
	if (!bCameraInitialized || ObservedCitySession != CitySession ||
	    (Session->IsDesktopCameraInputAllowed() && Player->IsInputKeyDown(EKeys::Home)))
	{
		for (TActorIterator<ACityBoardActor> It(GetWorld()); It; ++It)
		{
			const auto Pose = It->GetActorTransform();
			const auto Size = CityBoardPicking::GetLogicalBoardSize(It->GetDimensions());
			const double Width = Size.X * Pose.GetScale3D().X;
			CameraState.Reset(Pose.TransformPosition(FVector(Size.X * .5, Size.Y * .5, 0)), Width);
			bCameraInitialized = true;
			ObservedCitySession = CitySession;
			break;
		}
	}
	if (Session->IsDesktopCameraInputAllowed())
	{
		float DeltaX = 0, DeltaY = 0;
		Player->GetInputMouseDelta(DeltaX, DeltaY);
		if (Player->IsInputKeyDown(EKeys::RightMouseButton))
			CameraState.Orbit(FVector2D(DeltaX, DeltaY));
		if (Player->IsInputKeyDown(EKeys::MiddleMouseButton))
			CameraState.Drag(FVector2D(DeltaX, DeltaY));
		CameraState.Zoom(Player->GetInputAnalogKeyState(EKeys::MouseWheelAxis));
		const double Forward = (Player->IsInputKeyDown(EKeys::W) || Player->IsInputKeyDown(EKeys::Up)) -
		                       (Player->IsInputKeyDown(EKeys::S) || Player->IsInputKeyDown(EKeys::Down));
		const double Right = (Player->IsInputKeyDown(EKeys::D) || Player->IsInputKeyDown(EKeys::Right)) -
		                     (Player->IsInputKeyDown(EKeys::A) || Player->IsInputKeyDown(EKeys::Left));
		CameraState.Pan(FVector2D(Right, Forward) * FMath::Min(DeltaSeconds, .1f) * .6);
	}
	// Follow the displayed ground when panning at detail distance. The board
	// datum can be metres above/below a beach; aiming at it hides close detail.
	for (TActorIterator<ACityBoardActor> It(GetWorld()); It; ++It)
	{
		const auto Pose = It->GetActorTransform();
		const auto Local = Pose.InverseTransformPosition(CameraState.Focus);
		FVector Normal;
		const double Ground = FCityBeachScenery::SampleGround(
		    GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetTerrain(),
		    Local.X / CityBoardPicking::LogicalCellSizeCm, Local.Y / CityBoardPicking::LogicalCellSizeCm, Normal);
		CameraState.Focus.Z = Pose.TransformPosition(FVector(Local.X, Local.Y, Ground)).Z;
		break;
	}
	SetActorLocation(CameraState.Location());
	Player->SetControlRotation(CameraState.Rotation());
	DesktopCamera->SetRelativeLocation(FVector::ZeroVector);
}

void ACitySandboxDesktopPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (bDesktopGameplay && bCameraInitialized)
		OutResult.PerspectiveNearClipPlane = CameraState.NearClipPlane();
}

void ACitySandboxDesktopPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (NewController)
	{
		NewController->SetControlRotation(FRotator(-25.0, 90.0, 0.0));
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_VIEW Origin=%s Rotation=%s"),
		       *GetPawnViewLocation().ToCompactString(), *NewController->GetControlRotation().ToCompactString());
	}
}
