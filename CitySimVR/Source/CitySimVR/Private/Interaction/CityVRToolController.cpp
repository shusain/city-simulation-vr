#include "Interaction/CityVRToolController.h"
#include "Presentation/CityTutorialSettings.h"
#include "Simulation/CityTechnology.h"
#include "Simulation/CityCommercialAi.h"
#include "Interaction/CityHudLayout.h"
#include "Presentation/CityAudioSettings.h"
#include "Persistence/CityPersistence.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IXRTrackingSystem.h"
#include "MotionControllerComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Components/WidgetComponent.h"
#include "Engine/Font.h"
#include "Blueprint/UserWidget.h"

namespace
{
using CityHudLayout::CapacityBarRowSpacing;
using CityHudLayout::CapacityBarTopZ;
using CityHudLayout::DemandBarCenterY;
using CityHudLayout::DemandBarHalfWidth;
using CityHudLayout::DemandBarRowSpacing;
using CityHudLayout::DemandBarTopZ;
using CityHudLayout::DemandVisualBound;

constexpr uint16 InputA = 1 << 0;
constexpr uint16 InputB = 1 << 1;
constexpr uint16 InputX = 1 << 2;
constexpr uint16 InputY = 1 << 3;
constexpr uint16 InputLeftGrip = 1 << 4;
constexpr uint16 InputRightGrip = 1 << 5;
constexpr uint16 InputLeftTrigger = 1 << 6;
constexpr uint16 InputRightTrigger = 1 << 7;
constexpr uint16 InputLeftStick = 1 << 8;
constexpr uint16 InputRightStick = 1 << 9;
} // namespace

namespace
{
// Presentation meshes must not project HUD/player silhouettes onto the city.
// Include child actors (e.g. hand visuals), but never unrelated attached world objects.
void DisablePresentationShadows(AActor& Actor)
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Actor.GetComponents(Primitives, true);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCastShadow(false);
		Primitive->SetCastHiddenShadow(false);
		Primitive->SetCastContactShadow(false);
	}
}

constexpr float PaletteRayLength = 1000.0f;
constexpr float PaletteButtonHalfWidth = 7.0f;

TAutoConsoleVariable<float> DrawerDistance(TEXT("citysim.Ui.DrawerDistance"), 80.0f,
                                           TEXT("Tool drawer distance in cm (50-150)."));
TAutoConsoleVariable<float> DrawerHeight(TEXT("citysim.Ui.DrawerHeight"), -28.0f,
                                         TEXT("Tool bar vertical offset in cm (-60 to 0)."));
TAutoConsoleVariable<float> DrawerScale(TEXT("citysim.Ui.DrawerScale"), 1.0f,
                                        TEXT("Tool drawer and hit target scale (0.6-1.5)."));

} // namespace

void ACityVRToolController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	DisablePresentationShadows(*this);
	InitializeHudOverlay();
}

void ACityVRToolController::BeginPlay()
{
	Super::BeginPlay();
	bDesktopUi = FParse::Param(FCommandLine::Get(), TEXT("CityDesktop"));
	SetDebugInfoVisible(false);
	PaletteIconDynamics.Reset();
	for (UMaterialInterface* IconMaterial : PaletteIconMaterials)
	{
		// Overlay conversion already creates a mutable instance with the icon's
		// texture copied. Unreal does not allow a dynamic instance as a parent.
		auto* Dynamic = Cast<UMaterialInstanceDynamic>(IconMaterial);
		PaletteIconDynamics.Add(Dynamic        ? Dynamic
		                        : IconMaterial ? UMaterialInstanceDynamic::Create(IconMaterial, this)
		                                       : nullptr);
	}
	// Constructor visibility changes are not a reliable runtime state boundary
	// for native default subobjects. Initialize the visual and interaction state
	// together after components are registered.
	SetPaletteOpen(false);
	for (const auto& Root : ConditionRoots)
		Root->SetVisibility(false, true);
	ConditionLeaderInstances->SetVisibility(false);
	for (TActorIterator<ACityBoardActor> It(GetWorld()); It; ++It)
	{
		Board = *It;
		BoardPresentationPose = FCityBoardPresentationPose::FromTransform(Board->GetActorTransform());
		break;
	}
	FindMotionControllers();
	if (bDesktopUi)
	{
		SetActorHiddenInGame(true);
		InitializeDesktopVideo();
		auto* Player = GetWorld()->GetFirstPlayerController();
		auto* HudClass = LoadClass<UCityDesktopHud>(
		    nullptr, TEXT("/Game/CitySim/UI/WBP_CityDesktopHud.WBP_CityDesktopHud_C"));
		if (Player && HudClass)
		{
			DesktopHud = CreateWidget<UCityDesktopHud>(Player, HudClass);
			DesktopHud->SetSession(this);
			DesktopHud->AddToViewport();
			Player->bShowMouseCursor = true;
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Player->SetInputMode(Mode);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_HUD_READY SharedContent=1"));
		}
		else
			UE_LOG(LogTemp, Error, TEXT("CITYSIM_DESKTOP_HUD_MISSING"));
	}
	if (!GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->IsBenchmarkActive())
		ShowMainMenu();
	BackgroundDelegateHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
	    this, &ACityVRToolController::HandleApplicationBackgrounded);
	ForegroundDelegateHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
	    this, &ACityVRToolController::HandleApplicationForegrounded);
}

void ACityVRToolController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DesktopVideo.Shutdown();
	if (auto* PC = GetWorld()->GetFirstPlayerController())
		if (auto* Local = PC->GetLocalPlayer())
			if (auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				if (RoadHeightMapping)
					Input->RemoveMappingContext(RoadHeightMapping);
	if (DesktopHud)
		DesktopHud->RemoveFromParent();
	SetEntrySceneActive(false);
	RestoreDebugInfoDisplay();
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundDelegateHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundDelegateHandle);
	Super::EndPlay(EndPlayReason);
}

void ACityVRToolController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryInitializeInput();
	UpdateLifecycleInputGate();
	if (bDesktopUi)
	{
		UpdateEntryScenePresentation();
		if (DesktopVideo.Tick(FPlatformTime::Seconds()) && PalettePage == ECityPalettePage::Graphics)
			SetPalettePage(ECityPalettePage::Graphics);
		UpdateDesktopInteraction();
		UpdateInspectionTooltip();
		RefreshReports();
		return;
	}
	if (!LeftAim || !RightAim || !LeftGrip || !RightGrip || !HeadsetCamera)
	{
		FindMotionControllers();
	}
	UpdateHeadsetStatusAttachment();
	UpdatePaletteAttachment();
	UpdateEntryScenePresentation();
	LogRawInputState();
	UpdateBoardManipulation();
	UpdateConditionIndicators();
	UpdateControllerRay();
	UpdateInspectionTooltip();
}

UInputAction* ACityVRToolController::MakeAction(const TCHAR* Name)
{
	UInputAction* Action = NewObject<UInputAction>(this, Name);
	Action->ValueType = EInputActionValueType::Boolean;
	Action->bConsumeInput = true;
	InputActions.Add(Action);
	return Action;
}

void ACityVRToolController::MapAction(UInputAction* Action, const FKey& Key)
{
	MappingContext->MapKey(Action, Key);
}

bool ACityVRToolController::DisableTemplateMenuBindings(APawn& PlayerPawn)
{
	UInputAction* LeftMenuAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Left.IA_Menu_Toggle_Left"));
	UInputAction* RightMenuAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Right.IA_Menu_Toggle_Right"));
	UEnhancedInputComponent* PawnInput = Cast<UEnhancedInputComponent>(PlayerPawn.InputComponent);
	if (!PawnInput || !LeftMenuAction || !RightMenuAction)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("CITYSIM_TEMPLATE_MENU_DISABLE_FAILED Missing pawn input or action asset"));
		return false;
	}

	int32 RemovedBindings = 0;
	for (int32 Index = PawnInput->GetActionEventBindings().Num() - 1; Index >= 0; --Index)
	{
		const UInputAction* Action = PawnInput->GetActionEventBindings()[Index]->GetAction();
		if (Action == LeftMenuAction || Action == RightMenuAction)
		{
			RemovedBindings += PawnInput->RemoveActionEventBinding(Index) ? 1 : 0;
		}
	}
	if (RemovedBindings > 0)
	{
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_TEMPLATE_MENU_BINDINGS_REMOVED Count=%d RemainingPawnBindings=%d"),
		       RemovedBindings, PawnInput->GetActionEventBindings().Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CITYSIM_TEMPLATE_MENU_BINDINGS_REMOVED Count=0 Pawn=%s"),
		       *PlayerPawn.GetClass()->GetName());
	}
	return true;
}

void ACityVRToolController::TryInitializeInput()
{
	if (bInputInitialized || !Board)
	{
		return;
	}
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	UEnhancedInputComponent* Input =
	    PlayerController ? Cast<UEnhancedInputComponent>(PlayerController->InputComponent) : nullptr;
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
	    LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (bDesktopUi && Input && Subsystem && PlayerPawn)
	{
		InitializeDesktopInput(Input, Subsystem);
		return;
	}
	if (!Input || !Subsystem || !PlayerPawn || !DisableTemplateMenuBindings(*PlayerPawn))
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("CityToolMappingContext"));
	CityFaceMappingContext = LoadObject<UInputMappingContext>(
	    nullptr, TEXT("/Game/CitySim/Input/IMC_CityFaceButtons.IMC_CityFaceButtons"));
	UInputAction* LeftPrimaryAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/CitySim/Input/IA_City_LeftPrimary.IA_City_LeftPrimary"));
	UInputAction* LeftSecondaryAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/CitySim/Input/IA_City_LeftSecondary.IA_City_LeftSecondary"));
	UInputAction* RightPrimaryAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/CitySim/Input/IA_City_RightPrimary.IA_City_RightPrimary"));
	UInputAction* RightSecondaryAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/CitySim/Input/IA_City_RightSecondary.IA_City_RightSecondary"));
	if (!CityFaceMappingContext || !LeftPrimaryAction || !LeftSecondaryAction || !RightPrimaryAction ||
	    !RightSecondaryAction)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_FACE_INPUT_MISSING Static city input asset failed to load"));
		return;
	}
	auto Bind = [this, Input](const TCHAR* Name, const FKey& Key, auto Handler) {
		UInputAction* Action = MakeAction(Name);
		MapAction(Action, Key);
		Input->BindAction(Action, ETriggerEvent::Started, this, Handler);
		return Action;
	};

	Bind(TEXT("CityConfirmDesktop"), EKeys::LeftMouseButton, &ACityVRToolController::ConfirmDesktop);
	Bind(TEXT("CityCancelMouse"), EKeys::RightMouseButton, &ACityVRToolController::Cancel);
	Bind(TEXT("CityCancelEscape"), EKeys::Escape, &ACityVRToolController::Cancel);
	Bind(TEXT("CityRotateDesktop"), EKeys::R, &ACityVRToolController::Rotate);
	Bind(TEXT("CityHomeDesktop"), EKeys::One, &ACityVRToolController::SelectHome);
	Bind(TEXT("CityShopDesktop"), EKeys::Two, &ACityVRToolController::SelectShop);
	Bind(TEXT("CityParkDesktop"), EKeys::Three, &ACityVRToolController::SelectPark);
	Bind(TEXT("CityRoadDesktop"), EKeys::Four, &ACityVRToolController::SelectRoad);
	Bind(TEXT("CityInspectDesktop"), EKeys::Five, &ACityVRToolController::SelectInspect);
	Bind(TEXT("CityDemolishDesktop"), EKeys::Six, &ACityVRToolController::SelectDemolish);
	Bind(TEXT("CityPowerDesktop"), EKeys::Seven, &ACityVRToolController::SelectPower);
	Bind(TEXT("CityServiceDesktop"), EKeys::Eight, &ACityVRToolController::SelectService);
	Bind(TEXT("CityZoneDesktop"), EKeys::Nine, &ACityVRToolController::SelectZone);
	Bind(TEXT("CityStartScenarioDesktop"), EKeys::F2, &ACityVRToolController::StartScenario);
	Bind(TEXT("CityResetScenarioDesktop"), EKeys::F3, &ACityVRToolController::ResetScenario);
	Bind(TEXT("CityPauseDesktop"), EKeys::P, &ACityVRToolController::TogglePause);
	Bind(TEXT("CitySpeed1Desktop"), EKeys::Equals, &ACityVRToolController::SetSpeed1);
	Bind(TEXT("CitySpeed3Desktop"), EKeys::Hyphen, &ACityVRToolController::SetSpeed3);
	Bind(TEXT("CitySaveDesktop"), EKeys::F5, &ACityVRToolController::SaveCity);
	Bind(TEXT("CityLoadDesktop"), EKeys::F9, &ACityVRToolController::LoadCity);
	UInputAction* LeftTriggerAction = Bind(TEXT("CityLeftTrigger"), EKeys::OculusTouch_Left_Trigger_Click,
	                                       &ACityVRToolController::LeftTrigger);
	MapAction(LeftTriggerAction, EKeys::OculusTouch_Left_Trigger_Axis);
	Input->BindAction(LeftTriggerAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::LeftTriggerReleased);
	UInputAction* RightTriggerAction = Bind(TEXT("CityRightTrigger"), EKeys::OculusTouch_Right_Trigger_Click,
	                                        &ACityVRToolController::RightTrigger);
	MapAction(RightTriggerAction, EKeys::OculusTouch_Right_Trigger_Axis);
	Input->BindAction(RightTriggerAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::RightTriggerReleased);
	Input->BindAction(LeftPrimaryAction, ETriggerEvent::Started, this, &ACityVRToolController::LeftPrimary);
	Input->BindAction(LeftPrimaryAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::LeftPrimaryReleased);
	Input->BindAction(RightPrimaryAction, ETriggerEvent::Started, this, &ACityVRToolController::RightPrimary);
	Input->BindAction(RightPrimaryAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::RightPrimaryReleased);
	Input->BindAction(LeftSecondaryAction, ETriggerEvent::Started, this,
	                  &ACityVRToolController::LeftSecondary);
	Input->BindAction(LeftSecondaryAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::LeftSecondaryReleased);
	Input->BindAction(RightSecondaryAction, ETriggerEvent::Started, this,
	                  &ACityVRToolController::RightSecondary);
	Input->BindAction(RightSecondaryAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::RightSecondaryReleased);
	UInputAction* LeftGripAction = Bind(TEXT("CityLeftGrip"), EKeys::OculusTouch_Left_Grip_Click,
	                                    &ACityVRToolController::LeftGripStarted);
	MapAction(LeftGripAction, EKeys::OculusTouch_Left_Grip_Axis);
	Input->BindAction(LeftGripAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::LeftGripCompleted);
	UInputAction* RightGripAction = Bind(TEXT("CityRightGrip"), EKeys::OculusTouch_Right_Grip_Click,
	                                     &ACityVRToolController::RightGripStarted);
	MapAction(RightGripAction, EKeys::OculusTouch_Right_Grip_Axis);
	Input->BindAction(RightGripAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::RightGripCompleted);
	UInputAction* LeftStickAction = Bind(TEXT("CityLeftStick"), EKeys::OculusTouch_Left_Thumbstick_Click,
	                                     &ACityVRToolController::LeftStickClick);
	Input->BindAction(LeftStickAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::LeftStickReleased);
	UInputAction* RightStickAction = Bind(TEXT("CityRightStick"), EKeys::OculusTouch_Right_Thumbstick_Click,
	                                      &ACityVRToolController::RightStickClick);
	Input->BindAction(RightStickAction, ETriggerEvent::Completed, this,
	                  &ACityVRToolController::RightStickReleased);

	// The stock XR pawn already activates IMC_Default. Bind to its proven grab
	// actions as well as the direct device keys so Native OpenXR behavior is
	// observable and the palette follows the same path as template grabbing.
	auto BindTemplateGrab = [this, Input](const TCHAR* AssetPath, const TCHAR* DiagnosticName, auto Handler) {
		if (UInputAction* Action = LoadObject<UInputAction>(nullptr, AssetPath))
		{
			Input->BindAction(Action, ETriggerEvent::Triggered, this, Handler);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_INPUT_TEMPLATE_BOUND Action=%s Asset=%s"), DiagnosticName,
			       AssetPath);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("CITYSIM_INPUT_TEMPLATE_MISSING Action=%s Asset=%s"), DiagnosticName,
			       AssetPath);
		}
	};
	BindTemplateGrab(TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Left_Pressed.IA_Grab_Left_Pressed"),
	                 TEXT("LeftGripPressed"), &ACityVRToolController::LeftGripStarted);
	BindTemplateGrab(TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Left_Released.IA_Grab_Left_Released"),
	                 TEXT("LeftGripReleased"), &ACityVRToolController::LeftGripCompleted);
	BindTemplateGrab(TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Right_Pressed.IA_Grab_Right_Pressed"),
	                 TEXT("RightGripPressed"), &ACityVRToolController::RightGripStarted);
	BindTemplateGrab(TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Right_Released.IA_Grab_Right_Released"),
	                 TEXT("RightGripReleased"), &ACityVRToolController::RightGripCompleted);

	Subsystem->AddMappingContext(MappingContext, 50);
	Subsystem->AddMappingContext(CityFaceMappingContext, 50);
	Board->SetCommandInputBound(true);
	bInputInitialized = true;
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_TOOL_INPUT_READY Dominant=Right ContextPriority=50 StaticFaceContext=1 "
	            "DirectKeys=OculusTouch TriggerAxis+Click GripAxis+Click"));
}

void ACityVRToolController::LogRawInputState()
{
	if (!bDebugInfoVisible)
		return;
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const float LeftGripAxis = PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Left_Grip_Axis);
	const float RightGripAxis = PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Right_Grip_Axis);
	const float LeftTriggerAxis =
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Left_Trigger_Axis);
	const float RightTriggerAxis =
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Right_Trigger_Axis);
	auto IsActionPressed = [this](const uint16 Bit) { return (ActionButtonMask & Bit) != 0; };

	const FString Monitor = FString::Printf(
	    TEXT("QUEST INPUT MONITOR  route:%s  hand:%s  gate:%s\n")
	        TEXT("A:%d B:%d   X:%d Y:%d   LStick:%d RStick:%d\n")
	            TEXT("LGrip:%.2f/%d RGrip:%.2f/%d   LTrig:%.2f/%d RTrig:%.2f/%d\n") TEXT("Last: %s"),
	    bInputInitialized ? TEXT("READY") : TEXT("WAITING"),
	    DominantHand == ECityControllerHand::Right ? TEXT("RIGHT") : TEXT("LEFT"),
	    LifecycleInputGate.CanAcceptInput() ? TEXT("OPEN") : TEXT("RELEASE ALL"), IsActionPressed(InputA),
	    IsActionPressed(InputB), IsActionPressed(InputX), IsActionPressed(InputY),
	    IsActionPressed(InputLeftStick), IsActionPressed(InputRightStick), LeftGripAxis,
	    IsActionPressed(InputLeftGrip), RightGripAxis, IsActionPressed(InputRightGrip), LeftTriggerAxis,
	    IsActionPressed(InputLeftTrigger), RightTriggerAxis, IsActionPressed(InputRightTrigger),
	    *LastInputTransition);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9100, 0.0f, FColor::Orange, Monitor, false);
	}
}

void ACityVRToolController::SetDebugInfoVisible(const bool bVisible)
{
	bDebugInfoVisible = bVisible && !UE_BUILD_SHIPPING;
	if (!bDebugInfoVisible && Board &&
	    ((Board->GetToolMode() >= ECityToolMode::TestFire &&
	      Board->GetToolMode() <= ECityToolMode::TestArson) ||
	     Board->GetToolMode() == ECityToolMode::Home || Board->GetToolMode() == ECityToolMode::Shop ||
	     Board->GetToolMode() == ECityToolMode::TestSickness ||
	     Board->GetToolMode() == ECityToolMode::TestSevereSickness))
	{
		Board->RequestCancel();
		Board->SetToolMode(ECityToolMode::Inspect);
	}
	if (PaletteLabels.Num() > 0)
		SetPalettePage(!bDebugInfoVisible && PalettePage == ECityPalettePage::SafetyTests
		                   ? ECityPalettePage::Settings
		                   : PalettePage);
	if (GEngine)
	{
		if (!bOwnsDebugInfoDisplay)
		{
			bPreviousDebugInfoDisplay = GEngine->bEnableOnScreenDebugMessagesDisplay;
			bOwnsDebugInfoDisplay = true;
		}
		// This only controls rendering. Diagnostics continue to reach the log.
		GEngine->bEnableOnScreenDebugMessagesDisplay = bDebugInfoVisible && !bEntrySceneActive;
		if (!bVisible)
			GEngine->RemoveOnScreenDebugMessage(9100);
	}
}

void ACityVRToolController::RestoreDebugInfoDisplay()
{
	if (GEngine && bOwnsDebugInfoDisplay)
		GEngine->bEnableOnScreenDebugMessagesDisplay = bPreviousDebugInfoDisplay;
	bOwnsDebugInfoDisplay = false;
}

void ACityVRToolController::RecordInputAction(const uint16 InputBit, const TCHAR* InputName,
                                              const bool bPressed)
{
	const bool bWasPressed = (ActionButtonMask & InputBit) != 0;
	if (bWasPressed == bPressed)
	{
		return;
	}
	ActionButtonMask = bPressed ? ActionButtonMask | InputBit : ActionButtonMask & ~InputBit;
	LastInputTransition =
	    FString::Printf(TEXT("%s %s"), InputName, bPressed ? TEXT("PRESS") : TEXT("RELEASE"));
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_INPUT_EDGE %s Route=EnhancedInput Mask=0x%03X Palette=%d Dominant=%s"),
	       *LastInputTransition, ActionButtonMask, bPaletteOpen,
	       DominantHand == ECityControllerHand::Right ? TEXT("Right") : TEXT("Left"));
}

void ACityVRToolController::FindMotionControllers()
{
	APawn* Pawn =
	    GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	// Apply after the gameplay pawn is constructed, including delayed player binding.
	// XR visualization asynchronously changes its mesh on the same component, so
	// the policy also survives controller model loads and interaction-profile changes.
	DisablePresentationShadows(*Pawn);
	HeadsetCamera = Pawn->FindComponentByClass<UCameraComponent>();
	TInlineComponentArray<UMotionControllerComponent*> Components(Pawn);
	for (UMotionControllerComponent* Component : Components)
	{
		const FString Name = Component->GetName();
		if (Name.Contains(TEXT("LeftAim")))
			LeftAim = Component;
		else if (Name.Contains(TEXT("RightAim")))
			RightAim = Component;
		else if (Name.Contains(TEXT("LeftGrip")))
			LeftGrip = Component;
		else if (Name.Contains(TEXT("RightGrip")))
			RightGrip = Component;
	}
}

void ACityVRToolController::UpdateHeadsetStatusAttachment()
{
	if (!HeadsetCamera)
	{
		return;
	}
	if (HeadsetStatusRoot->GetAttachParent() != HeadsetCamera)
	{
		HeadsetStatusRoot->AttachToComponent(HeadsetCamera,
		                                     FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	if (InspectorStatusRoot->GetAttachParent())
		InspectorStatusRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	if (InspectorLeader->GetAttachParent())
		InspectorLeader->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	if (NeedsToggleRoot->GetAttachParent() != HeadsetCamera)
		NeedsToggleRoot->AttachToComponent(HeadsetCamera,
		                                   FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Align with the play/speed row (headset status Z 22 + controls Z 7.5),
	// centered over the dashboard rather than inside its information area.
	NeedsToggleRoot->SetRelativeLocation(FVector(80.0, 31.0, 29.5));
	NeedsToggleRoot->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	if (ObjectiveToggleRoot->GetAttachParent() != HeadsetCamera)
		ObjectiveToggleRoot->AttachToComponent(HeadsetCamera,
		                                       FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ObjectiveToggleRoot->SetRelativeLocation(FVector(80, -31, 29.5));
	ObjectiveToggleRoot->SetRelativeRotation(FRotator(0, 180, 0));
	if (ObjectiveRoot->GetAttachParent() != HeadsetCamera)
		ObjectiveRoot->AttachToComponent(HeadsetCamera,
		                                 FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ObjectiveRoot->SetRelativeLocation(FVector(80, -28, 24));
	ObjectiveRoot->SetRelativeRotation(FRotator(0, 180, 0));

	// Keep the compact summary in the upper-right periphery at a comfortable
	// focal distance. TextRender faces along +X, so turn it back toward the HMD.
	HeadsetStatusRoot->SetRelativeLocation(FVector(80.0f, 0.0f, 22.0f));
	HeadsetStatusRoot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	NeedsStatusRoot->SetRelativeLocation(FVector(0.0f, -15.5f, 0.0f));
}

void ACityVRToolController::UpdatePaletteAttachment()
{
	if (bEntrySceneActive)
	{
		if (!HeadsetCamera)
			return;
		if (PaletteRoot->GetAttachParent() != HeadsetCamera)
			PaletteRoot->AttachToComponent(HeadsetCamera,
			                               FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PaletteRoot->SetRelativeLocation(FVector(80, 0, -11));
		PaletteRoot->SetRelativeRotation(FRotator(0, 180, 0));
		PaletteRoot->SetRelativeScale3D(FVector::OneVector);
		return;
	}
	if (!UsesHandPalette())
	{
		if (!HeadsetCamera)
			return;
		const FVector Offset(FMath::Clamp(DrawerDistance.GetValueOnGameThread(), 50.0f, 150.0f), 0.0f,
		                     FMath::Clamp(DrawerHeight.GetValueOnGameThread(), -60.0f, 0.0f));
		const float Scale = FMath::Clamp(DrawerScale.GetValueOnGameThread(), 0.6f, 1.5f);
		if (PaletteAnchorMode == 0 || PalettePage == ECityPalettePage::Reports)
		{
			if (PaletteRoot->GetAttachParent() != HeadsetCamera)
				PaletteRoot->AttachToComponent(HeadsetCamera,
				                               FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			PaletteRoot->SetRelativeLocation(Offset);
			PaletteRoot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
			PaletteRoot->SetRelativeScale3D(FVector(Scale));
		}
		else if (bPaletteOpen && !bPalettePoseCaptured)
		{
			PaletteRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			PaletteRoot->SetWorldLocation(HeadsetCamera->GetComponentTransform().TransformPosition(Offset));
			PaletteRoot->SetWorldRotation(HeadsetCamera->GetComponentQuat() *
			                              FRotator(0.0f, 180.0f, 0.0f).Quaternion());
			PaletteRoot->SetWorldScale3D(FVector(Scale));
			bPalettePoseCaptured = true;
		}
		return;
	}
	UMotionControllerComponent* OffHandGrip =
	    DominantHand == ECityControllerHand::Right ? LeftGrip.Get() : RightGrip.Get();
	if (!OffHandGrip)
	{
		return;
	}
	if (PaletteRoot->GetAttachParent() != OffHandGrip)
	{
		PaletteRoot->AttachToComponent(OffHandGrip, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PaletteRoot->SetRelativeLocation(FVector(10.0f, 0.0f, 5.0f));
	}
	PaletteRoot->SetRelativeScale3D(FVector::OneVector);

	// Keep the panel attached to the off hand while making its +X text face the
	// headset. Controller-local axes differ between hands and can otherwise put
	// the readable face against the glove.
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		const FVector ToHeadset =
		    PlayerController->PlayerCameraManager->GetCameraLocation() - PaletteRoot->GetComponentLocation();
		if (!ToHeadset.IsNearlyZero())
		{
			FRotator FacingRotation = ToHeadset.Rotation();
			FacingRotation.Roll = 0.0f;
			PaletteRoot->SetWorldRotation(FacingRotation);
		}
	}
}

void ACityVRToolController::CyclePaletteAnchor()
{
	CancelTaxEdit();
	PaletteAnchorMode = (PaletteAnchorMode + 1) % 3;
	bPalettePoseCaptured = false;
	UpdatePaletteAttachment();
	SetPalettePage(ECityPalettePage::Settings);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_MENU_ANCHOR Mode=%d"), PaletteAnchorMode);
}

void ACityVRToolController::UpdateRoadHeightInput()
{
	auto* PC = GetWorld()->GetFirstPlayerController();
	auto* Local = PC ? PC->GetLocalPlayer() : nullptr;
	auto* Input = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const bool Enabled = PC && Board && Board->GetToolMode() == ECityToolMode::Road && !bPaletteOpen &&
	                     !bMainMenuPinned && !bManipulationSession && LifecycleInputGate.CanAcceptInput();
	if (Input && (Enabled != bRoadHeightContextActive || (Enabled && RoadHeightHand != DominantHand)))
	{
		if (RoadHeightMapping)
			Input->RemoveMappingContext(RoadHeightMapping);
		RoadHeightControl.Update(0, false);
		if (Enabled)
		{
			RoadHeightMapping = NewObject<UInputMappingContext>(this);
			RoadHeightAction = NewObject<UInputAction>(this);
			RoadHeightAction->ValueType = EInputActionValueType::Axis1D;
			RoadHeightAction->bConsumeInput = true;
			RoadHeightMapping->MapKey(RoadHeightAction, DominantHand == ECityControllerHand::Right
			                                                ? EKeys::OculusTouch_Right_Thumbstick_Y
			                                                : EKeys::OculusTouch_Left_Thumbstick_Y);
			RoadHeightMapping->MapKey(RoadHeightAction, DominantHand == ECityControllerHand::Right
			                                                ? EKeys::OculusTouch_Right_Thumbstick_2D
			                                                : EKeys::OculusTouch_Left_Thumbstick_2D);
			Input->AddMappingContext(RoadHeightMapping, 60);
			RoadHeightHand = DominantHand;
		}
		bRoadHeightContextActive = Enabled;
	}
	const double Axis = PC ? PC->GetInputAnalogKeyState(DominantHand == ECityControllerHand::Right
	                                                        ? EKeys::OculusTouch_Right_Thumbstick_Y
	                                                        : EKeys::OculusTouch_Left_Thumbstick_Y)
	                       : 0;
	const int32 Delta = RoadHeightControl.Update(Axis, Enabled);
	if (Delta)
		Board->AdjustRoadElevation(Delta);
}

void ACityVRToolController::UpdateControllerRay()
{
	UpdateRoadHeightInput();
	if (!Board)
	{
		return;
	}
	SummaryText->SetText(FText::FromString(TEXT("CITY OVERVIEW")));
	UpdateCityInfoCards();
	CalendarText->SetText(FText::FromString(Board->DescribeCalendar()));
	UpdateCityTotals();
	const FString HeatDescription = Board->DescribeHeatMap();
	const bool bShowHeatLegend =
	    !HeatDescription.IsEmpty() && !bManipulationSession && LifecycleInputGate.CanAcceptInput();
	HeatMapLegend->SetText(FText::FromString(HeatDescription));
	HeatMapLegend->SetVisibility(bShowHeatLegend);
	HeatMapLegendBackground->SetVisibility(bShowHeatLegend);
	for (int32 Index = 0; Index < TimeControlButtons.Num(); ++Index)
	{
		const int32 Speed = Index == 1 ? 1 : Index == 2 ? 3 : 5;
		const bool bSelected =
		    Index == 0
		        ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->IsSimulationPaused()
		        : GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetSimulationSpeed() == Speed &&
		              !GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->IsSimulationPaused();
		TimeControlButtons[Index]->SetVisibility(true);
		TimeControlLabels[Index]->SetTextRenderColor(FColor::White);
		if (!TimeButtonMaterials.IsValidIndex(Index))
			TimeButtonMaterials.Add(TimeControlButtons[Index]->CreateDynamicMaterialInstance(0));
		if (TimeButtonMaterials[Index])
			TimeButtonMaterials[Index]->SetVectorParameterValue(TEXT("BarColor"),
			                                                    bSelected ? FLinearColor(.025f, .3f, .65f)
			                                                              : FLinearColor(.025f, .035f, .05f));
	}
	UpdateNeedsStatusBars();
	HoveredObjectiveFooter = INDEX_NONE;
	bObjectiveToggleFocused = false;
	bObjectivePanelFocused = false;
	UpdateObjectiveCard();
	bNeedsToggleFocused = false;
	bNeedsPanelFocused = false;
	HoveredTimeControl = INDEX_NONE;
	bPaletteSurfaceFocused = false;
	bInspectorPanelFocused = false;
	RefreshNeedsVisibility();
	UpdateConstructionCallout();

	if (bManipulationSession)
	{
		Board->SetControllerRay(FVector::ZeroVector, FVector::ForwardVector, false);

		return;
	}

	UMotionControllerComponent* Aim =
	    DominantHand == ECityControllerHand::Right ? RightAim.Get() : LeftAim.Get();
	const bool bXrActive = GEngine && GEngine->XRSystem.IsValid() &&
	                       GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*GetWorld());
	if (!Aim || !bXrActive)
	{
		Board->SetControllerRay(FVector::ZeroVector, FVector::ForwardVector, false);
		return;
	}
	const FVector Origin = Aim->GetComponentLocation();
	const FVector Direction = Aim->GetForwardVector();
	UpdateTaxDragFromRay(Origin, Direction);
	const int32 NewHoveredIndex = bPaletteOpen ? FindPaletteButton(Origin, Direction) : INDEX_NONE;
	if (NewHoveredIndex != HoveredPaletteIndex)
	{
		HoveredPaletteIndex = NewHoveredIndex;
		RefreshPaletteText();
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_PALETTE_HOVER Index=%d"), HoveredPaletteIndex);
	}
	bPaletteSurfaceFocused = bPaletteOpen && IsPaletteSurfaceHit(Origin, Direction);
	HoveredTimeControl = FindTimeControl(Origin, Direction);
	bInspectorPanelFocused = !bPaletteOpen && IsInspectorSurfaceHit(Origin, Direction);
	bNeedsToggleFocused = !bPaletteOpen && IsNeedsToggleHit(Origin, Direction);
	bNeedsPanelFocused = !bPaletteOpen && IsNeedsPanelHit(Origin, Direction);
	RefreshNeedsVisibility();
	HoveredObjectiveFooter = !bPaletteOpen ? FindObjectiveFooter(Origin, Direction) : INDEX_NONE;
	bObjectiveToggleFocused = !bPaletteOpen && IsObjectiveToggleHit(Origin, Direction);
	bObjectivePanelFocused = !bPaletteOpen && IsObjectivePanelHit(Origin, Direction);
	UpdateObjectiveCard();
	const float RayDistance = GetHudRayDistance(Origin, Direction, PaletteRayLength);
	const bool bUiFocused = RayDistance < PaletteRayLength || bInspectorPanelFocused ||
	                        HoveredObjectiveFooter != INDEX_NONE || bObjectiveToggleFocused ||
	                        bObjectivePanelFocused || bPaletteOpen || bNeedsToggleFocused ||
	                        bNeedsPanelFocused || HoveredTimeControl != INDEX_NONE;
	Board->SetControllerRay(Origin, Direction, !bUiFocused, bUiFocused);
	DrawDebugLine(GetWorld(), Origin, Origin + Direction * RayDistance,
	              bUiFocused ? FColor::Yellow : FColor::Cyan, false, 0.0f, 0, 0.35f);
}

namespace
{
bool HitsPanelRect(const USceneComponent* Surface, const FVector& Origin, const FVector& Direction,
                   const double HalfWidth, const double HalfHeight)
{
	const FVector Normal = Surface->GetForwardVector();
	const double Denominator = FVector::DotProduct(Direction, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
		return false;
	const double Distance =
	    FVector::DotProduct(Surface->GetComponentLocation() - Origin, Normal) / Denominator;
	if (Distance < 0.0 || Distance > PaletteRayLength)
		return false;
	const FVector Local =
	    Surface->GetComponentTransform().InverseTransformPosition(Origin + Direction * Distance);
	return FMath::Abs(Local.Y) <= HalfWidth && FMath::Abs(Local.Z) <= HalfHeight;
}

FString WrapInspectionText(const FString& Text)
{
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	FString Result;
	for (const FString& Line : Lines)
	{
		TArray<FString> Words;
		Line.ParseIntoArray(Words, TEXT(" "), true);
		int32 Column = 0;
		for (const FString& Word : Words)
		{
			if (Column > 0 && Column + Word.Len() + 1 > 60)
			{
				Result += TEXT("\n");
				Column = 0;
			}
			if (Column > 0)
			{
				Result += TEXT(" ");
				++Column;
			}
			Result += Word;
			Column += Word.Len();
		}
		Result += TEXT("\n");
	}
	return Result.TrimEnd();
}
} // namespace

bool ACityVRToolController::IsObjectiveToggleHit(const FVector& Origin, const FVector& Direction) const
{
	return ObjectiveToggle->IsVisible() && HitsPanelRect(ObjectiveToggleRoot, Origin, Direction, 3, 3);
}

bool ACityVRToolController::IsObjectivePanelHit(const FVector& Origin, const FVector& Direction) const
{
	return ObjectiveBackground->IsVisible() && HitsPanelRect(ObjectiveBackground, Origin, Direction, 50, 50);
}

int32 ACityVRToolController::FindObjectiveFooter(const FVector& Origin, const FVector& Direction) const
{
	for (int32 Index = 0; Index < ObjectiveFooterButtons.Num(); ++Index)
		if (ObjectiveFooterButtons[Index]->IsVisible() &&
		    HitsPanelRect(ObjectiveFooterButtons[Index], Origin, Direction, 50, 50))
			return Index;
	return INDEX_NONE;
}

bool ACityVRToolController::IsNeedsToggleHit(const FVector& Origin, const FVector& Direction) const
{
	return HitsPanelRect(NeedsToggleRoot, Origin, Direction, 3.0, 3.0);
}

bool ACityVRToolController::IsNeedsPanelHit(const FVector& Origin, const FVector& Direction) const
{
	return bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId && !bPaletteOpen && !bEntrySceneActive &&
	       HitsPanelRect(NeedsBackground, Origin, Direction, 50.0, 50.0);
}

void ACityVRToolController::RefreshNeedsVisibility()
{
	const bool bVisible = bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId &&
	                      !bPaletteOpen && !bEntrySceneActive;
	SummaryText->SetVisibility(bVisible);
	NeedsText->SetVisibility(bVisible);
	NeedsBackground->SetVisibility(bVisible);
	GrowthNeedsTitle->SetVisibility(bVisible);
	GrowthNeedsBackground->SetVisibility(bVisible);
	for (int32 Index = 0; Index < CityInfoTitles.Num(); ++Index)
	{
		CityInfoTitles[Index]->SetVisibility(bVisible);
		CityInfoLabels[Index]->SetVisibility(bVisible);
		CityInfoValues[Index]->SetVisibility(bVisible);
		CityInfoBackgrounds[Index]->SetVisibility(bVisible);
	}
	NeedsToggleHelp->SetVisibility(bNeedsToggleFocused);
	NeedsToggleHelp->SetText(
	    FText::FromString(bNeedsOpen ? TEXT("Hide city summary") : TEXT("Show city summary")));
	NeedsToggleIcon->SetRelativeScale3D(FVector(bNeedsToggleFocused ? 0.047 : bNeedsOpen ? 0.043 : 0.04));
	if (bVisible)
	{
		// Include exact text extents plus the fixed utilization/demand rails.
		FBox Bounds(FVector(-1.0, -26.0, -36.0), FVector(1.0, 2.0, 2.0));
		Bounds += SummaryText->CalcBounds(SummaryText->GetRelativeTransform()).GetBox();
		Bounds += NeedsText->CalcBounds(NeedsText->GetRelativeTransform()).GetBox();
		FBox GuidanceBounds = NeedsText->CalcBounds(NeedsText->GetRelativeTransform()).GetBox();
		GuidanceBounds += GrowthNeedsTitle->CalcBounds(GrowthNeedsTitle->GetRelativeTransform()).GetBox();
		GuidanceBounds.Min.Y = FMath::Min(GuidanceBounds.Min.Y, -34.5);
		GuidanceBounds.Max.Y = FMath::Max(GuidanceBounds.Max.Y, 0.0);
		GrowthNeedsBackground->SetRelativeLocation(
		    FVector(.1f, GuidanceBounds.GetCenter().Y, GuidanceBounds.GetCenter().Z));
		GrowthNeedsBackground->SetRelativeScale3D(FVector(.003f, (GuidanceBounds.GetSize().Y + 2.f) / 100.f,
		                                                  (GuidanceBounds.GetSize().Z + 2.f) / 100.f));
		Bounds += GrowthNeedsBackground->CalcBounds(GrowthNeedsBackground->GetRelativeTransform()).GetBox();
		for (int32 Index = 0; Index < CityInfoTitles.Num(); ++Index)
		{
			Bounds +=
			    CityInfoTitles[Index]->CalcBounds(CityInfoTitles[Index]->GetRelativeTransform()).GetBox();
			Bounds +=
			    CityInfoLabels[Index]->CalcBounds(CityInfoLabels[Index]->GetRelativeTransform()).GetBox();
			Bounds +=
			    CityInfoValues[Index]->CalcBounds(CityInfoValues[Index]->GetRelativeTransform()).GetBox();
			Bounds += CityInfoBackgrounds[Index]
			              ->CalcBounds(CityInfoBackgrounds[Index]->GetRelativeTransform())
			              .GetBox();
		}
		for (UTextRenderComponent* Label : NeedsCapacityLabels)
			Bounds += Label->CalcBounds(Label->GetRelativeTransform()).GetBox();
		for (UTextRenderComponent* Label : NeedsDemandLabels)
			Bounds += Label->CalcBounds(Label->GetRelativeTransform()).GetBox();
		const FVector Center = Bounds.GetCenter();
		const FVector Size = Bounds.GetSize();
		NeedsBackground->SetRelativeLocation(FVector(-0.8, Center.Y, Center.Z));
		NeedsBackground->SetRelativeScale3D(FVector(0.008, (Size.Y + 3.0) / 100.0, (Size.Z + 3.0) / 100.0));
	}
}

void ACityVRToolController::ClearInspection()
{
	StationUpgradePreviewEntity = 0;
	SharedUpgradeLabel = FText::GetEmpty();
	bInspectorUpgradeAvailable = false;
	bInspectorPanelFocused = false;
	InspectedEntityId = FCityGrid::EmptyEntityId;
	LastInspectionText.Reset();
	SharedInspectionMetrics.Reset();
	InspectorStatusRoot->SetVisibility(false, true);
	SharedInspectionDetail.Reset();
	SharedInspectionTitle = FText::GetEmpty();
	SharedInspectionSummary = FText::GetEmpty();
	SharedInspectionGauges.Reset();
	SharedInspectionSections.Reset();
	InspectorLeader->SetVisibility(false);
	if (Board)
		Board->SetInspectionEntity(FCityGrid::EmptyEntityId);
	RefreshNeedsVisibility();
	UpdateNeedsStatusBars();
}

void ACityVRToolController::ToggleInspection(const FVector& Origin, const FVector& Direction)
{
	if (!Board || Board->GetToolMode() != ECityToolMode::Inspect)
		return;
	uint64 EntityId = FCityGrid::EmptyEntityId;
	FVector Hit;
	const bool bBuilding = Board->PickInspectionBuilding(Origin, Direction, EntityId, Hit);
	UpdateConditionIndicators();
	double MarkerDistance = 0;
	const int32 Marker = FindConditionIndicator(Origin, Direction, MarkerDistance);
	// A highlighted building takes priority; badges remain shortcuts over empty tiles.
	if (Marker != INDEX_NONE && !bBuilding)
	{
		EntityId = PickConditionMember(Marker, Origin, Direction);
		FString Ignored;
		Board->GetInspectionBuilding(EntityId, Hit, Ignored);
	}
	if (EntityId == FCityGrid::EmptyEntityId)
	{
		ClearInspection();
		return;
	}
	InspectBuilding(static_cast<int64>(EntityId));
	InspectionLocalHit = Board->GetActorTransform().InverseTransformPosition(Hit);
	UpdateInspectionTooltip();
}

void ACityVRToolController::InspectBuilding(const int64 EntityId)
{
	if (!Board || Board->GetToolMode() != ECityToolMode::Inspect || EntityId <= 0)
	{
		ClearInspection();
		return;
	}
	FVector Roof;
	FString Detail;
	if (!Board->GetInspectionBuilding(static_cast<uint64>(EntityId), Roof, Detail))
	{
		ClearInspection();
		return;
	}
	StationUpgradePreviewEntity = 0;
	InspectedEntityId = static_cast<uint64>(EntityId);
	InspectionLocalHit = Board->GetActorTransform().InverseTransformPosition(Roof);
	InspectionGeneration = GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCitySessionGeneration();
	Board->SetInspectionEntity(InspectedEntityId);
	RefreshNeedsVisibility();
	UpdateNeedsStatusBars();
	UpdateInspectionTooltip();
}

void ACityVRToolController::UpdateInspectionTooltip()
{
	if (!Board || !HeadsetCamera || InspectedEntityId == FCityGrid::EmptyEntityId)
		return;
	const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	FVector Roof;
	FString Detail;
	TArray<FCityInspectionMetric> Metrics;
	if (Board->GetToolMode() != ECityToolMode::Inspect || !Simulation ||
	    InspectionGeneration != Simulation->GetCitySessionGeneration() ||
	    !Board->GetInspectionBuilding(InspectedEntityId, Roof, Detail, &Metrics,
	                                  bDesktopUi ? &SharedInspectionSections : nullptr))
	{
		ClearInspection();
		return;
	}
	const bool bShow = !bPaletteOpen && !bManipulationSession && LifecycleInputGate.CanAcceptInput();
	InspectorStatusRoot->SetVisibility(bShow, true);
	InspectorLeader->SetVisibility(bShow);
	if (!bShow)
		return;
	const auto* Entity = Simulation->GetPlacementModel().GetEntities().Find(InspectedEntityId);
	const auto* Definition = Entity && Simulation->GetBuildingCatalog()
	                             ? Simulation->GetBuildingCatalog()->FindDefinition(Entity->DefinitionId)
	                             : nullptr;
	const auto* State = Simulation->GetDevelopmentSnapshot().Find(InspectedEntityId);
	const bool Station = Definition && Definition->bPlayerPurchasedLevels && State;
	bInspectorUpgradeAvailable = Station && State->Stage == ECityDevelopmentStage::Operational &&
	                             !State->HasCivicUpgrade() && State->DensityLevel != ECityDensityLevel::High;
	InspectorUpgradeButton->SetVisibility(Station);
	InspectorUpgradeLabel->SetVisibility(Station);
	const auto* Next = bInspectorUpgradeAvailable
	                       ? Definition->FindDensityLevel(
	                             static_cast<ECityDensityLevel>(static_cast<int32>(State->DensityLevel) + 1))
	                       : nullptr;
	bInspectorUpgradeAvailable = bInspectorUpgradeAvailable && Next;
	SharedUpgradeLabel = FText::GetEmpty();
	const int32 ExtraDetailStart = Detail.Len();
	if (Station)
	{
		const FString Label =
		    Next                                                  ? FString::Printf(TEXT("%s  $%lld"),
                                   StationUpgradePreviewEntity == InspectedEntityId ? TEXT("CONFIRM UPGRADE")
		                                                                                                                             : TEXT("UPGRADE"),
		                                                                            Next->UpgradeCost)
		    : State->HasCivicUpgrade()                            ? TEXT("UPGRADING")
		    : State->Stage == ECityDevelopmentStage::Construction ? TEXT("BUILDING")
		                                                          : TEXT("FULLY UPGRADED");
		SharedUpgradeLabel = FText::FromString(Label);
		InspectorUpgradeLabel->SetText(SharedUpgradeLabel);
		if (State->HasCivicUpgrade() || StationUpgradePreviewEntity == InspectedEntityId)
			Detail += TEXT("\n") + Simulation->DescribeStationUpgrade(InspectedEntityId);
		if (StationUpgradePreviewEntity == InspectedEntityId)
			Detail += TEXT("\nConfirm below to purchase; Cancel dismisses");
	}
	if (bDesktopUi && SharedInspectionSections.IsValidIndex(4) && Detail.Len() > ExtraDetailStart)
		SharedInspectionSections[4].Detail =
		    FText::FromString(SharedInspectionSections[4].Detail.ToString() + Detail.Mid(ExtraDetailStart));
	const FString ConditionCause = FCityConditionIndicators::Explain(*Simulation, InspectedEntityId);
	if (!bDesktopUi && !ConditionCause.IsEmpty())
		Detail = ConditionCause + TEXT("\n") + Detail;
	const FString Text = WrapInspectionText(Detail);
	SharedInspectionDetail = Detail;
	SharedInspectionTitle = FText::FromString(Definition ? Definition->DisplayName : TEXT("Building"));
	TArray<FString> DetailLines;
	Detail.ParseIntoArrayLines(DetailLines);
	const FString Density = State && State->DensityLevel == ECityDensityLevel::High     ? TEXT("High")
	                        : State && State->DensityLevel == ECityDensityLevel::Medium ? TEXT("Medium")
	                                                                                    : TEXT("Low");
	SharedInspectionSummary = FText::FromString(
	    (bDesktopUi ? Density + (Station ? TEXT(" facility stage | ") : TEXT(" density | ")) : FString()) +
	    (DetailLines.Num() > 1 ? DetailLines[1] : FString()));
	SharedInspectionGauges.Reset();
	for (const auto& Metric : Metrics)
	{
		FCityUiGauge Gauge;
		Gauge.Label = FText::FromString(Metric.Label);
		Gauge.Value = FText::FromString(FString::Printf(TEXT("%lld / %lld"), Metric.Value, Metric.Maximum));
		Gauge.Fill =
		    Metric.Maximum > 0 ? FMath::Clamp(static_cast<double>(Metric.Value) / Metric.Maximum, 0., 1.) : 0;
		Gauge.Color = FLinearColor(.32f, .75f, .62f);
		SharedInspectionGauges.Add(Gauge);
	}
	if (Text != LastInspectionText)
	{
		LastInspectionText = Text;
		InspectorText->SetText(FText::FromString(Text));
	}
	const FVector TextSize = InspectorText->GetTextLocalSize();
	const double Width = FMath::Max(28.0, TextSize.Y + 3.0);
	const double Height =
	    FMath::Max(5.0, TextSize.Z + 3.0 + FMath::Min(Metrics.Num(), InspectorMetricRails.Num()) * 3.5 +
	                        (Station ? 7.0 : 0.0));
	InspectorHalfWidth = Width * 0.5;
	InspectorHalfHeight = Height * 0.5;
	InspectorUpgradeButton->SetRelativeLocation(FVector(0.8, 0, -Height * 0.5 + 3.5));
	InspectorUpgradeButton->SetRelativeScale3D(FVector(0.006, (Width - 4.0) / 100.0, 0.044));
	InspectorUpgradeLabel->SetRelativeLocation(FVector(1.3, 0, -Height * 0.5 + 3.5));
	SharedInspectionMetrics.Reset();
	for (int32 Index = 0; Index < InspectorMetricRails.Num(); ++Index)
	{
		const bool bActive = Metrics.IsValidIndex(Index);
		InspectorMetricRails[Index]->SetVisibility(bActive);
		InspectorMetricLabels[Index]->SetVisibility(bActive);
		InspectorMetricFills[Index]->SetVisibility(bActive && Metrics[Index].Maximum > 0 &&
		                                           Metrics[Index].Value > 0);
		if (!bActive)
			continue;
		const auto& Metric = Metrics[Index];
		const double Fraction =
		    Metric.Maximum > 0 ? FMath::Clamp(static_cast<double>(Metric.Value) / Metric.Maximum, 0.0, 1.0)
		                       : 0.0;
		const double Z = Height * 0.5 - 2.0 - TextSize.Z - Index * 3.5;
		const FString Label =
		    FString::Printf(TEXT("%s  %lld / %lld"), *Metric.Label, Metric.Value, Metric.Maximum);
		SharedInspectionMetrics += TEXT("\n") + Label;
		if (InspectorMetricLabels[Index]->Text.ToString() != Label)
			InspectorMetricLabels[Index]->SetText(FText::FromString(Label));
		InspectorMetricLabels[Index]->SetRelativeLocation(FVector(1, 0, Z));
		InspectorMetricRails[Index]->SetRelativeLocation(FVector(0.6, 0, Z - 1.8));
		InspectorMetricRails[Index]->SetRelativeScale3D(FVector(0.003, (Width - 4.0) / 100.0, 0.004));
		InspectorMetricFills[Index]->SetRelativeLocation(
		    FVector(0.9, (Width - 4.0) * (1.0 - Fraction) * 0.5, Z - 1.8));
		InspectorMetricFills[Index]->SetRelativeScale3D(
		    FVector(0.003, (Width - 4.0) * FMath::Max(0.001, Fraction) / 100.0, 0.004));
	}
	// Maintain approximately constant angular text size, independent of board zoom.
	const double Scale =
	    FMath::Clamp(FVector::Distance(Roof, HeadsetCamera->GetComponentLocation()) / 80.0, 0.35, 8.0);
	const FVector Location = Roof + FVector::UpVector * (Height * 0.5 + 4.0) * Scale;
	FRotator Facing = (HeadsetCamera->GetComponentLocation() - Location).Rotation();
	Facing.Roll = 0.0;
	InspectorStatusRoot->SetWorldLocationAndRotation(Location, Facing);
	InspectorStatusRoot->SetWorldScale3D(FVector(Scale));
	InspectorText->SetRelativeLocation(FVector(1.0, 0.0, Height * 0.5 - 1.5));
	InspectorBackground->SetRelativeScale3D(FVector(0.008, Width / 100.0, Height / 100.0));
	const FVector Start = Board->GetActorTransform().TransformPosition(InspectionLocalHit);
	const FVector End =
	    InspectorStatusRoot->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, -Height * 0.5));
	InspectorLeader->SetWorldLocationAndRotation((Start + End) * 0.5, (End - Start).Rotation());
	InspectorLeader->SetWorldScale3D(
	    FVector(FVector::Distance(Start, End) / 100.0, 0.0015 * Scale, 0.0015 * Scale));
}

int32 ACityVRToolController::FindTimeControl(const FVector& RayOrigin, const FVector& RayDirection) const
{
	const FVector Normal = HeadsetStatusRoot->GetForwardVector();
	const double Denominator = FVector::DotProduct(RayDirection, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
		return INDEX_NONE;
	const double Distance =
	    FVector::DotProduct(HeadsetStatusRoot->GetComponentLocation() - RayOrigin, Normal) / Denominator;
	if (Distance < 0.0 || Distance > PaletteRayLength)
		return INDEX_NONE;
	const FVector Local = HeadsetStatusRoot->GetComponentTransform().InverseTransformPosition(
	    RayOrigin + RayDirection * Distance);
	if (FMath::Abs(Local.Z - TimeControlButtons[0]->GetRelativeLocation().Z) > 1.6f)
		return INDEX_NONE;
	const int32 Index = FMath::RoundToInt((Local.Y + 6.75f) / 4.5f);
	return Index >= 0 && Index < 4 && FMath::Abs(Local.Y - (-6.75f + Index * 4.5f)) < 1.8f ? Index
	                                                                                       : INDEX_NONE;
}

void ACityVRToolController::ExecuteTimeControl(const int32 ControlIndex)
{
	if (ControlIndex == 0)
		TogglePause();
	else if (ControlIndex == 1)
		SetSpeed1();
	else if (ControlIndex == 2)
		SetSpeed3();
	else if (ControlIndex == 3)
		SetSpeed5();
}

void ACityVRToolController::UpdateBoardManipulation()
{
	if (!bManipulationSession || !Board || !LeftGrip || !RightGrip)
	{
		return;
	}

	const bool bLeftTracked = LeftGrip->IsTracked();
	const bool bRightTracked = RightGrip->IsTracked();
	if (BoardManipulator.Update(bLeftGripHeld, bRightGripHeld, bLeftTracked, bRightTracked,
	                            LeftGrip->GetComponentLocation(), RightGrip->GetComponentLocation(),
	                            BoardPresentationPose))
	{
		Board->SetActorTransform(BoardPresentationPose.ToTransform());
	}
}

FVector ACityVRToolController::GetPaletteButtonLocation(const int32 PaletteIndex,
                                                        const ECityPaletteAction Action) const
{
	return GetPaletteButtonRect(PaletteIndex, Action).Center;
}
int32 ACityVRToolController::FindPaletteButton(const FVector& RayOrigin, const FVector& RayDirection) const
{
	const FVector Normal = PaletteRoot->GetForwardVector();
	const double Denominator = FVector::DotProduct(RayDirection, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
	{
		return INDEX_NONE;
	}
	const double Distance =
	    FVector::DotProduct(PaletteRoot->GetComponentLocation() - RayOrigin, Normal) / Denominator;
	if (Distance < 0.0 || Distance > PaletteRayLength)
	{
		return INDEX_NONE;
	}
	const FVector Local =
	    PaletteRoot->GetComponentTransform().InverseTransformPosition(RayOrigin + RayDirection * Distance);
	for (int32 Index = 0; Index < VisiblePaletteEntries.Num(); ++Index)
	{
		if (GetPaletteButtonRect(Index, VisiblePaletteEntries[Index].Action).Contains(Local))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool ACityVRToolController::IsPaletteSurfaceHit(const FVector& RayOrigin, const FVector& RayDirection) const
{
	const FVector Normal = PaletteRoot->GetForwardVector();
	const double Denominator = FVector::DotProduct(RayDirection, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const double Distance =
	    FVector::DotProduct(PaletteRoot->GetComponentLocation() - RayOrigin, Normal) / Denominator;
	if (Distance < 0.0 || Distance > PaletteRayLength)
	{
		return false;
	}
	const FVector Local =
	    PaletteRoot->GetComponentTransform().InverseTransformPosition(RayOrigin + RayDirection * Distance);
	return FCityPaletteModel::SurfaceRect(PalettePage, UsesHandPalette()).Contains(Local);
}

void ACityVRToolController::SetPaletteOpen(const bool bOpen)
{
	if (!bOpen)
		DesktopVideo.Cancel();
	if (!bOpen)
	{
		bMainMenuPinned = false;
		bSettingsFromMainMenu = false;
		NewCityDraft = FCityNewCitySetup();
		EntryMenuStatus.Reset();
	}
	if (bPaletteOpen && !bOpen)
	{
		SuppressedTriggerMask |= ActionButtonMask & (InputLeftTrigger | InputRightTrigger);
		EndTaxDrag();
	}
	if (!bOpen && PalettePage == ECityPalettePage::Taxes)
	{
		CancelTaxEdit();
	}
	bPaletteOpen = bOpen;
	if (bOpen)
		StationUpgradePreviewEntity = 0;
	if (bOpen)
	{
		bPalettePoseCaptured = false;
		UpdatePaletteAttachment();
		SetPalettePage(ECityPalettePage::Root);
	}
	PaletteBackground->SetVisibility(bOpen);
	for (UStaticMeshComponent* Button : PaletteButtons)
	{
		Button->SetVisibility(bOpen);
	}
	for (UTextRenderComponent* Label : PaletteLabels)
	{
		Label->SetVisibility(bOpen);
	}
	for (UStaticMeshComponent* Icon : PaletteIcons)
	{
		Icon->SetVisibility(false);
	}
	for (UStaticMeshComponent* Fill : PaletteSliderFills)
	{
		Fill->SetVisibility(false);
	}
	for (UStaticMeshComponent* Thumb : PaletteSliderThumbs)
	{
		Thumb->SetVisibility(false);
	}
	PaletteTitle->SetVisibility(bOpen);
	PaletteHelp->SetVisibility(bOpen);
	InspectorStatusRoot->SetVisibility(false, true);
	InspectorLeader->SetVisibility(false);
	SummaryText->SetVisibility(bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId && !bPaletteOpen && !bEntrySceneActive);
	NeedsText->SetVisibility(bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId && !bPaletteOpen && !bEntrySceneActive);
	NeedsBackground->SetVisibility(bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId && !bPaletteOpen && !bEntrySceneActive);
	for (UStaticMeshComponent* Rail : NeedsCapacityRails)
		Rail->SetVisibility(false);
	for (UStaticMeshComponent* Fill : NeedsCapacityFills)
		Fill->SetVisibility(false);
	for (UTextRenderComponent* Label : NeedsCapacityLabels)
		Label->SetVisibility(false);
	for (UStaticMeshComponent* Rail : NeedsDemandRails)
		Rail->SetVisibility(false);
	for (UStaticMeshComponent* Fill : NeedsDemandFills)
		Fill->SetVisibility(false);
	for (UStaticMeshComponent* Marker : NeedsDemandZeroMarkers)
		Marker->SetVisibility(false);
	for (UTextRenderComponent* Label : NeedsDemandLabels)
		Label->SetVisibility(false);
	if (!bOpen)
	{
		HoveredPaletteIndex = INDEX_NONE;
		bPaletteSurfaceFocused = false;
	}
	if (GetWorld())
		UpdateNeedsStatusBars();
	RefreshNeedsVisibility();
	RefreshPaletteText();
}

void ACityVRToolController::RefreshPaletteText()
{
	for (int32 IconIndex = 0; IconIndex < PaletteIconDynamics.Num(); ++IconIndex)
	{
		if (UMaterialInstanceDynamic* Icon = PaletteIconDynamics[IconIndex])
		{
			Icon->SetVectorParameterValue(TEXT("IconTint"), IconIndex == 3 ? FLinearColor(1.0f, 0.18f, 0.1f)
			                                                               : FLinearColor::White);
		}
	}
	const bool bHandPalette = UsesHandPalette();
	const FCityPaletteRect Surface = FCityPaletteModel::SurfaceRect(PalettePage, bHandPalette);
	PaletteBackground->SetRelativeLocation(Surface.Center);
	PaletteBackground->SetRelativeScale3D(
	    FVector(0.01f, Surface.HalfSize.X / 50.0, Surface.HalfSize.Y / 50.0));
	PaletteTitle->SetRelativeLocation(
	    bHandPalette ? FVector(2.0, -15.0, 17.0)
	                 : FVector(2.0, 0.0, PalettePage == ECityPalettePage::Root ? 5.0 : 28.0));
	PaletteTitle->SetHorizontalAlignment(bHandPalette ? EHTA_Left : EHTA_Center);
	PaletteHelp->SetRelativeLocation(bHandPalette ? FVector(2.0, -15.0, -17.5) : FVector(2.0, 0.0, -5.0));
	PaletteHelp->SetHorizontalAlignment(bHandPalette ? EHTA_Left : EHTA_Center);
	PaletteHelp->SetWorldSize(bHandPalette ? 1.35f : 1.2f);
	const bool bEntryPage = FCityPaletteModel::IsEntryPage(PalettePage);
	if (bEntryPage)
	{
		PaletteTitle->SetRelativeLocation(FVector(2, 0, 29));
		PaletteTitle->SetHorizontalAlignment(EHTA_Center);
		PaletteTitle->SetWorldSize(bHandPalette ? 1.6f : 2.0f);
		PaletteHelp->SetRelativeLocation(FVector(2, 0, 23));
		PaletteHelp->SetHorizontalAlignment(EHTA_Center);
		PaletteHelp->SetWorldSize(1.2f);
	}
	else
		PaletteTitle->SetWorldSize(1.7f);
	if (bEntryPage && PalettePage != ECityPalettePage::MainMenu)
	{
		PaletteTitle->SetRelativeLocation(FVector(2, 0, 38));
		PaletteHelp->SetRelativeLocation(FVector(2, -15, 29));
		PaletteHelp->SetWorldSize(1.05f);
	}
	UpdateTerrainPreview();
	PaletteTitle->SetText(FText::FromString(FCityPaletteModel::PageTitle(PalettePage)));
	if (PalettePage == ECityPalettePage::Reports || PalettePage == ECityPalettePage::DisasterHistory)
		PaletteTitle->SetRelativeLocation(FVector(2, 0, 53));
	if (PalettePage == ECityPalettePage::QuarterlyNews || PalettePage == ECityPalettePage::Loans ||
	    PalettePage == ECityPalettePage::LoanReview || PalettePage == ECityPalettePage::Policies ||
	    PalettePage == ECityPalettePage::CommercialAi || PalettePage == ECityPalettePage::TechnologyReview || PalettePage == ECityPalettePage::PolicyReview)
	{
		PaletteTitle->SetRelativeLocation(FVector(2, 0, 84.5));
		PaletteTitle->SetHorizontalAlignment(EHTA_Center);
		PaletteHelp->SetRelativeLocation(FVector(2, 0, -5));
		PaletteHelp->SetHorizontalAlignment(EHTA_Center);
	}
	RefreshReports();
	FString Help = GetPaletteHelp();
	const bool bConstructionHelp =
	    VisiblePaletteEntries.IsValidIndex(HoveredPaletteIndex) &&
	    !FCityPaletteModel::ConstructionDefinition(VisiblePaletteEntries[HoveredPaletteIndex].Action)
	         .IsNone();
	if (bConstructionHelp)
	{
		const int32 Wrap = bHandPalette ? 40 : 70;
		TArray<FString> Lines;
		Help.ParseIntoArrayLines(Lines, false);
		Help.Reset();
		for (FString Line : Lines)
		{
			if (!Help.IsEmpty())
				Help += TEXT("\n");
			while (Line.Len() > Wrap)
			{
				int32 Split = Wrap;
				while (Split > 0 && Line[Split] != TEXT(' '))
					--Split;
				if (!Split)
					Split = Wrap;
				Help += Line.Left(Split) + TEXT("\n");
				Line = Line.Mid(Split).TrimStart();
			}
			Help += Line;
		}
	}
	PaletteHelp->SetText(FText::FromString(Help));
	if (bConstructionHelp)
	{
		const FBox HelpBounds = PaletteHelp->CalcBounds(PaletteHelp->GetRelativeTransform()).GetBox();
		const double MinY = FMath::Min(Surface.Center.Y - Surface.HalfSize.X, HelpBounds.Min.Y - 1.0);
		const double MaxY = FMath::Max(Surface.Center.Y + Surface.HalfSize.X, HelpBounds.Max.Y + 1.0);
		const double MinZ = FMath::Min(Surface.Center.Z - Surface.HalfSize.Y, HelpBounds.Min.Z - 1.0);
		const double MaxZ = FMath::Max(Surface.Center.Z + Surface.HalfSize.Y, HelpBounds.Max.Z + 1.0);
		PaletteBackground->SetRelativeLocation(FVector(0, (MinY + MaxY) * .5, (MinZ + MaxZ) * .5));
		PaletteBackground->SetRelativeScale3D(FVector(.01, (MaxY - MinY) / 100.0, (MaxZ - MinZ) / 100.0));
	}

	for (int32 Index = 0; Index < PaletteLabels.Num(); ++Index)
	{
		const bool bVisible = bPaletteOpen && VisiblePaletteEntries.IsValidIndex(Index);
		PaletteLabels[Index]->SetVisibility(false);
		PaletteIcons[Index]->SetVisibility(false);
		PaletteSliderFills[Index]->SetVisibility(false);
		PaletteSliderThumbs[Index]->SetVisibility(false);
		PaletteButtons[Index]->SetVisibility(bVisible);
		if (!bVisible)
		{
			continue;
		}
		const FCityPaletteEntry& Entry = VisiblePaletteEntries[Index];
		const FCityPaletteRect Rect = GetPaletteButtonRect(Index, Entry.Action);
		const FVector EntryLocation = GetPaletteButtonLocation(Index, Entry.Action);
		PaletteButtons[Index]->SetRelativeLocation(EntryLocation);
		PaletteLabels[Index]->SetRelativeLocation(FVector(2.0f, EntryLocation.Y, EntryLocation.Z));
		PaletteIcons[Index]->SetRelativeLocation(FVector(2.1f, EntryLocation.Y, EntryLocation.Z));
		const FCityUiActionView View = GetPaletteEntryView(Index);
		PaletteLabels[Index]->SetText(View.Label);
		PaletteLabels[Index]->SetWorldSize(
		    !bHandPalette && !bEntrySceneActive && Index < FCityPaletteModel::BarEntryCount ? 1.3f : 1.65f);
		if (bEntryPage)
			PaletteLabels[Index]->SetWorldSize(bHandPalette ? 1.2f : 1.35f);
		if (Entry.Action == ECityPaletteAction::SelectNeighborhoodPark ||
		    Entry.Action == ECityPaletteAction::SelectBasketballPark)
			PaletteLabels[Index]->SetWorldSize(1.2f);
		const bool bHovered = Index == HoveredPaletteIndex;
		const bool bSelected = View.bSelected;
		const bool bDisabled = !View.bEnabled;
		const bool bDestructive =
		    Entry.Action == ECityPaletteAction::Reset || Entry.Action == ECityPaletteAction::SelectDemolish;
		while (!PaletteButtonMaterials.IsValidIndex(Index))
			PaletteButtonMaterials.Add(
			    PaletteButtons[PaletteButtonMaterials.Num()]->CreateDynamicMaterialInstance(0));
		if (PaletteButtonMaterials[Index])
			PaletteButtonMaterials[Index]->SetVectorParameterValue(
			    TEXT("BarColor"),
			    View.bTutorialAttention ? FLinearColor(.12f + .12f * CityTutorialAttention::Pulse(FPlatformTime::Seconds()), .18f, .035f)
		    : bDisabled   ? FLinearColor(.02f, .025f, .03f)
			    : bSelected ? (bDestructive ? FLinearColor(.7f, .12f, .025f) : FLinearColor(.025f, .3f, .65f))
			    : bHovered  ? FLinearColor(.12f, .2f, .3f)
			                : FLinearColor(.025f, .035f, .05f));
		const bool bResidentialTrack = Entry.Action == ECityPaletteAction::TaxResidentialTrack;
		const bool bCommercialTrack = Entry.Action == ECityPaletteAction::TaxCommercialTrack;
		const bool bIndustrialTrack = Entry.Action == ECityPaletteAction::TaxIndustrialTrack;
		const bool bSliderTrack = bResidentialTrack || bCommercialTrack || bIndustrialTrack;
		UMaterialInterface* IconMaterial = Entry.bIconOnly ? GetPaletteIconMaterial(Entry.Action) : nullptr;
		const bool bShowIcon = IconMaterial != nullptr;
		PaletteLabels[Index]->SetVisibility(!bShowIcon);
		PaletteIcons[Index]->SetVisibility(bShowIcon);
		if (bShowIcon)
		{
			PaletteIcons[Index]->SetMaterial(0, IconMaterial);
			PaletteIcons[Index]->SetRelativeScale3D(FVector(bHovered ? 0.032f : bSelected ? 0.029f : 0.026f));
			if (UMaterialInstanceDynamic* IconDynamic = Cast<UMaterialInstanceDynamic>(IconMaterial))
			{
				IconDynamic->SetVectorParameterValue(
				    TEXT("IconTint"), bHovered                     ? FLinearColor(1.0f, 0.75f, 0.05f)
				                      : bDestructive && !bSelected ? FLinearColor(1.0f, 0.18f, 0.1f)
				                                                   : FLinearColor::White);
			}
		}
		PaletteLabels[Index]->SetTextRenderColor(bDisabled                    ? FColor(90, 90, 90)
		                                         : bHovered                   ? FColor::Yellow
		                                         : bDestructive && !bSelected ? FColor(255, 100, 80)
		                                                                      : FColor::White);
		if (bSliderTrack)
		{
			const FCityBoundedValueControl& Control = bResidentialTrack  ? ResidentialTaxControl
			                                          : bCommercialTrack ? CommercialTaxControl
			                                                             : IndustrialTaxControl;
			const float Normalized = static_cast<float>(Control.GetNormalizedPosition());
			const FVector ButtonLocation = EntryLocation;
			// Palette-local +Y is screen-left from the player's view. Start the
			// value range there so the conventional minimum-left/maximum-right
			// direction survives the viewer-facing palette transform.
			const float TrackLeft = ButtonLocation.Y + PaletteButtonHalfWidth;
			const float ThumbY = static_cast<float>(FCityHorizontalSliderPresentation::LocalYFromNormalized(
			    Normalized, ButtonLocation.Y, PaletteButtonHalfWidth));
			PaletteButtons[Index]->SetRelativeScale3D(FVector(0.005f, 0.135f, 0.004f));
			PaletteLabels[Index]->SetRelativeLocation(
			    FVector(2.0f, ButtonLocation.Y, ButtonLocation.Z + 1.25f));
			PaletteLabels[Index]->SetWorldSize(1.2f);
			UStaticMeshComponent* Fill = PaletteSliderFills[Index];
			UStaticMeshComponent* Thumb = PaletteSliderThumbs[Index];
			UMaterialInterface* SliderMaterial = bResidentialTrack  ? ResidentialDemandMaterial.Get()
			                                     : bCommercialTrack ? CommercialDemandMaterial.Get()
			                                                        : IndustrialDemandMaterial.Get();
			Fill->SetMaterial(0, SliderMaterial);
			Thumb->SetMaterial(0, SliderMaterial);
			Fill->SetVisibility(Normalized > 0.0f);
			Fill->SetRelativeLocation(FVector(1.7f, (TrackLeft + ThumbY) * 0.5f, ButtonLocation.Z));
			Fill->SetRelativeScale3D(FVector(0.006f, 0.135f * Normalized, 0.007f));
			Thumb->SetVisibility(true);
			Thumb->SetRelativeLocation(FVector(2.0f, ThumbY, ButtonLocation.Z));
			Thumb->SetRelativeScale3D(
			    FVector(0.008f, bHovered ? 0.019f : 0.015f, bHovered ? 0.024f : 0.020f));
		}
		else
		{
			PaletteLabels[Index]->SetRelativeLocation(FVector(2.0f, EntryLocation.Y, EntryLocation.Z));
			PaletteButtons[Index]->SetRelativeScale3D(FVector(bDisabled   ? 0.006f
			                                                  : bHovered  ? 0.014f
			                                                  : bSelected ? 0.011f
			                                                              : 0.008f,
			                                                  Rect.HalfSize.X / 50.0,
			                                                  Rect.HalfSize.Y / 50.0));
		}
	}
}

void ACityVRToolController::UpdateNeedsStatusBars()
{
	const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	const bool bShow = bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId && !bPaletteOpen && !bEntrySceneActive && Simulation != nullptr;
	for (int32 Index = 0; Index < NeedsCapacityRails.Num(); ++Index)
	{
		const bool bActiveCategory = bShow && Index < 5;
		NeedsCapacityRails[Index]->SetVisibility(bActiveCategory);
		NeedsCapacityLabels[Index]->SetVisibility(bActiveCategory);
		NeedsCapacityFills[Index]->SetVisibility(false);
	}
	for (int32 Index = 0; Index < NeedsDemandRails.Num(); ++Index)
	{
		const bool bActiveCategory = bShow && Index < 3;
		NeedsDemandRails[Index]->SetVisibility(bActiveCategory);
		NeedsDemandZeroMarkers[Index]->SetVisibility(bActiveCategory);
		NeedsDemandLabels[Index]->SetVisibility(bActiveCategory);
		NeedsDemandFills[Index]->SetVisibility(false);
	}
	if (!bShow)
	{
		return;
	}

	const FCityNeedsSnapshot& Needs = Simulation->GetNeedsSnapshot();
	// Budget recovery text may grow; keep all bars below the rendered text.
	const float TextBottom =
	    static_cast<float>(NeedsText->CalcBounds(NeedsText->GetRelativeTransform()).GetBox().Min.Z);
	const float BarOffset = FMath::Min(0.0f, TextBottom - 2.0f - CapacityBarTopZ);
	const int32 CapacityValues[5] = {Needs.OccupiedResidents, Needs.EmployedJobs, Needs.PowerUsage,
	                                 Needs.ServiceUsage, Needs.GoodsSold};
	const int32 CapacityLimits[5] = {Needs.HousingCapacity, Needs.JobCapacity, Needs.PowerCapacity,
	                                 Needs.ServiceCapacity, Needs.HouseholdGoodsDemand};
	UMaterialInterface* CapacityMaterials[5] = {
	    ResidentialDemandMaterial.Get(), CommercialDemandMaterial.Get(), IndustrialDemandMaterial.Get(),
	    CommercialDemandMaterial.Get(), IndustrialDemandMaterial.Get()};
	const TCHAR* CapacityNames[5] = {TEXT("HOME"), TEXT("JOBS"), TEXT("POWER"), TEXT("WATER"), TEXT("GOODS")};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const double Normalized =
		    FCityNeedsPresentation::NormalizeRatioValue(CapacityValues[Index], CapacityLimits[Index]);
		const float StartY = DemandBarCenterY + DemandBarHalfWidth;
		const float EndY = StartY - static_cast<float>(Normalized) * DemandBarHalfWidth * 2.0f;
		const float RowZ = CapacityBarTopZ + BarOffset - Index * CapacityBarRowSpacing;
		NeedsCapacityRails[Index]->SetRelativeLocation(FVector(0.4f, DemandBarCenterY, RowZ));
		NeedsCapacityLabels[Index]->SetRelativeLocation(FVector(1.1f, -2.7f, RowZ));
		UStaticMeshComponent* Fill = NeedsCapacityFills[Index];
		Fill->SetMaterial(0, CapacityMaterials[Index]);
		Fill->SetRelativeLocation(FVector(0.8f, (StartY + EndY) * 0.5f, RowZ));
		Fill->SetRelativeScale3D(FVector(0.006f, FMath::Abs(EndY - StartY) / 100.0f, 0.009f));
		Fill->SetVisibility(Normalized > 0.0);
		NeedsCapacityLabels[Index]->SetText(FText::FromString(FString::Printf(
		    TEXT("%s %d/%d"), CapacityNames[Index], CapacityValues[Index], CapacityLimits[Index])));
	}

	const int32 Values[3] = {Needs.ResidentialDemandBalance, Needs.CommercialDemandBalance,
	                         Needs.IndustrialDemandBalance};
	UMaterialInterface* Materials[3] = {ResidentialDemandMaterial.Get(), CommercialDemandMaterial.Get(),
	                                    IndustrialDemandMaterial.Get()};
	const TCHAR* Names[3] = {TEXT("R"), TEXT("C"), TEXT("I")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const double Normalized =
		    FCityNeedsPresentation::NormalizeSignedValue(Values[Index], DemandVisualBound);
		const float EndY = DemandBarCenterY - static_cast<float>(Normalized) * DemandBarHalfWidth;
		const float RowZ = DemandBarTopZ + BarOffset - Index * DemandBarRowSpacing;
		NeedsDemandRails[Index]->SetRelativeLocation(FVector(0.4f, DemandBarCenterY, RowZ));
		NeedsDemandLabels[Index]->SetRelativeLocation(FVector(1.1f, -2.7f, RowZ));
		NeedsDemandZeroMarkers[Index]->SetRelativeLocation(FVector(1.0f, DemandBarCenterY, RowZ));
		UStaticMeshComponent* Fill = NeedsDemandFills[Index];
		Fill->SetMaterial(0, Materials[Index]);
		Fill->SetRelativeLocation(FVector(0.8f, (DemandBarCenterY + EndY) * 0.5f, RowZ));
		Fill->SetRelativeScale3D(FVector(0.006f, FMath::Abs(EndY - DemandBarCenterY) / 100.0f, 0.009f));
		Fill->SetVisibility(!FMath::IsNearlyZero(Normalized));
		NeedsDemandLabels[Index]->SetText(FText::FromString(
		    FString::Printf(TEXT("%s %s%d"), Names[Index], Values[Index] >= 0 ? TEXT("+") : TEXT("-"),
		                    FMath::Abs(Values[Index]))));
	}
}

int32 ACityVRToolController::GetPaletteIconIndex(const ECityPaletteAction Action) const
{
	switch (Action)
	{
	case ECityPaletteAction::Save:
		return 0;
	case ECityPaletteAction::Load:
		return 1;
	case ECityPaletteAction::Start:
		return 2;
	case ECityPaletteAction::Reset:
		return 3;
	case ECityPaletteAction::Back:
		return 4;
	case ECityPaletteAction::OpenSettings:
		return 5;
	default:
		return INDEX_NONE;
	}
}

UMaterialInterface* ACityVRToolController::GetPaletteIconMaterial(const ECityPaletteAction Action) const
{
	const int32 IconIndex = GetPaletteIconIndex(Action);
	if (PaletteIconDynamics.IsValidIndex(IconIndex) && PaletteIconDynamics[IconIndex])
	{
		return PaletteIconDynamics[IconIndex];
	}
	return PaletteIconMaterials.IsValidIndex(IconIndex) ? PaletteIconMaterials[IconIndex].Get() : nullptr;
}

void ACityVRToolController::ExecutePaletteAction(const int32 PaletteIndex)
{
	if (!VisiblePaletteEntries.IsValidIndex(PaletteIndex))
	{
		return;
	}
	ExecuteSharedPaletteAction(VisiblePaletteEntries[PaletteIndex].Action);
}

void ACityVRToolController::ExecuteSharedPaletteAction(const ECityPaletteAction Action)
{
	if (IsPaletteActionDisabled(Action))
	{
		return;
	}
	switch (Action)
	{
	case ECityPaletteAction::ReportDisasters:
		ReportPeriodIndex = INDEX_NONE;
		SetPalettePage(ECityPalettePage::DisasterHistory);
		break;
	case ECityPaletteAction::TestEarthquake:
		Board->StartNaturalDisasterTest(false);
		break;
	case ECityPaletteAction::TestTornado:
		Board->StartNaturalDisasterTest(true);
		break;
	case ECityPaletteAction::OpenDisasters:
		SetPalettePage(ECityPalettePage::Disasters);
		break;
	case ECityPaletteAction::CycleDisasters:
		NewCityIdentity.DisasterFrequency = static_cast<ECityDisasterFrequency>(
		    (static_cast<uint8>(NewCityIdentity.DisasterFrequency) + 1) % 4);
		EntryMenuStatus.Reset();
		break;
	case ECityPaletteAction::DisastersOff:
	case ECityPaletteAction::DisastersLow:
	case ECityPaletteAction::DisastersMedium:
	case ECityPaletteAction::DisastersHigh:
		GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->SetDisasterFrequency(
		    Action == ECityPaletteAction::DisastersOff    ? ECityDisasterFrequency::Off
		    : Action == ECityPaletteAction::DisastersLow  ? ECityDisasterFrequency::Low
		    : Action == ECityPaletteAction::DisastersHigh ? ECityDisasterFrequency::High
		                                                  : ECityDisasterFrequency::Medium);
		break;
	case ECityPaletteAction::OpenMainMenu:
		ShowMainMenu();
		break;
	case ECityPaletteAction::ContinueCity:
		ReturnToCity();
		break;
	case ECityPaletteAction::OpenNewCitySetup:
		ShowNewCitySetup();
		break;
	case ECityPaletteAction::ReviewLoad:
		ReviewCityLoad();
		break;
	case ECityPaletteAction::ConfirmLoad:
		ConfirmReviewedCityLoad();
		break;
	case ECityPaletteAction::ToggleArchivedCities:
		bReviewArchivedCities = !bReviewArchivedCities;
		RefreshCityLibrary();
		break;
	case ECityPaletteAction::RefreshCityLibrary:
		RefreshCityLibrary();
		break;
	case ECityPaletteAction::ReviewArchive:
		ReviewSelectedCityArchive();
		break;
	case ECityPaletteAction::ConfirmArchive:
		ConfirmSelectedCityArchive();
		break;
	case ECityPaletteAction::RestoreCity:
		RestoreSelectedCity();
		break;
	case ECityPaletteAction::RerollTerrain:
		RerollNewCityTerrain();
		break;
	case ECityPaletteAction::ReviewCreate:
		ReviewCityCreation();
		break;
	case ECityPaletteAction::ConfirmCreate:
		ConfirmReviewedCityCreation();
		break;
	case ECityPaletteAction::PreviousCity:
	case ECityPaletteAction::NextCity:
		if (!ReviewedCitySlots.IsEmpty())
			ReviewedCityIndex = (ReviewedCityIndex + ReviewedCitySlots.Num() +
			                     (Action == ECityPaletteAction::NextCity ? 1 : -1)) %
			                    ReviewedCitySlots.Num();
		EntryMenuStatus.Reset();
		break;
	case ECityPaletteAction::SaveAndBack:
		if (auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			const bool bSaved = Simulation->SaveCity();
			EntryMenuStatus = bSaved
			                      ? TEXT("Current city saved; no city was loaded")
			                      : TEXT("Save failed; current city retained\nCheck local storage or cancel");
			if (bSaved)
				SetPalettePage(ECityPalettePage::MainMenu);
		}
		break;
	case ECityPaletteAction::DifficultyRelaxed:
	case ECityPaletteAction::DifficultyNormal:
	case ECityPaletteAction::DifficultyChallenging:
		NewCityDraft = FCityNewCitySetup::Proposal(
		    Action == ECityPaletteAction::DifficultyRelaxed       ? ECityDifficultyDraft::Relaxed
		    : Action == ECityPaletteAction::DifficultyChallenging ? ECityDifficultyDraft::Challenging
		                                                          : ECityDifficultyDraft::Normal,
		    GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetEconomyTuning());
		break;
	case ECityPaletteAction::CycleMapSize:
		NewCityTerrain.Dimensions = NewCityTerrain.Dimensions.GetWidth() == 64 ? FCityGridDimensions(80, 80)
		                                                                       : FCityGridDimensions(64, 64);
		EntryMenuStatus.Reset();
		break;
	case ECityPaletteAction::CycleAnchor:
		CyclePaletteAnchor();
		break;
	case ECityPaletteAction::ToggleDebugInfo:
		SetDebugModeEnabled(!bDebugInfoVisible);
		break;
	case ECityPaletteAction::ToggleConditionIndicators:
		SetConditionIndicatorsVisible(!bConditionIndicatorsVisible);
		break;
	case ECityPaletteAction::FocusNextCondition:
		FocusNextCondition();
		break;
	case ECityPaletteAction::LightingAutomatic:
		Board->SetLightingPreset(ECityLightingPreset::Automatic);
		break;
	case ECityPaletteAction::LightingDay:
		Board->SetLightingPreset(ECityLightingPreset::Day);
		break;
	case ECityPaletteAction::LightingDusk:
		Board->SetLightingPreset(ECityLightingPreset::Dusk);
		break;
	case ECityPaletteAction::LightingNight:
		Board->SetLightingPreset(ECityLightingPreset::Night);
		break;
	case ECityPaletteAction::OpenBuild:
		SetPalettePage(ECityPalettePage::Build);
		break;
	case ECityPaletteAction::OpenRoads:
		SetPalettePage(ECityPalettePage::Roads);
		break;
	case ECityPaletteAction::OpenParks:
		SetPalettePage(ECityPalettePage::Parks);
		break;
    case ECityPaletteAction::OpenCommercialAi:
        SetPalettePage(ECityPalettePage::CommercialAi);
        break;
    case ECityPaletteAction::EnableCommercialAi:
    case ECityPaletteAction::DisableCommercialAi:
        if (auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
            Sim->SetCommercialAiPilot(Action == ECityPaletteAction::EnableCommercialAi);
        RefreshReports();
        break;
	case ECityPaletteAction::OpenTechnology:
		SetPalettePage(ECityPalettePage::TechnologyReview);
		break;
	case ECityPaletteAction::OpenTechnologyFacilities:
		SetPalettePage(ECityPalettePage::TechnologyFacilities);
		break;
	case ECityPaletteAction::ReviewAIInstitute:
	case ECityPaletteAction::ReviewEnterpriseAICenter:
	case ECityPaletteAction::ReviewDigitalExchange:
	case ECityPaletteAction::ReviewComputingCenter:
	case ECityPaletteAction::ReviewResearchCampus:
		TechnologyReviewDefinition = Action == ECityPaletteAction::ReviewAIInstitute ? TEXT("AIInstitute")
                                     : Action == ECityPaletteAction::ReviewEnterpriseAICenter ? TEXT("EnterpriseAICenter")
                                     : Action == ECityPaletteAction::ReviewDigitalExchange   ? TEXT("DigitalExchange")
		                             : Action == ECityPaletteAction::ReviewComputingCenter ? TEXT("ComputingCenter")
		                                                                                   : TEXT("ResearchCampus");
		RefreshReports();
		break;
	case ECityPaletteAction::SelectDigitalExchange:
	case ECityPaletteAction::SelectComputingCenter:
	case ECityPaletteAction::SelectAIInstitute:
	case ECityPaletteAction::SelectEnterpriseAICenter:
	case ECityPaletteAction::SelectResearchCampus:
		Board->SetSelectedFacility(FCityPaletteModel::ConstructionDefinition(Action));
		break;
	case ECityPaletteAction::OpenEducationFacilities:
		SetPalettePage(ECityPalettePage::EducationFacilities);
		break;
	case ECityPaletteAction::SelectPrimarySchool:
		Board->SetSelectedFacility(TEXT("PrimarySchool"));
		break;
	case ECityPaletteAction::SelectSecondarySchool:
		Board->SetSelectedFacility(TEXT("SecondarySchool"));
		break;
	case ECityPaletteAction::OpenPowerFacilities:
		SetPalettePage(ECityPalettePage::PowerFacilities);
		break;
	case ECityPaletteAction::OpenWaterFacilities:
		SetPalettePage(ECityPalettePage::WaterFacilities);
		break;
	case ECityPaletteAction::SelectWaterTower:
		Board->SetSelectedFacility(TEXT("WaterTower"));
		break;
	case ECityPaletteAction::SelectCoalPower:
		Board->SetSelectedFacility(TEXT("CoalPower"));
		break;
	case ECityPaletteAction::SelectOilPower:
		Board->SetSelectedFacility(TEXT("OilPower"));
		break;
	case ECityPaletteAction::SelectWindPower:
		Board->SetSelectedFacility(TEXT("WindPower"));
		break;
	case ECityPaletteAction::SelectSolarPower:
		Board->SetSelectedFacility(TEXT("SolarPower"));
		break;
	case ECityPaletteAction::SelectNuclearPower:
		Board->SetSelectedFacility(TEXT("NuclearPower"));
		break;
	case ECityPaletteAction::SelectCollege:
		Board->SetSelectedFacility(TEXT("College"));
		break;
	case ECityPaletteAction::OpenZone:
		SetPalettePage(ECityPalettePage::Zone);
		break;
	case ECityPaletteAction::OpenCity:
		SetPalettePage(ECityPalettePage::City);
		break;
	case ECityPaletteAction::OpenReports:
		ShowReports(ReportPeriod);
		break;
	case ECityPaletteAction::OpenPolicies:
		PolicyMessage.Reset();
		SetPalettePage(ECityPalettePage::Policies);
		break;
	case ECityPaletteAction::ReviewReservePolicy:
	case ECityPaletteAction::ReviewBursaryPolicy:
		PendingPolicy = Action == ECityPaletteAction::ReviewReservePolicy ? ECityPolicy::ReserveAssistance
		                                                                  : ECityPolicy::SchoolBursaries;
		bPendingPolicyEnable = !GetWorld()
		                            ->GetSubsystem<UCitySimulationSubsystem>()
		                            ->GetEconomySnapshot()
		                            .Policies.Programs[static_cast<int32>(PendingPolicy)]
		                            .bEnabled;
		PolicyMessage.Reset();
		SetPalettePage(ECityPalettePage::PolicyReview);
		break;
	case ECityPaletteAction::ConfirmPolicy:
		if (PalettePage == ECityPalettePage::PolicyReview)
		{
			auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
			if (Sim && Sim->SetCityPolicy(PendingPolicy, bPendingPolicyEnable))
				PolicyMessage = TEXT("Policy choice saved; funding checked next day.");
			SetPalettePage(ECityPalettePage::Policies);
		}
		break;
	case ECityPaletteAction::OpenLoans:
		LoanMessage.Reset();
		PendingLoanAmount = 0;
		bLoanPayoffReview = false;
		SetPalettePage(ECityPalettePage::Loans);
		break;
	case ECityPaletteAction::ReviewLoanSmall:
	case ECityPaletteAction::ReviewLoanMedium:
	case ECityPaletteAction::ReviewLoanLarge:
	case ECityPaletteAction::ReviewLoanPayoff:
		LoanMessage.Reset();
		bLoanPayoffReview = Action == ECityPaletteAction::ReviewLoanPayoff;
		PendingLoanAmount = Action == ECityPaletteAction::ReviewLoanSmall    ? 5000
		                    : Action == ECityPaletteAction::ReviewLoanMedium ? 10000
		                                                                     : 20000;
		SetPalettePage(ECityPalettePage::LoanReview);
		break;
	case ECityPaletteAction::ConfirmLoan:
		if (PalettePage == ECityPalettePage::LoanReview)
		{
			auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
			if (Sim)
			{
				const bool Success = bLoanPayoffReview ? Sim->RepayCityLoan(LoanMessage)
				                                       : Sim->BorrowCityLoan(PendingLoanAmount, LoanMessage);
				if (Success)
					LoanMessage = bLoanPayoffReview ? TEXT("Full payoff transferred to civic lender.")
					                                : TEXT("Loan principal transferred to city treasury.");
				SetPalettePage(ECityPalettePage::Loans);
			}
		}
		break;
	case ECityPaletteAction::OpenQuarterlyNews:
		NewsPage = 0;
		SetPalettePage(ECityPalettePage::QuarterlyNews);
		RefreshReports();
		break;
	case ECityPaletteAction::NewsOlder:
		++NewsPage;
		break;
	case ECityPaletteAction::NewsNewer:
		NewsPage = FMath::Max(0, NewsPage - 1);
		break;
	case ECityPaletteAction::NewsMoney:
		ReportCategory = 2;
		ReportMetricIndex = 0;
		ShowReports();
		break;
	case ECityPaletteAction::OpenHousingHistory:
		HousingHistoryPage = 0;
		SetPalettePage(ECityPalettePage::HousingHistory);
		RefreshReports();
		break;
	case ECityPaletteAction::HousingOlder:
		++HousingHistoryPage;
		break;
	case ECityPaletteAction::HousingNewer:
		HousingHistoryPage = FMath::Max(0, HousingHistoryPage - 1);
		break;
	case ECityPaletteAction::ReportPeople:
		ReportCategory = 0;
		ReportMetricIndex = 0;
		break;
	case ECityPaletteAction::ReportStaff:
		ReportCategory = 1;
		ReportMetricIndex = 0;
		break;
	case ECityPaletteAction::ReportMoney:
		ReportCategory = 2;
		ReportMetricIndex = 0;
		break;
	case ECityPaletteAction::ReportGoods:
		ReportCategory = 3;
		ReportMetricIndex = 0;
		break;
	case ECityPaletteAction::ReportWaste:
		ReportCategory = 4;
		ReportMetricIndex = 5;
		ShowReports();
		RefreshReports();
		break;
	case ECityPaletteAction::ReportHealth:
		ReportCategory = 5;
		ReportMetricIndex = 0;
		ReportPeriodIndex = INDEX_NONE;
		RefreshReports();
		break;
	case ECityPaletteAction::ReportUtilities:
		ReportCategory = 4;
		ReportMetricIndex = 0;
		break;
	case ECityPaletteAction::ReportMetricPrevious:
		--ReportMetricIndex;
		break;
	case ECityPaletteAction::ReportMetricNext:
		++ReportMetricIndex;
		break;
	case ECityPaletteAction::ReportPeriodPrevious:
		ReportPeriodIndex = FMath::Max(0, ReportPeriodIndex - 1);
		break;
	case ECityPaletteAction::ReportPeriodNext:
		++ReportPeriodIndex;
		break;
	case ECityPaletteAction::ReportHourly:
	case ECityPaletteAction::ReportDaily:
	case ECityPaletteAction::ReportWeekly:
	case ECityPaletteAction::ReportMonthly:
	case ECityPaletteAction::ReportAnnual:
		ReportPeriod = Action == ECityPaletteAction::ReportHourly ? ECityReportPeriod::Hourly
		    : Action == ECityPaletteAction::ReportDaily ? ECityReportPeriod::Daily
		    : Action == ECityPaletteAction::ReportWeekly ? ECityReportPeriod::Weekly
		    : Action == ECityPaletteAction::ReportAnnual ? ECityReportPeriod::Annual : ECityReportPeriod::Monthly;
		ReportPeriodIndex = INDEX_NONE;
		break;
	case ECityPaletteAction::OpenTaxes:
		BeginTaxEdit();
		SetPalettePage(ECityPalettePage::Taxes);
		break;
	case ECityPaletteAction::OpenSettings:
		ShowGameSettings();
		break;
	case ECityPaletteAction::OpenSafetyTests:
		ShowDebugTools();
		break;
	case ECityPaletteAction::OpenAudio:
		SetPalettePage(ECityPalettePage::Audio);
		break;
	case ECityPaletteAction::OpenGraphics:
		DesktopVideo.BeginEdit();
		SetPalettePage(ECityPalettePage::Graphics);
		break;
	case ECityPaletteAction::GraphicsMode:
		DesktopVideo.CycleMode();
		SetPalettePage(ECityPalettePage::Graphics);
		break;
	case ECityPaletteAction::GraphicsPreviousResolution:
	case ECityPaletteAction::GraphicsNextResolution:
		DesktopVideo.CycleResolution(Action == ECityPaletteAction::GraphicsPreviousResolution ? -1 : 1);
		break;
	case ECityPaletteAction::GraphicsVSync:
		DesktopVideo.ToggleVSync();
		break;
	case ECityPaletteAction::GraphicsRenderScale:
		DesktopVideo.CycleRenderScale();
		break;
	case ECityPaletteAction::GraphicsAntiAliasing:
		DesktopVideo.CycleAntiAliasing();
		break;
	case ECityPaletteAction::GraphicsApply:
		DesktopVideo.Apply(FPlatformTime::Seconds());
		SetPalettePage(ECityPalettePage::Graphics);
		break;
	case ECityPaletteAction::GraphicsKeep:
		DesktopVideo.Keep();
		SetPalettePage(ECityPalettePage::Settings);
		break;
	case ECityPaletteAction::GraphicsRevert:
		DesktopVideo.Cancel();
		SetPalettePage(ECityPalettePage::Graphics);
		break;
	case ECityPaletteAction::AlarmMute:
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(0.f);
		break;
	case ECityPaletteAction::Alarm25:
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(.25f);
		break;
	case ECityPaletteAction::Alarm50:
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(.5f);
		break;
	case ECityPaletteAction::Alarm75:
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(.75f);
		break;
	case ECityPaletteAction::Alarm100:
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(1.f);
		break;
	case ECityPaletteAction::AmbienceMute:
		GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(0.f);
		break;
	case ECityPaletteAction::Ambience25:
		GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(.25f);
		break;
	case ECityPaletteAction::Ambience50:
		GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(.5f);
		break;
	case ECityPaletteAction::Ambience75:
		GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(.75f);
		break;
	case ECityPaletteAction::Ambience100:
		GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(1.f);
		break;
	case ECityPaletteAction::MusicMute:
		GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(0.f);
		break;
	case ECityPaletteAction::Music25:
		GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(.25f);
		break;
	case ECityPaletteAction::Music50:
		GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(.5f);
		break;
	case ECityPaletteAction::Music75:
		GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(.75f);
		break;
	case ECityPaletteAction::Music100:
		GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(1.f);
		break;
	case ECityPaletteAction::PreviewAlarm:
		Board->PreviewDisasterAlarm();
		break;
	case ECityPaletteAction::OpenLighting:
		SetPalettePage(ECityPalettePage::Lighting);
		break;
	case ECityPaletteAction::OpenOverlays:
		SetPalettePage(ECityPalettePage::Overlays);
		break;
	case ECityPaletteAction::Back:
		NavigatePaletteBack();
		break;
	case ECityPaletteAction::CancelTaxes:
		CancelTaxEdit();
		SetPalettePage(ECityPalettePage::City);
		break;
	case ECityPaletteAction::Close:
		SetPaletteOpen(false);
		break;
	case ECityPaletteAction::SelectInspect:
		Board->SetToolMode(ECityToolMode::Inspect);
		break;
	case ECityPaletteAction::SelectDemolish:
		Board->SetToolMode(ECityToolMode::Demolish);
		break;
	case ECityPaletteAction::SelectRoad:
		Board->SetToolMode(ECityToolMode::Road);
		break;
	case ECityPaletteAction::SelectDirtRoad:
		Board->SetSelectedRoadDefinition(TEXT("DirtRoad"));
		break;
	case ECityPaletteAction::SelectLocalRoad:
		Board->SetSelectedRoadDefinition(TEXT("LocalRoad"));
		break;
	case ECityPaletteAction::SelectAvenue:
		Board->SetSelectedRoadDefinition(TEXT("Avenue"));
		break;

	case ECityPaletteAction::SelectHighway:
		Board->SetSelectedRoadDefinition(TEXT("Highway"));
		break;
	case ECityPaletteAction::RoadGround:
		Board->SetSelectedRoadProfile(ECityRoadProfile::Ground);
		break;
	case ECityPaletteAction::RoadBridge:
		Board->SetSelectedRoadProfile(ECityRoadProfile::Bridge);
		break;
	case ECityPaletteAction::RoadElevated:
		Board->SetSelectedRoadProfile(ECityRoadProfile::Elevated);
		break;
	case ECityPaletteAction::SelectHome:
		Board->SetToolMode(ECityToolMode::Home);
		break;
	case ECityPaletteAction::SelectShop:
		Board->SetToolMode(ECityToolMode::Shop);
		break;
	case ECityPaletteAction::SelectPark:
		Board->SetToolMode(ECityToolMode::Park);
		break;
	case ECityPaletteAction::SelectNeighborhoodPark:
		Board->SetToolMode(ECityToolMode::NeighborhoodPark);
		break;
	case ECityPaletteAction::SelectBasketballPark:
		Board->SetToolMode(ECityToolMode::BasketballPark);
		break;
	case ECityPaletteAction::SelectSkatePark:
		Board->SetToolMode(ECityToolMode::SkatePark);
		break;
	case ECityPaletteAction::SelectBaseballPark:
		Board->SetToolMode(ECityToolMode::BaseballPark);
		break;
	case ECityPaletteAction::SelectSoccerPark:
		Board->SetToolMode(ECityToolMode::SoccerPark);
		break;
	case ECityPaletteAction::SelectCityPark:
		Board->SetToolMode(ECityToolMode::CityPark);
		break;
	case ECityPaletteAction::SelectSchool:
		Board->SetSelectedFacility(TEXT("School"));
		break;
	case ECityPaletteAction::SelectFire:
		Board->SetToolMode(ECityToolMode::Fire);
		break;
	case ECityPaletteAction::OpenWasteFacilities:
		SetPalettePage(ECityPalettePage::WasteFacilities);
		break;
	case ECityPaletteAction::SelectWasteDepot:
		Board->SetToolMode(ECityToolMode::WasteDepot);
		break;
	case ECityPaletteAction::SelectRecycling:
		Board->SetToolMode(ECityToolMode::Recycling);
		break;
	case ECityPaletteAction::SelectTestWaste:
		Board->SetToolMode(ECityToolMode::TestWaste);
		break;
	case ECityPaletteAction::OverlayWaste:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Waste);
		break;
	case ECityPaletteAction::OverlayHappiness:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Happiness);
		break;
	case ECityPaletteAction::SelectHospital:
		Board->SetToolMode(ECityToolMode::Hospital);
		break;
	case ECityPaletteAction::SelectTestSickness:
		Board->SetToolMode(ECityToolMode::TestSickness);
		break;
	case ECityPaletteAction::SelectTestSevereSickness:
		Board->SetToolMode(ECityToolMode::TestSevereSickness);
		break;
	case ECityPaletteAction::SelectTestFire:
		Board->SetToolMode(ECityToolMode::TestFire);
		break;
	case ECityPaletteAction::SelectTestCrime:
		Board->SetToolMode(ECityToolMode::TestCrime);
		break;
	case ECityPaletteAction::SelectTestArson:
		Board->SetToolMode(ECityToolMode::TestArson);
		break;
	case ECityPaletteAction::SelectRepair:
		Board->SetToolMode(ECityToolMode::Repair);
		break;
	case ECityPaletteAction::SelectPolice:
		Board->SetToolMode(ECityToolMode::Police);
		break;
	case ECityPaletteAction::SelectPower:
		Board->SetSelectedFacility(TEXT("Power"));
		break;
	case ECityPaletteAction::SelectService:
		Board->SetSelectedFacility(TEXT("Service"));
		break;
	case ECityPaletteAction::SelectResidentialZone:
		Board->SetSelectedZone(ECityZoneType::Residential);
		break;
	case ECityPaletteAction::SelectCommercialZone:
		Board->SetSelectedZone(ECityZoneType::Commercial);
		break;
	case ECityPaletteAction::SelectIndustrialZone:
		Board->SetSelectedZone(ECityZoneType::Industrial);
		break;
	case ECityPaletteAction::Start:
		StartScenario();
		break;
	case ECityPaletteAction::ToggleTutorial:
		SetTutorialEnabled(!GetDefault<UCityTutorialSettings>()->bEnabled);
		break;
	case ECityPaletteAction::ResetTutorial:
		SetTutorialEnabled(true, true);
		break;
	case ECityPaletteAction::Reset:
		ResetScenario();
		break;
	case ECityPaletteAction::Pause:
		TogglePause();
		break;
	case ECityPaletteAction::Speed1:
		SetSpeed1();
		break;
	case ECityPaletteAction::Speed3:
		SetSpeed3();
		break;
	case ECityPaletteAction::Save:
		SaveCity();
		break;
	case ECityPaletteAction::Load:
		LoadCity();
		break;
	case ECityPaletteAction::TaxResidentialDown:
		ResidentialTaxControl.StepBy(-1);
		break;
	case ECityPaletteAction::TaxResidentialUp:
		ResidentialTaxControl.StepBy(1);
		break;
	case ECityPaletteAction::TaxCommercialDown:
		CommercialTaxControl.StepBy(-1);
		break;
	case ECityPaletteAction::TaxCommercialUp:
		CommercialTaxControl.StepBy(1);
		break;
	case ECityPaletteAction::TaxIndustrialDown:
		IndustrialTaxControl.StepBy(-1);
		break;
	case ECityPaletteAction::TaxIndustrialUp:
		IndustrialTaxControl.StepBy(1);
		break;
	case ECityPaletteAction::TaxResidentialTrack:
		ActiveTaxDrag = Action;
		break;
	case ECityPaletteAction::TaxCommercialTrack:
		ActiveTaxDrag = Action;
		break;
	case ECityPaletteAction::TaxIndustrialTrack:
		ActiveTaxDrag = Action;
		break;
	case ECityPaletteAction::ApplyTaxes:
		if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			Simulation->SetTaxRates(ResidentialTaxControl.Commit(), CommercialTaxControl.Commit(),
			                        IndustrialTaxControl.Commit());
		}
		SetPalettePage(ECityPalettePage::City);
		break;
	case ECityPaletteAction::OverlayService:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Service);
		break;
	case ECityPaletteAction::OverlayPower:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Power);
		break;
	case ECityPaletteAction::OverlayResidential:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::ResidentialDemand);
		break;
	case ECityPaletteAction::OverlayCommercial:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::CommercialDemand);
		break;
	case ECityPaletteAction::OverlayIndustrial:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::IndustrialDemand);
		break;
	case ECityPaletteAction::OverlayParks:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Parks);
		break;
	case ECityPaletteAction::OverlayHealth:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Health);
		break;
    case ECityPaletteAction::OverlayCommercialAi:
        Board->SetToolMode(ECityToolMode::Inspect);
        Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::CommercialAi);
        break;
	case ECityPaletteAction::OverlayEducation:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Education);
		break;
	case ECityPaletteAction::OverlayFire:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Fire);
		break;
	case ECityPaletteAction::OverlayCrime:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Crime);
		break;
	case ECityPaletteAction::OverlayTraffic:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Traffic);
		break;
	case ECityPaletteAction::OverlayPollution:
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Pollution);
		break;
	case ECityPaletteAction::OverlayClear:
		Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
		break;
	default:
		break;
	}
}

void ACityVRToolController::BeginTaxEdit()
{
	if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		const FCityEconomySnapshot& Snapshot = Simulation->GetEconomySnapshot();
		ResidentialTaxControl.Begin(Snapshot.ResidentialTaxBasisPoints);
		CommercialTaxControl.Begin(Snapshot.CommercialTaxBasisPoints);
		IndustrialTaxControl.Begin(Snapshot.IndustrialTaxBasisPoints);
	}
}

void ACityVRToolController::CancelTaxEdit()
{
	EndTaxDrag();
	ResidentialTaxControl.Cancel();
	CommercialTaxControl.Cancel();
	IndustrialTaxControl.Cancel();
}

void ACityVRToolController::UpdateTaxDragFromRay(const FVector& RayOrigin, const FVector& RayDirection)
{
	if (ActiveTaxDrag != ECityPaletteAction::TaxResidentialTrack &&
	    ActiveTaxDrag != ECityPaletteAction::TaxCommercialTrack &&
	    ActiveTaxDrag != ECityPaletteAction::TaxIndustrialTrack)
	{
		return;
	}
	const FVector Normal = PaletteRoot->GetForwardVector();
	const double Denominator = FVector::DotProduct(RayDirection, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const double Distance =
	    FVector::DotProduct(PaletteRoot->GetComponentLocation() - RayOrigin, Normal) / Denominator;
	if (Distance < 0.0 || Distance > PaletteRayLength)
	{
		return;
	}
	const FVector Local =
	    PaletteRoot->GetComponentTransform().InverseTransformPosition(RayOrigin + RayDirection * Distance);
	const int32 EntryIndex = VisiblePaletteEntries.IndexOfByPredicate(
	    [this](const FCityPaletteEntry& Entry) { return Entry.Action == ActiveTaxDrag; });
	if (EntryIndex == INDEX_NONE)
	{
		EndTaxDrag();
		return;
	}
	const FVector TrackLocation = GetPaletteButtonLocation(EntryIndex, ActiveTaxDrag);
	const double TrackPosition = FCityHorizontalSliderPresentation::NormalizedFromLocalY(
	    Local.Y, TrackLocation.Y, PaletteButtonHalfWidth);
	if (ActiveTaxDrag == ECityPaletteAction::TaxResidentialTrack)
	{
		ResidentialTaxControl.SetFromTrack(TrackPosition);
	}
	else if (ActiveTaxDrag == ECityPaletteAction::TaxCommercialTrack)
	{
		CommercialTaxControl.SetFromTrack(TrackPosition);
	}
	else
	{
		IndustrialTaxControl.SetFromTrack(TrackPosition);
	}
	RefreshPaletteText();
}

void ACityVRToolController::EndTaxDrag()
{
	ActiveTaxDrag = ECityPaletteAction::Close;
}

FString ACityVRToolController::GetPaletteEntryLabel(const FCityPaletteEntry& Entry) const
{
	if (Entry.Action == ECityPaletteAction::GraphicsMode)
		return TEXT("MODE: ") + DesktopVideo.ModeLabel();
	if (Entry.Action == ECityPaletteAction::GraphicsVSync)
		return DesktopVideo.IsVSyncEnabled() ? TEXT("VSYNC: ON") : TEXT("VSYNC: OFF");
	if (Entry.Action == ECityPaletteAction::GraphicsRenderScale)
		return FString::Printf(TEXT("SCENE SCALE: %d%%"), DesktopVideo.GetRenderScale());
	if (Entry.Action == ECityPaletteAction::GraphicsAntiAliasing)
		return FString::Printf(TEXT("ANTI-ALIASING: %dx"), DesktopVideo.GetAntiAliasingSamples());
	if (Entry.Action == ECityPaletteAction::CycleDisasters)
		return FString::Printf(TEXT("DISASTERS: %s"),
		                       FCityIdentity::DisasterFrequencyName(NewCityIdentity.DisasterFrequency));
	if (Entry.Action == ECityPaletteAction::CycleMapSize)
		return FString::Printf(TEXT("MAP SIZE: %d x %d"), NewCityTerrain.Dimensions.GetWidth(),
		                       NewCityTerrain.Dimensions.GetHeight());
	if (Entry.Action == ECityPaletteAction::OpenDisasters)
		return FString::Printf(
		    TEXT("DISASTERS: %s"),
		    FCityIdentity::DisasterFrequencyName(
		        GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCityIdentity().DisasterFrequency));
	auto RateLabel = [](const TCHAR* Prefix, const FCityBoundedValueControl& Control) {
		return FString::Printf(TEXT("%s %.0f%%"), Prefix, Control.GetPendingValue() / 100.0);
	};
	switch (Entry.Action)
	{
	case ECityPaletteAction::ToggleTutorial:
		return GetDefault<UCityTutorialSettings>()->bEnabled ? TEXT("TUTORIAL: ON") : TEXT("TUTORIAL: OFF");
	case ECityPaletteAction::ToggleArchivedCities:
		return bReviewArchivedCities ? TEXT("ACTIVE CITIES") : TEXT("ARCHIVED CITIES");
	case ECityPaletteAction::ToggleDebugInfo:
		return bDebugInfoVisible ? TEXT("DEBUG MODE: ON") : TEXT("DEBUG MODE: OFF");
	case ECityPaletteAction::ToggleConditionIndicators:
		return bConditionIndicatorsVisible ? TEXT("MAP BADGES: ON") : TEXT("MAP BADGES: OFF");
	case ECityPaletteAction::FocusNextCondition:
		if (const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			int32 Total = 0;
			FCityConditionIndicators::Build(Sim->GetPlacementModel(), Sim->GetFireSnapshot(),
			                                Sim->GetCrimeSnapshot(), Sim->GetEconomyTuning().FireRepairSteps,
			                                &Sim->GetHealthSnapshot(), &Sim->GetPopulationSnapshot(), 0,
			                                &Sim->GetWasteSnapshot(), &Sim->GetServiceSnapshot(), 0, 0,
			                                &Total);
			return FString::Printf(TEXT("NEXT ISSUE (%d)"), Total);
		}
		return TEXT("NEXT ISSUE");
	case ECityPaletteAction::CycleAnchor:
		return PaletteAnchorMode == 0   ? TEXT("MENU: VIEW")
		       : PaletteAnchorMode == 1 ? TEXT("MENU: STABLE")
		                                : TEXT("MENU: HAND");
	case ECityPaletteAction::TaxResidentialDown:
		return FString::Printf(TEXT("- 1%%  %.0f%%"), ResidentialTaxControl.GetPendingValue() / 100.0);
	case ECityPaletteAction::TaxResidentialUp:
		return FString::Printf(TEXT("+ 1%%  %.0f%%"), ResidentialTaxControl.GetPendingValue() / 100.0);
	case ECityPaletteAction::TaxCommercialDown:
		return FString::Printf(TEXT("- 1%%  %.0f%%"), CommercialTaxControl.GetPendingValue() / 100.0);
	case ECityPaletteAction::TaxCommercialUp:
		return FString::Printf(TEXT("+ 1%%  %.0f%%"), CommercialTaxControl.GetPendingValue() / 100.0);
	case ECityPaletteAction::TaxResidentialTrack:
		return RateLabel(TEXT("RESIDENTIAL"), ResidentialTaxControl);
	case ECityPaletteAction::TaxCommercialTrack:
		return RateLabel(TEXT("COMMERCIAL"), CommercialTaxControl);
	case ECityPaletteAction::TaxIndustrialTrack:
		return RateLabel(TEXT("INDUSTRIAL"), IndustrialTaxControl);
	case ECityPaletteAction::TaxIndustrialDown:
		return FString::Printf(TEXT("- 1%%  %.0f%%"), IndustrialTaxControl.GetPendingValue() / 100.0);
	case ECityPaletteAction::TaxIndustrialUp:
		return FString::Printf(TEXT("+ 1%%  %.0f%%"), IndustrialTaxControl.GetPendingValue() / 100.0);
	default:
		return FString(Entry.Label);
	}
}

void ACityVRToolController::SetPalettePage(const ECityPalettePage NewPage)
{
	if (PalettePage == ECityPalettePage::Graphics && NewPage != PalettePage)
		DesktopVideo.Cancel();
	if ((PalettePage == ECityPalettePage::NewCitySetup || PalettePage == ECityPalettePage::NewCityReview) &&
	    NewPage != ECityPalettePage::NewCitySetup && NewPage != ECityPalettePage::NewCityReview)
		NewCityDraft = FCityNewCitySetup();
	if (NewPage == ECityPalettePage::Root)
		bMainMenuPinned = false;
	if (PalettePage == ECityPalettePage::Taxes && NewPage != PalettePage)
		CancelTaxEdit();
	if (NewPage == ECityPalettePage::SafetyTests && !bDebugInfoVisible)
	{
		SetPalettePage(ECityPalettePage::Settings);
		return;
	}
	PalettePage =
	    bEntrySceneActive && NewPage == ECityPalettePage::Root ? ECityPalettePage::MainMenu : NewPage;
	VisiblePaletteEntries = bEntrySceneActive
	                            ? FCityPaletteModel::Entries(PalettePage)
	                            : FCityPaletteModel::SurfaceEntries(PalettePage, UsesHandPalette());
	if (bDesktopUi && PalettePage == ECityPalettePage::Settings)
		VisiblePaletteEntries.Insert(
		    {ECityPaletteAction::OpenGraphics, TEXT("GRAPHICS"), TEXT("Display mode, resolution and VSync")},
		    VisiblePaletteEntries.Num() - 1);
	if (PalettePage == ECityPalettePage::Graphics)
		VisiblePaletteEntries.RemoveAll([this](const FCityPaletteEntry& Entry) {
			const bool bConfirmAction = Entry.Action == ECityPaletteAction::GraphicsKeep ||
			                            Entry.Action == ECityPaletteAction::GraphicsRevert;
			if (DesktopVideo.IsConfirming())
				return !bConfirmAction && Entry.Action != ECityPaletteAction::Back;
			return bConfirmAction || (DesktopVideo.GetMode() == EWindowMode::WindowedFullscreen &&
			                          (Entry.Action == ECityPaletteAction::GraphicsPreviousResolution ||
			                           Entry.Action == ECityPaletteAction::GraphicsNextResolution));
		});
	VisiblePaletteEntries.RemoveAll([this](const FCityPaletteEntry& Entry) {
		return (!bDebugInfoVisible && Entry.Action == ECityPaletteAction::OpenSafetyTests) ||
		       (!bDesktopUi && Entry.Action == ECityPaletteAction::CycleMapSize) ||
               (bDesktopUi && Entry.Action == ECityPaletteAction::CycleAnchor) ||
		       (UE_BUILD_SHIPPING && Entry.Action == ECityPaletteAction::ToggleDebugInfo);
	});
	HoveredPaletteIndex = INDEX_NONE;
	RefreshPaletteText();
}

bool ACityVRToolController::IsPaletteActionSelected(const ECityPaletteAction Action) const
{
	if (!Board)
	{
		return false;
	}
	const auto* DisasterSimulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	switch (Action)
	{
	case ECityPaletteAction::ReviewDigitalExchange:
		return TechnologyReviewDefinition == TEXT("DigitalExchange");
	case ECityPaletteAction::ReviewComputingCenter:
		return TechnologyReviewDefinition == TEXT("ComputingCenter");
	case ECityPaletteAction::ReviewAIInstitute:
        return TechnologyReviewDefinition == TEXT("AIInstitute");
    case ECityPaletteAction::ReviewEnterpriseAICenter:
        return TechnologyReviewDefinition == TEXT("EnterpriseAICenter");
    case ECityPaletteAction::EnableCommercialAi:
        return DisasterSimulation && DisasterSimulation->GetPopulationSnapshot().bCommercialAiPilot;
    case ECityPaletteAction::DisableCommercialAi:
        return DisasterSimulation && !DisasterSimulation->GetPopulationSnapshot().bCommercialAiPilot;
    case ECityPaletteAction::ReviewResearchCampus:
		return TechnologyReviewDefinition == TEXT("ResearchCampus");
	case ECityPaletteAction::ReportHourly: return ReportPeriod == ECityReportPeriod::Hourly;
	case ECityPaletteAction::ReportDaily: return ReportPeriod == ECityReportPeriod::Daily;
	case ECityPaletteAction::ReportWeekly: return ReportPeriod == ECityReportPeriod::Weekly;
	case ECityPaletteAction::ReportMonthly: return ReportPeriod == ECityReportPeriod::Monthly;
	case ECityPaletteAction::ReportAnnual: return ReportPeriod == ECityReportPeriod::Annual;
	case ECityPaletteAction::ReportPeople:
		return ReportCategory == 0;
	case ECityPaletteAction::OverlayHappiness:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Happiness;
	case ECityPaletteAction::ReportStaff:
		return ReportCategory == 1;
	case ECityPaletteAction::ReportMoney:
		return ReportCategory == 2;
	case ECityPaletteAction::ReportGoods:
		return ReportCategory == 3;
	case ECityPaletteAction::ReportWaste:
		return ReportCategory == 4 && ReportMetricIndex >= 5;
	case ECityPaletteAction::ReportHealth:
		return ReportCategory == 5;
	case ECityPaletteAction::ReportUtilities:
		return ReportCategory == 4 && ReportMetricIndex < 5;
	case ECityPaletteAction::DisastersOff:
		return DisasterSimulation->GetCityIdentity().DisasterFrequency == ECityDisasterFrequency::Off;
	case ECityPaletteAction::DisastersLow:
		return DisasterSimulation->GetCityIdentity().DisasterFrequency == ECityDisasterFrequency::Low;
	case ECityPaletteAction::DisastersMedium:
		return DisasterSimulation->GetCityIdentity().DisasterFrequency == ECityDisasterFrequency::Medium;
	case ECityPaletteAction::DisastersHigh:
		return DisasterSimulation->GetCityIdentity().DisasterFrequency == ECityDisasterFrequency::High;
	case ECityPaletteAction::DifficultyRelaxed:
		return NewCityDraft.Difficulty == ECityDifficultyDraft::Relaxed;
	case ECityPaletteAction::DifficultyNormal:
		return NewCityDraft.Difficulty == ECityDifficultyDraft::Normal;
	case ECityPaletteAction::DifficultyChallenging:
		return NewCityDraft.Difficulty == ECityDifficultyDraft::Challenging;
	case ECityPaletteAction::OpenBuild:
		return PalettePage == ECityPalettePage::Build || PalettePage == ECityPalettePage::Roads ||
		       PalettePage == ECityPalettePage::PowerFacilities ||
		       PalettePage == ECityPalettePage::WaterFacilities ||
		       PalettePage == ECityPalettePage::TechnologyFacilities ||
		       PalettePage == ECityPalettePage::EducationFacilities ||
		       PalettePage == ECityPalettePage::WasteFacilities || PalettePage == ECityPalettePage::Parks;
	case ECityPaletteAction::OpenZone:
		return PalettePage == ECityPalettePage::Zone;
	case ECityPaletteAction::OpenCity:
		return PalettePage == ECityPalettePage::City || PalettePage == ECityPalettePage::Taxes ||
		       PalettePage == ECityPalettePage::Reports || PalettePage == ECityPalettePage::Loans ||
		       PalettePage == ECityPalettePage::LoanReview || PalettePage == ECityPalettePage::Policies ||
		       PalettePage == ECityPalettePage::CommercialAi || PalettePage == ECityPalettePage::PolicyReview || PalettePage == ECityPalettePage::TechnologyReview;
	case ECityPaletteAction::OpenSettings:
		return PalettePage == ECityPalettePage::Audio || PalettePage == ECityPalettePage::Settings ||
		       PalettePage == ECityPalettePage::Disasters || PalettePage == ECityPalettePage::Lighting ||
		       PalettePage == ECityPalettePage::SafetyTests;
	case ECityPaletteAction::OpenSafetyTests:
		return PalettePage == ECityPalettePage::SafetyTests;
	case ECityPaletteAction::OpenAudio:
		return PalettePage == ECityPalettePage::Audio;
	case ECityPaletteAction::AlarmMute:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAlarmVolume(), 0.f);
	case ECityPaletteAction::Alarm25:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAlarmVolume(), .25f);
	case ECityPaletteAction::Alarm50:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAlarmVolume(), .5f);
	case ECityPaletteAction::Alarm75:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAlarmVolume(), .75f);
	case ECityPaletteAction::Alarm100:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAlarmVolume(), 1.f);
	case ECityPaletteAction::AmbienceMute:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), 0.f);
	case ECityPaletteAction::Ambience25:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), .25f);
	case ECityPaletteAction::Ambience50:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), .5f);
	case ECityPaletteAction::Ambience75:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), .75f);
	case ECityPaletteAction::Ambience100:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), 1.f);
	case ECityPaletteAction::MusicMute:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetMusicVolume(), 0.f);
	case ECityPaletteAction::Music25:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetMusicVolume(), .25f);
	case ECityPaletteAction::Music50:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetMusicVolume(), .5f);
	case ECityPaletteAction::Music75:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetMusicVolume(), .75f);
	case ECityPaletteAction::Music100:
		return FMath::IsNearlyEqual(GetDefault<UCityAudioSettings>()->GetMusicVolume(), 1.f);
	case ECityPaletteAction::OpenLighting:
		return PalettePage == ECityPalettePage::Lighting;
	case ECityPaletteAction::ToggleDebugInfo:
		return bDebugInfoVisible;
	case ECityPaletteAction::ToggleConditionIndicators:
		return bConditionIndicatorsVisible;
	case ECityPaletteAction::OpenOverlays:
		return PalettePage == ECityPalettePage::Overlays;
	case ECityPaletteAction::Start:
		if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			return Simulation->GetScenarioSnapshot().Phase == ECityScenarioPhase::NotStarted;
		}
		return false;
	case ECityPaletteAction::Reset:
		if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			return Simulation->IsResetConfirmationPending();
		}
		return false;
	case ECityPaletteAction::Pause:
		if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			return Simulation->IsSimulationPaused();
		}
		return false;
	case ECityPaletteAction::Speed1:
		if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			return !Simulation->IsSimulationPaused() && Simulation->GetSimulationSpeed() == 1;
		}
		return false;
	case ECityPaletteAction::Speed3:
		if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			return !Simulation->IsSimulationPaused() && Simulation->GetSimulationSpeed() == 3;
		}
		return false;
	case ECityPaletteAction::SelectInspect:
		return Board->GetToolMode() == ECityToolMode::Inspect;
	case ECityPaletteAction::SelectDemolish:
		return Board->GetToolMode() == ECityToolMode::Demolish;
	case ECityPaletteAction::SelectRoad:
		return Board->GetToolMode() == ECityToolMode::Road;
	case ECityPaletteAction::SelectDirtRoad:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadDefinition() == TEXT("DirtRoad");
	case ECityPaletteAction::SelectLocalRoad:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadDefinition() == TEXT("LocalRoad");
	case ECityPaletteAction::SelectAvenue:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadDefinition() == TEXT("Avenue");

	case ECityPaletteAction::RoadGround:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadProfile() == ECityRoadProfile::Ground;
	case ECityPaletteAction::RoadBridge:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadProfile() == ECityRoadProfile::Bridge;
	case ECityPaletteAction::RoadElevated:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadProfile() == ECityRoadProfile::Elevated;
	case ECityPaletteAction::SelectHighway:
		return Board->GetToolMode() == ECityToolMode::Road &&
		       Board->GetSelectedRoadDefinition() == TEXT("Highway");
	case ECityPaletteAction::SelectHome:
		return Board->GetToolMode() == ECityToolMode::Home;
	case ECityPaletteAction::SelectShop:
		return Board->GetToolMode() == ECityToolMode::Shop;
	case ECityPaletteAction::SelectPrimarySchool:
		return Board->GetSelectedFacilityId() == TEXT("PrimarySchool");
	case ECityPaletteAction::SelectSecondarySchool:
		return Board->GetSelectedFacilityId() == TEXT("SecondarySchool");
	case ECityPaletteAction::SelectCollege:
		return Board->GetSelectedFacilityId() == TEXT("College");
	case ECityPaletteAction::SelectSchool:
		return Board->GetSelectedFacilityId() == TEXT("School");
	case ECityPaletteAction::SelectFire:
		return Board->GetToolMode() == ECityToolMode::Fire;
	case ECityPaletteAction::SelectWasteDepot:
		return Board->GetToolMode() == ECityToolMode::WasteDepot;
	case ECityPaletteAction::SelectRecycling:
		return Board->GetToolMode() == ECityToolMode::Recycling;
	case ECityPaletteAction::OverlayWaste:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Waste;
	case ECityPaletteAction::SelectHospital:
		return Board->GetToolMode() == ECityToolMode::Hospital;
	case ECityPaletteAction::SelectTestSickness:
		return Board->GetToolMode() == ECityToolMode::TestSickness;
	case ECityPaletteAction::SelectTestWaste:
		return Board->GetToolMode() == ECityToolMode::TestWaste;
	case ECityPaletteAction::SelectTestSevereSickness:
		return Board->GetToolMode() == ECityToolMode::TestSevereSickness;
	case ECityPaletteAction::SelectTestFire:
		return Board->GetToolMode() == ECityToolMode::TestFire;
	case ECityPaletteAction::SelectTestCrime:
		return Board->GetToolMode() == ECityToolMode::TestCrime;
	case ECityPaletteAction::SelectTestArson:
		return Board->GetToolMode() == ECityToolMode::TestArson;
	case ECityPaletteAction::SelectRepair:
		return Board->GetToolMode() == ECityToolMode::Repair;
	case ECityPaletteAction::SelectPolice:
		return Board->GetToolMode() == ECityToolMode::Police;
	case ECityPaletteAction::SelectPark:
		return Board->GetToolMode() == ECityToolMode::Park;
	case ECityPaletteAction::SelectNeighborhoodPark:
		return Board->GetToolMode() == ECityToolMode::NeighborhoodPark;
	case ECityPaletteAction::SelectBasketballPark:
		return Board->GetToolMode() == ECityToolMode::BasketballPark;
	case ECityPaletteAction::SelectSkatePark:
		return Board->GetToolMode() == ECityToolMode::SkatePark;
	case ECityPaletteAction::SelectBaseballPark:
		return Board->GetToolMode() == ECityToolMode::BaseballPark;
	case ECityPaletteAction::SelectSoccerPark:
		return Board->GetToolMode() == ECityToolMode::SoccerPark;
	case ECityPaletteAction::SelectCityPark:
		return Board->GetToolMode() == ECityToolMode::CityPark;
	case ECityPaletteAction::OverlayParks:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Parks;
	case ECityPaletteAction::SelectPower:
		return Board->GetSelectedFacilityId() == TEXT("Power");
	case ECityPaletteAction::SelectService:
		return Board->GetSelectedFacilityId() == TEXT("Service");
	case ECityPaletteAction::SelectResidentialZone:
		return Board->GetToolMode() == ECityToolMode::Zone &&
		       Board->GetSelectedZone() == ECityZoneType::Residential;
	case ECityPaletteAction::SelectCommercialZone:
		return Board->GetToolMode() == ECityToolMode::Zone &&
		       Board->GetSelectedZone() == ECityZoneType::Commercial;
	case ECityPaletteAction::SelectIndustrialZone:
		return Board->GetToolMode() == ECityToolMode::Zone &&
		       Board->GetSelectedZone() == ECityZoneType::Industrial;
	case ECityPaletteAction::OverlayService:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Service;
	case ECityPaletteAction::OverlayPower:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Power;
	case ECityPaletteAction::OverlayResidential:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::ResidentialDemand;
	case ECityPaletteAction::OverlayCommercial:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::CommercialDemand;
	case ECityPaletteAction::OverlayIndustrial:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::IndustrialDemand;
	case ECityPaletteAction::OverlayHealth:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Health;
	case ECityPaletteAction::OverlayEducation:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Education;
	case ECityPaletteAction::OverlayFire:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Fire;
	case ECityPaletteAction::OverlayCrime:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Crime;
	case ECityPaletteAction::OverlayTraffic:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Traffic;
	case ECityPaletteAction::OverlayPollution:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Pollution;
	case ECityPaletteAction::LightingAutomatic:
		return Board->GetLightingPreset() == ECityLightingPreset::Automatic;
	case ECityPaletteAction::LightingDay:
		return Board->GetLightingPreset() == ECityLightingPreset::Day;
	case ECityPaletteAction::LightingDusk:
		return Board->GetLightingPreset() == ECityLightingPreset::Dusk;
	case ECityPaletteAction::LightingNight:
		return Board->GetLightingPreset() == ECityLightingPreset::Night;
	case ECityPaletteAction::OverlayClear:
		return Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::None;
	default:
		return !FCityPaletteModel::ConstructionDefinition(Action).IsNone() &&
		       Board->GetSelectedFacilityId() == FCityPaletteModel::ConstructionDefinition(Action);
	}
}

bool ACityVRToolController::IsPaletteActionDisabled(const ECityPaletteAction Action) const
{
	if (Action >= ECityPaletteAction::OpenGraphics && Action <= ECityPaletteAction::GraphicsAntiAliasing)
	{
		if (!bDesktopUi || !DesktopVideo.IsReady())
			return true;
		if (Action == ECityPaletteAction::OpenGraphics)
			return false;
		if (PalettePage != ECityPalettePage::Graphics)
			return true;
		if (Action == ECityPaletteAction::GraphicsKeep || Action == ECityPaletteAction::GraphicsRevert)
			return !DesktopVideo.IsConfirming();
		if (Action == ECityPaletteAction::GraphicsApply)
			return DesktopVideo.IsConfirming() || !DesktopVideo.IsDirty();
		return DesktopVideo.IsConfirming() || ((Action == ECityPaletteAction::GraphicsPreviousResolution ||
		                                        Action == ECityPaletteAction::GraphicsNextResolution) &&
		                                       DesktopVideo.GetMode() == EWindowMode::WindowedFullscreen);
	}
	if (!bDebugInfoVisible &&
	    (Action == ECityPaletteAction::SelectHome || Action == ECityPaletteAction::SelectShop ||
	     Action == ECityPaletteAction::OpenSafetyTests || Action == ECityPaletteAction::PreviewAlarm ||
	     Action == ECityPaletteAction::TestEarthquake || Action == ECityPaletteAction::TestTornado ||
	     Action == ECityPaletteAction::SelectTestWaste || Action == ECityPaletteAction::SelectTestSickness ||
	     Action == ECityPaletteAction::SelectTestSevereSickness ||
	     Action == ECityPaletteAction::SelectTestFire || Action == ECityPaletteAction::SelectTestCrime ||
	     Action == ECityPaletteAction::SelectTestArson))
		return true;
	if (UE_BUILD_SHIPPING && Action == ECityPaletteAction::ToggleDebugInfo)
		return true;
	if (Action == ECityPaletteAction::ConfirmPolicy)
		return PalettePage != ECityPalettePage::PolicyReview;
	if (Action == ECityPaletteAction::ReviewLoanSmall || Action == ECityPaletteAction::ReviewLoanMedium ||
	    Action == ECityPaletteAction::ReviewLoanLarge || Action == ECityPaletteAction::ReviewLoanPayoff ||
	    Action == ECityPaletteAction::ConfirmLoan)
	{
		const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
		if (!Sim)
			return true;
		const auto& L = Sim->GetEconomySnapshot().Loan;
		if (Action == ECityPaletteAction::ReviewLoanPayoff)
			return L.Payoff() == 0;
		if (Action == ECityPaletteAction::ConfirmLoan)
			return PalettePage != ECityPalettePage::LoanReview ||
			       (bLoanPayoffReview ? L.Payoff() == 0 || Sim->GetPlacementModel().GetFunds() < L.Payoff()
			                          : L.Payoff() != 0);
		return L.Payoff() != 0;
	}
	if (Action == ECityPaletteAction::NewsOlder || Action == ECityPaletteAction::NewsNewer)
	{
		const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
		if (!Sim)
			return true;
		const int32 Count = FCityQuarterlyNews::Editions(Sim->GetReportHistory()).Num();
		return Action == ECityPaletteAction::NewsNewer ? NewsPage <= 0 : NewsPage + 1 >= Count;
	}
	if (Action == ECityPaletteAction::HousingOlder || Action == ECityPaletteAction::HousingNewer)
	{
		const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
		if (!Sim)
			return true;
		const int32 Count = Sim->GetPopulationSnapshot().HousingEvents.Num();
		const int32 PerPage = bDesktopUi ? FCityHousingHistory::EventsPerPage : 1;
		return Action == ECityPaletteAction::HousingNewer ? HousingHistoryPage <= 0
		                                                  : (HousingHistoryPage + 1) * PerPage >= Count;
	}
	switch (Action)
	{
	case ECityPaletteAction::ConfirmLoad: {
		FString Error;
		return !ReviewedCitySlots.IsValidIndex(ReviewedCityIndex) || bReviewArchivedCities ||
		       !ReviewedCitySlots[ReviewedCityIndex].bSupported ||
		       !FCityPersistence::ValidatePlatform(ReviewedCitySlots[ReviewedCityIndex].Dimensions,
		                                           FCityPersistence::NativePlatform(!bDesktopUi), Error);
	}
	case ECityPaletteAction::CycleMapSize:
		return !bDesktopUi || PalettePage != ECityPalettePage::NewCitySetup;
	case ECityPaletteAction::ReviewArchive:
		return !ReviewedCitySlots.IsValidIndex(ReviewedCityIndex) || bReviewArchivedCities ||
		       ReviewedCitySlots[ReviewedCityIndex].Identity.Id ==
		           GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCityIdentity().Id;
	case ECityPaletteAction::RestoreCity:
		return !ReviewedCitySlots.IsValidIndex(ReviewedCityIndex) || !bReviewArchivedCities;
	case ECityPaletteAction::TaxResidentialDown:
		return ResidentialTaxControl.GetPendingValue() <= ResidentialTaxControl.GetMinimum();
	case ECityPaletteAction::TaxResidentialUp:
		return ResidentialTaxControl.GetPendingValue() >= ResidentialTaxControl.GetMaximum();
	case ECityPaletteAction::TaxCommercialDown:
		return CommercialTaxControl.GetPendingValue() <= CommercialTaxControl.GetMinimum();
	case ECityPaletteAction::TaxCommercialUp:
		return CommercialTaxControl.GetPendingValue() >= CommercialTaxControl.GetMaximum();
	case ECityPaletteAction::TaxIndustrialDown:
		return IndustrialTaxControl.GetPendingValue() <= IndustrialTaxControl.GetMinimum();
	case ECityPaletteAction::TaxIndustrialUp:
		return IndustrialTaxControl.GetPendingValue() >= IndustrialTaxControl.GetMaximum();
	default:
		return false;
	}
}

void ACityVRToolController::RouteTrigger(const ECityControllerHand Hand)
{
	const uint16 TriggerBit = Hand == ECityControllerHand::Left ? InputLeftTrigger : InputRightTrigger;
	if (!Board || bManipulationSession || !LifecycleInputGate.CanAcceptInput() ||
	    (SuppressedTriggerMask & TriggerBit) != 0)
	{
		return;
	}
	if (bEntrySceneActive && !bPaletteOpen)
		return;
	// Resolve current pose and ray at the input edge rather than last frame's hover.
	UpdatePaletteAttachment();
	UpdateControllerRay();
	if (Hand != DominantHand)
		return;
	const ECityTriggerRoute Route =
	    FCityToolInputRouter::RouteTrigger(Hand, DominantHand, bPaletteOpen, bPaletteSurfaceFocused);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_TRIGGER Hand=%s Route=%d PaletteOpen=%d Hover=%d"),
	       Hand == ECityControllerHand::Right ? TEXT("Right") : TEXT("Left"), static_cast<int32>(Route),
	       bPaletteOpen, HoveredPaletteIndex);
	if (Route == ECityTriggerRoute::SelectPalette)
	{
		if (HoveredPaletteIndex != INDEX_NONE)
		{
			ExecutePaletteAction(HoveredPaletteIndex);
		}
		RefreshPaletteText();
	}
	else if (!bPaletteOpen && bObjectiveToggleFocused)
	{
		SetObjectivesVisible(!bObjectivesOpen);
	}
	else if (!bPaletteOpen && HoveredObjectiveFooter != INDEX_NONE)
	{
		ExecuteObjectiveFooter(HoveredObjectiveFooter);
	}
	else if (!bPaletteOpen && bObjectivePanelFocused)
	{
		if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
			if (Simulation->GetScenarioSnapshot().Phase == ECityScenarioPhase::NotStarted)
				Simulation->StartScenario();
		UpdateObjectiveCard();
		return;
	}
	else if (!bPaletteOpen && bNeedsToggleFocused)
	{
		SetCitySummaryVisible(!bNeedsOpen);
		UpdateNeedsStatusBars();
		RefreshNeedsVisibility();
	}
	else if (!bPaletteOpen && bInspectorPanelFocused)
	{

		UMotionControllerComponent* Aim =
		    DominantHand == ECityControllerHand::Right ? RightAim.Get() : LeftAim.Get();
		if (Aim)
			HandleInspectorUpgradeRay(Aim->GetComponentLocation(), Aim->GetForwardVector());
		return;
	}
	else if (!bPaletteOpen && bNeedsPanelFocused)
	{
		return;
	}
	else if (!bPaletteOpen && HoveredTimeControl != INDEX_NONE)
	{
		ExecuteTimeControl(HoveredTimeControl);
	}
	else if (Route == ECityTriggerRoute::ConfirmTool)
	{
		if (Board->TryToggleWindOverlay())
			return;
		if (Board->GetToolMode() == ECityToolMode::Inspect)
		{

			UMotionControllerComponent* Aim =
			    DominantHand == ECityControllerHand::Right ? RightAim.Get() : LeftAim.Get();
			if (Aim && !HandleInspectorUpgradeRay(Aim->GetComponentLocation(), Aim->GetForwardVector()))
				ToggleInspection(Aim->GetComponentLocation(), Aim->GetForwardVector());
		}
		else
		{
			if (Board->GetToolMode() == ECityToolMode::Repair ||
			    ((Board->GetToolMode() >= ECityToolMode::TestFire &&
			      Board->GetToolMode() <= ECityToolMode::TestArson) ||
			     Board->GetToolMode() == ECityToolMode::TestSickness ||
			     Board->GetToolMode() == ECityToolMode::TestSevereSickness))
			{

				UMotionControllerComponent* Aim =
				    DominantHand == ECityControllerHand::Right ? RightAim.Get() : LeftAim.Get();
				ClearInspection();
				if (Aim)
					ToggleInspection(Aim->GetComponentLocation(), Aim->GetForwardVector());
			}
			Board->RequestConfirm();
		}
	}
}

void ACityVRToolController::HandleGrip(const ECityControllerHand Hand, const bool bPressed)
{
	if (!LifecycleInputGate.CanAcceptInput())
	{
		return;
	}
	if (bEntrySceneActive)
		return;
	bool& bHeld = Hand == ECityControllerHand::Left ? bLeftGripHeld : bRightGripHeld;
	if (bHeld == bPressed)
	{
		return;
	}
	bHeld = bPressed;

	if (bPressed)
	{
		if (bManipulationSession || Hand == DominantHand || (bLeftGripHeld && bRightGripHeld))
		{
			BeginManipulationSession();
		}
		else
		{
			if (!bMainMenuPinned)
				SetPaletteOpen(true);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_PALETTE_OPEN=1 Hand=%s"),
			       Hand == ECityControllerHand::Right ? TEXT("Right") : TEXT("Left"));
		}
		return;
	}

	if (bManipulationSession)
	{
		if (!bLeftGripHeld && !bRightGripHeld)
		{
			EndManipulationSession();
		}
		else
		{
			ReinitializeManipulationGesture();
		}
	}
	else if (Hand == FCityToolInputRouter::OtherHand(DominantHand))
	{
		if (!bMainMenuPinned)
			SetPaletteOpen(false);
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_PALETTE_OPEN=0 Hand=%s"),
		       Hand == ECityControllerHand::Right ? TEXT("Right") : TEXT("Left"));
	}
}

void ACityVRToolController::BeginManipulationSession()
{
	if (!Board || bEntrySceneActive)
	{
		return;
	}
	const bool bWasActive = bManipulationSession;
	bManipulationSession = true;
	SetPaletteOpen(false);
	Board->SetManipulationActive(true);
	BoardPresentationPose = FCityBoardPresentationPose::FromTransform(Board->GetActorTransform());
	ReinitializeManipulationGesture();
	if (!bWasActive)
	{
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_MANIPULATION_BEGIN"));
	}
}

void ACityVRToolController::EndManipulationSession()
{
	bManipulationSession = false;
	BoardManipulator.Reset();
	if (Board)
	{
		Board->SetManipulationActive(false);
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_MANIPULATION_END"));
}

void ACityVRToolController::ReinitializeManipulationGesture()
{
	if (!LeftGrip || !RightGrip)
	{
		return;
	}
	if (Board)
		BoardManipulator.SetDimensions(Board->GetDimensions());
	BoardManipulator.Reinitialize(bLeftGripHeld, bRightGripHeld, LeftGrip->IsTracked(),
	                              RightGrip->IsTracked(), LeftGrip->GetComponentLocation(),
	                              RightGrip->GetComponentLocation(), BoardPresentationPose);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_MANIPULATION_MODE=%d"),
	       static_cast<int32>(BoardManipulator.GetMode()));
}

void ACityVRToolController::NudgeBoardHeight(const double DeltaHeightCm)
{
	if (!bManipulationSession || !Board)
	{
		return;
	}
	BoardManipulator.NudgeHeight(DeltaHeightCm, BoardPresentationPose);
	Board->SetActorTransform(BoardPresentationPose.ToTransform());
	ReinitializeManipulationGesture();
}

void ACityVRToolController::RecenterBoard()
{
	if (!Board)
	{
		return;
	}
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}
	BoardPresentationPose = FCityBoardManipulator::MakeRecenteredPose(
	    PlayerController->PlayerCameraManager->GetCameraLocation(),
	    PlayerController->PlayerCameraManager->GetCameraRotation().Vector(),
	    BoardPresentationPose.UniformScale, 110.0, 55.0, Board->GetDimensions());
	Board->SetActorTransform(BoardPresentationPose.ToTransform());
	if (bManipulationSession)
	{
		ReinitializeManipulationGesture();
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_BOARD_RECENTER Location=%s Yaw=%.1f Scale=%.5f"),
	       *BoardPresentationPose.Location.ToCompactString(), BoardPresentationPose.YawDegrees,
	       BoardPresentationPose.UniformScale);
}

void ACityVRToolController::SwapHandedness(const ECityControllerHand PressedHand)
{
	if (bManipulationSession || PressedHand != FCityToolInputRouter::OtherHand(DominantHand))
	{
		return;
	}
	DominantHand = PressedHand;
	if (bEntrySceneActive)
	{
		UpdatePaletteAttachment();
		RefreshPaletteText();
		return;
	}
	SetPaletteOpen(false);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_DOMINANT_HAND=%s"),
	       DominantHand == ECityControllerHand::Right ? TEXT("Right") : TEXT("Left"));
}

void ACityVRToolController::UpdateLifecycleInputGate()
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || LifecycleInputGate.CanAcceptInput())
	{
		return;
	}
	if (bDesktopUi)
	{
		if (!bDesktopFocused)
			return;
		bool bPressed = false;
		for (const FKey& Key : {EKeys::LeftMouseButton,
		                        EKeys::RightMouseButton,
		                        EKeys::MiddleMouseButton,
		                        EKeys::W,
		                        EKeys::A,
		                        EKeys::S,
		                        EKeys::D,
		                        EKeys::Escape,
		                        EKeys::SpaceBar,
		                        EKeys::R,
		                        EKeys::Z,
		                        EKeys::F5,
		                        EKeys::F9,
		                        EKeys::One,
		                        EKeys::Two,
		                        EKeys::Three,
		                        EKeys::Home,
		                        EKeys::Up,
		                        EKeys::Down,
		                        EKeys::Left,
		                        EKeys::Right,
		                        EKeys::LeftBracket,
		                        EKeys::RightBracket})
			bPressed |= PlayerController->IsInputKeyDown(Key);
		LifecycleInputGate.Update(bPressed);
		return;
	}
	static const FKey Keys[] = {
	    EKeys::OculusTouch_Left_Grip_Click,       EKeys::OculusTouch_Right_Grip_Click,
	    EKeys::OculusTouch_Left_Trigger_Click,    EKeys::OculusTouch_Right_Trigger_Click,
	    EKeys::OculusTouch_Left_X_Click,          EKeys::OculusTouch_Left_Y_Click,
	    EKeys::OculusTouch_Right_A_Click,         EKeys::OculusTouch_Right_B_Click,
	    EKeys::OculusTouch_Left_Thumbstick_Click, EKeys::OculusTouch_Right_Thumbstick_Click};
	bool bAnyPressed =
	    ActionButtonMask != 0 ||
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Left_Grip_Axis) > 0.1f ||
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Right_Grip_Axis) > 0.1f ||
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Left_Trigger_Axis) > 0.1f ||
	    PlayerController->GetInputAnalogKeyState(EKeys::OculusTouch_Right_Trigger_Axis) > 0.1f;
	for (const FKey& Key : Keys)
	{
		bAnyPressed |= PlayerController->IsInputKeyDown(Key);
	}
	LifecycleInputGate.Update(bAnyPressed);
}

void ACityVRToolController::HandleApplicationBackgrounded()
{
	ClearInspection();
	LifecycleInputGate.Suspend();
	RoadHeightControl.Update(0, false);
	bLeftGripHeld = false;
	bRightGripHeld = false;
	if (bManipulationSession)
	{
		EndManipulationSession();
	}
	SetPaletteOpen(false);
	if (Board)
	{
		Board->RequestCancel();
		Board->SetControllerRay(FVector::ZeroVector, FVector::ForwardVector, false);
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_INPUT_SUSPENDED FreshReleaseRequired=1"));
}

void ACityVRToolController::HandleApplicationForegrounded()
{
	LifecycleInputGate.Suspend();
	RoadHeightControl.Update(0, false);
	if (bEntrySceneActive)
		ShowMainMenu();
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_INPUT_FOREGROUNDED FreshReleaseRequired=1"));
}

void ACityVRToolController::ConfirmDesktop()
{
	if (!Board || !LifecycleInputGate.CanAcceptInput())
		return;
	if (bDesktopUi)
	{
		if (bPaletteOpen && !IsDesktopZoningPaletteActive() && !bEntrySceneActive && bDesktopFocused &&
		    DesktopHud && !DesktopHud->IsPointerOverUi())
		{
			// Consume the dismissal click; it must never place a building underneath.
			SetPaletteOpen(false);
			return;
		}
		if (IsDesktopPointerBlocked())
			return;
		UpdateDesktopInteraction();
	}
	if (bPaletteOpen && !IsDesktopZoningPaletteActive())
	{
		if (auto* Player = GetWorld()->GetFirstPlayerController())
		{
			FVector Origin, Direction;
			if (Player->DeprojectMousePositionToWorld(Origin, Direction))
			{
				const int32 Index = FindPaletteButton(Origin, Direction);
				if (Index != INDEX_NONE)
					ExecutePaletteAction(Index);
			}
		}
		return;
	}
	if (bEntrySceneActive)
		return;
	if (Board->GetToolMode() == ECityToolMode::Inspect)
	{
		if (auto* Player = GetWorld()->GetFirstPlayerController())
		{
			FVector Origin, Direction;
			if (!Player->DeprojectMousePositionToWorld(Origin, Direction))
			{
				FRotator Rotation;
				Player->GetPlayerViewPoint(Origin, Rotation);
				Direction = Rotation.Vector();
			}
			if (!HandleInspectorUpgradeRay(Origin, Direction))
				ToggleInspection(Origin, Direction);
		}
	}
	else
		Board->RequestConfirm();
}
void ACityVRToolController::Cancel()
{
	if (bPaletteOpen)
		NavigatePaletteBack();
	else
	{
		ClearInspection();
		if (Board)
			Board->RequestCancel();
	}
}
void ACityVRToolController::Rotate()
{
	if (Board && !bPaletteOpen)
		Board->RequestRotate();
}
void ACityVRToolController::SelectInspect()
{
	if (Board)
		Board->SetToolMode(ECityToolMode::Inspect);
}
void ACityVRToolController::SelectRoad()
{
	if (Board)
		Board->SetToolMode(ECityToolMode::Road);
}
void ACityVRToolController::SelectHome()
{
	if (Board && bDebugInfoVisible)
		Board->SetToolMode(ECityToolMode::Home);
}
void ACityVRToolController::SelectShop()
{
	if (Board && bDebugInfoVisible)
		Board->SetToolMode(ECityToolMode::Shop);
}
void ACityVRToolController::SelectPark()
{
	if (Board)
		Board->SetToolMode(ECityToolMode::Park);
}
void ACityVRToolController::SelectPower()
{
	if (Board)
		Board->SetSelectedFacility(TEXT("Power"));
}
void ACityVRToolController::SelectService()
{
	if (Board)
		Board->SetSelectedFacility(TEXT("Service"));
}
void ACityVRToolController::SelectZone()
{
	if (Board)
		Board->SetToolMode(ECityToolMode::Zone);
}
void ACityVRToolController::SelectDemolish()
{
	if (Board)
		Board->SetToolMode(ECityToolMode::Demolish);
}
void ACityVRToolController::StartScenario()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		Simulation->StartScenario();
}
void ACityVRToolController::ResetScenario()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		Simulation->RequestResetScenario();
}
void ACityVRToolController::TogglePause()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		Simulation->ToggleSimulationPaused();
}
void ACityVRToolController::SetSpeed1()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		Simulation->SetSimulationSpeed(1);
		Simulation->SetSimulationPaused(false);
	}
}
void ACityVRToolController::SetSpeed3()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		Simulation->SetSimulationSpeed(3);
		Simulation->SetSimulationPaused(false);
	}
}
void ACityVRToolController::SetSpeed5()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		Simulation->SetSimulationSpeed(5);
		Simulation->SetSimulationPaused(false);
	}
}
void ACityVRToolController::SaveCity()
{
	if (UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		Simulation->SaveCity();
}
void ACityVRToolController::LoadCity()
{
	ShowMainMenu();
	ReviewCityLoad();
}
void ACityVRToolController::LeftTrigger()
{
	RecordInputAction(InputLeftTrigger, TEXT("LTrigger"), true);
	RouteTrigger(ECityControllerHand::Left);
}
void ACityVRToolController::LeftTriggerReleased()
{
	RecordInputAction(InputLeftTrigger, TEXT("LTrigger"), false);
	SuppressedTriggerMask &= ~InputLeftTrigger;
	if (DominantHand == ECityControllerHand::Left)
		EndTaxDrag();
}
void ACityVRToolController::RightTrigger()
{
	RecordInputAction(InputRightTrigger, TEXT("RTrigger"), true);
	RouteTrigger(ECityControllerHand::Right);
}
void ACityVRToolController::RightTriggerReleased()
{
	RecordInputAction(InputRightTrigger, TEXT("RTrigger"), false);
	SuppressedTriggerMask &= ~InputRightTrigger;
	if (DominantHand == ECityControllerHand::Right)
		EndTaxDrag();
}
void ACityVRToolController::LeftPrimary()
{
	RecordInputAction(InputX, TEXT("X"), true);
	if (LifecycleInputGate.CanAcceptInput() && DominantHand == ECityControllerHand::Left)
	{
		bManipulationSession ? NudgeBoardHeight(10.0) : Rotate();
	}
}
void ACityVRToolController::LeftPrimaryReleased()
{
	RecordInputAction(InputX, TEXT("X"), false);
}
void ACityVRToolController::RightPrimary()
{
	RecordInputAction(InputA, TEXT("A"), true);
	if (LifecycleInputGate.CanAcceptInput() && DominantHand == ECityControllerHand::Right)
	{
		bManipulationSession ? NudgeBoardHeight(10.0) : Rotate();
	}
}
void ACityVRToolController::RightPrimaryReleased()
{
	RecordInputAction(InputA, TEXT("A"), false);
}
void ACityVRToolController::LeftSecondary()
{
	RecordInputAction(InputY, TEXT("Y"), true);
	if (LifecycleInputGate.CanAcceptInput() && DominantHand == ECityControllerHand::Left)
	{
		bManipulationSession ? NudgeBoardHeight(-10.0) : Cancel();
	}
}
void ACityVRToolController::LeftSecondaryReleased()
{
	RecordInputAction(InputY, TEXT("Y"), false);
}
void ACityVRToolController::RightSecondary()
{
	RecordInputAction(InputB, TEXT("B"), true);
	if (LifecycleInputGate.CanAcceptInput() && DominantHand == ECityControllerHand::Right)
	{
		bManipulationSession ? NudgeBoardHeight(-10.0) : Cancel();
	}
}
void ACityVRToolController::RightSecondaryReleased()
{
	RecordInputAction(InputB, TEXT("B"), false);
}
void ACityVRToolController::LeftGripStarted()
{
	RecordInputAction(InputLeftGrip, TEXT("LGrip"), true);
	HandleGrip(ECityControllerHand::Left, true);
}
void ACityVRToolController::LeftGripCompleted()
{
	RecordInputAction(InputLeftGrip, TEXT("LGrip"), false);
	HandleGrip(ECityControllerHand::Left, false);
}
void ACityVRToolController::RightGripStarted()
{
	RecordInputAction(InputRightGrip, TEXT("RGrip"), true);
	HandleGrip(ECityControllerHand::Right, true);
}
void ACityVRToolController::RightGripCompleted()
{
	RecordInputAction(InputRightGrip, TEXT("RGrip"), false);
	HandleGrip(ECityControllerHand::Right, false);
}
void ACityVRToolController::LeftStickClick()
{
	RecordInputAction(InputLeftStick, TEXT("LStick"), true);
	if (LifecycleInputGate.CanAcceptInput())
	{
		DominantHand == ECityControllerHand::Left ? RecenterBoard()
		                                          : SwapHandedness(ECityControllerHand::Left);
	}
}
void ACityVRToolController::LeftStickReleased()
{
	RecordInputAction(InputLeftStick, TEXT("LStick"), false);
}
void ACityVRToolController::RightStickClick()
{
	RecordInputAction(InputRightStick, TEXT("RStick"), true);
	if (LifecycleInputGate.CanAcceptInput())
	{
		DominantHand == ECityControllerHand::Right ? RecenterBoard()
		                                           : SwapHandedness(ECityControllerHand::Right);
	}
}
void ACityVRToolController::RightStickReleased()
{
	RecordInputAction(InputRightStick, TEXT("RStick"), false);
}

void ACityVRToolController::RefreshReports()
{
	auto SetSharedReportText = [this](const FText& Text) {
		SharedReportText = Text;
		ReportText->SetText(Text);
	};
	auto SetSharedReportRange = [this](const FText& Text) {
		SharedReportRange = Text;
		ReportRangeText->SetText(Text);
	};
	const bool Visible =
	    bPaletteOpen &&
	    (PalettePage == ECityPalettePage::CommercialAi || PalettePage == ECityPalettePage::TechnologyReview || PalettePage == ECityPalettePage::Reports ||
	     PalettePage == ECityPalettePage::DisasterHistory ||
	     (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	      PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	      PalettePage == ECityPalettePage::Policies || PalettePage == ECityPalettePage::PolicyReview));
	ReportText->SetRelativeLocation(FVector(
	    2, 0,
	    (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	     PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	     PalettePage == ECityPalettePage::Policies || PalettePage == ECityPalettePage::PolicyReview)
	        ? 54.5
	        : 48));
	if (PalettePage == ECityPalettePage::QuarterlyNews)
		ReportText->SetRelativeLocation(FVector(2, 0, 51.5));
	ReportText->SetWorldSize(
	    (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	     PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	     PalettePage == ECityPalettePage::Policies || PalettePage == ECityPalettePage::PolicyReview)
	        ? 1.45f
	        : 1.65f);
	ReportRangeText->SetRelativeLocation(FVector(
	    2, 0,
	    (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	     PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	     PalettePage == ECityPalettePage::Policies || PalettePage == ECityPalettePage::PolicyReview)
	        ? 28.5
	        : 25.5));
	SharedReportChart = {};
	ReportText->SetVisibility(Visible);
	if (NewspaperSheet)
		NewspaperSheet->SetVisibility(Visible && PalettePage == ECityPalettePage::QuarterlyNews &&
		                              !bDesktopUi);
	ReportRangeText->SetVisibility(Visible);
	ReportZeroLine->SetVisibility(false);
	for (const auto& Bar : ReportBars)
		Bar->SetVisibility(false);
	if (!Visible)
		return;
	const auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	if (!Simulation)
		return;
    if (PalettePage == ECityPalettePage::CommercialAi)
    {
        ReportText->SetRelativeLocation(FVector(2, 0, 79));
        ReportText->SetWorldSize(1.15f);
        ReportRangeText->SetRelativeLocation(FVector(2, 0, 28.5));
        SetSharedReportText(FText::FromString(FCityCommercialAi::Review(Simulation->GetPopulationSnapshot(), Simulation->GetServiceSnapshot())));
        SetSharedReportRange(FText::FromString(TEXT("Software provider is external; city facilities incur public bills.")));
        return;
    }
	if (PalettePage == ECityPalettePage::TechnologyReview)
	{
		ReportText->SetRelativeLocation(FVector(2, 0, 79));
		ReportText->SetWorldSize(1.45f);
		ReportRangeText->SetRelativeLocation(FVector(2, 0, 28.5));
		SetSharedReportText(FText::FromString(FCityTechnology::Review(
		    TechnologyReviewDefinition, Simulation->GetServiceSnapshot(), Simulation->GetBuildingCatalog())));
		SetSharedReportRange(
		    FText::FromString(TEXT("Review requirements here; choose buildings under Build > Technology.")));
		return;
	}
	if (PalettePage == ECityPalettePage::Policies || PalettePage == ECityPalettePage::PolicyReview)
	{
		const auto& E = Simulation->GetEconomySnapshot();
		const auto Budget = Simulation->GetProjectedBudget();
		const int64 Cash = Simulation->GetPlacementModel().GetFunds();
		FString Body;
		if (PalettePage == ECityPalettePage::Policies)
			Body = E.Policies.Summary(Cash, Budget.PolicyCost);
		else
		{
			auto Candidate = E.Policies;
			Candidate.SetEnabled(PendingPolicy, bPendingPolicyEnable);
			const auto Q = FCityPolicyState::Quote(
			    PendingPolicy, Simulation->GetPopulationSnapshot(), Simulation->GetServiceSnapshot(),
			    Simulation->GetEconomyTuning(), !E.HasOperatingUpkeepShortfall());
			const int64 NewCost =
			    Candidate.Forecast(Simulation->GetPopulationSnapshot(), Simulation->GetServiceSnapshot(),
			                       Simulation->GetEconomyTuning(), !E.HasOperatingUpkeepShortfall());
			Body = E.Policies.Review(PendingPolicy, bPendingPolicyEnable, Q, Cash,
			                         Budget.Net() + Budget.PolicyCost - NewCost);
		}
		SetSharedReportText(FText::FromString(Body));
		SetSharedReportRange(FText::FromString(PolicyMessage));
		return;
	}
	if (PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview)
	{
		const auto& L = Simulation->GetEconomySnapshot().Loan;
		const int64 Cash = Simulation->GetPlacementModel().GetFunds();
		const auto Budget = Simulation->GetProjectedBudget();
		FString Body;
		if (PalettePage == ECityPalettePage::Loans)
			Body = L.Summary(Cash, Budget.Net() + Budget.DebtService);
		else if (bLoanPayoffReview)
			Body =
			    FString::Printf(TEXT("Treasury -> external civic lender\nPrincipal $%lld + interest $%lld\n"
			                         "Full payoff now $%lld\nCity cash now $%lld\n"
			                         "No fee; no future interest after payoff.\n"
			                         "Cash after payoff $%lld\nConfirm pays the displayed current balance."),
			                    L.Principal, L.InterestDue, L.Payoff(), Cash, Cash - L.Payoff());
		else
			Body = FCityLoanState::Terms(PendingLoanAmount) +
			       FString::Printf(
			           TEXT("\nCash after borrowing $%lld\nForecast after first payment $%lld / month"),
			           Cash + PendingLoanAmount,
			           Budget.Net() - (PendingLoanAmount - PendingLoanAmount * 23 / 24 +
			                           FCityLoanState::MonthlyInterest(PendingLoanAmount)));
		SetSharedReportText(FText::FromString(Body));
		SetSharedReportRange(FText::FromString(LoanMessage));
		return;
	}
	if (PalettePage == ECityPalettePage::QuarterlyNews)
	{
		const auto& History = Simulation->GetReportHistory();
		NewsPage = FMath::Clamp(NewsPage, 0, FMath::Max(0, FCityQuarterlyNews::Editions(History).Num() - 1));
		const auto Editorial = CityNewspaperEdition(History, NewsPage);
		SharedNewspaper.Masthead = FText::FromString(Editorial.Masthead);
		SharedNewspaper.Dateline = FText::FromString(Editorial.Dateline);
		SharedNewspaper.Headline = FText::FromString(Editorial.Headline);
		SharedNewspaper.Lead = FText::FromString(Editorial.Lead);
		SharedNewspaper.Footer = FText::FromString(Editorial.Footer);
		SharedNewspaper.Articles.Reset();
		for (const auto& Article : Editorial.Articles)
			SharedNewspaper.Articles.Add({FText::FromString(Article.Section),
			                              FText::FromString(Article.Headline),
			                              FText::FromString(Article.Body)});
		const bool Changed = SharedReportText.ToString() != FCityQuarterlyNews::Body(History, NewsPage);
		SetSharedReportText(FText::FromString(FCityQuarterlyNews::Body(History, NewsPage)));
		SetSharedReportRange(FText::FromString(FCityQuarterlyNews::Footer(History, NewsPage)));
		if (!bDesktopUi)
		{
			if (!NewspaperSheet)
			{
				NewspaperTypeface = LoadObject<UFont>(
				    nullptr, TEXT("/Game/CitySim/UI/Fonts/F_CityNewsSerif.F_CityNewsSerif"));
				WorldNewsContent = MakeShared<FCityUiNewspaper>(SharedNewspaper);
				NewspaperSheet = NewObject<UWidgetComponent>(this, TEXT("QuarterlyNewspaper"));
				NewspaperSheet->SetupAttachment(PaletteRoot);
				NewspaperSheet->SetWidgetSpace(EWidgetSpace::World);
				NewspaperSheet->SetDrawSize(FVector2D(1000, 760));
				NewspaperSheet->SetRelativeLocation(FVector(2, 0, 53.5));
				// WidgetComponent's plane faces +X, matching the existing palette.
				NewspaperSheet->SetRelativeRotation(FRotator::ZeroRotator);
				NewspaperSheet->SetRelativeScale3D(FVector(.074));
				NewspaperSheet->SetTwoSided(true);
				// The palette backplate is an existing depth-independent HUD overlay.
				// Put the printed sheet above it and below the controls/labels.
				NewspaperSheet->SetBlendMode(EWidgetBlendMode::Transparent);
				NewspaperSheet->SetTranslucentSortPriority(115);
				NewspaperSheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				NewspaperSheet->SetCastShadow(false);
				NewspaperSheet->SetManuallyRedraw(true);
				NewspaperSheet->RegisterComponent();
				NewspaperSheet->SetSlateWidget(
				    CityNewspaper::MakeSheet(WorldNewsContent.ToSharedRef(), NewspaperTypeface, 1000, .82f));
			}
			*WorldNewsContent = SharedNewspaper;
			NewspaperSheet->SetVisibility(true);
			if (Changed)
			{
				NewspaperSheet->GetSlateWidget()->Invalidate(EInvalidateWidgetReason::Layout |
				                                             EInvalidateWidgetReason::Paint);
				NewspaperSheet->RequestRedraw();
			}
			ReportText->SetVisibility(false);
			ReportRangeText->SetVisibility(false);
		}
		return;
	}
	if (PalettePage == ECityPalettePage::HousingHistory)
	{
		const auto& P = Simulation->GetPopulationSnapshot();
		const int32 PerPage = bDesktopUi ? FCityHousingHistory::EventsPerPage : 1;
		HousingHistoryPage =
		    FMath::Clamp(HousingHistoryPage, 0, FMath::Max(0, (P.HousingEvents.Num() - 1) / PerPage));
		SetSharedReportText(FText::FromString(FCityHousingHistory::Recent(P, HousingHistoryPage, PerPage)));
		FString Warnings = FCityHousingHistory::Warnings(P);
		if (!bDesktopUi)
			Warnings = Warnings.Left(Warnings.Find(TEXT("\n")));
		const FString Page = FString::Printf(TEXT("Page %d/%d"), HousingHistoryPage + 1,
		                                     FMath::Max(1, (P.HousingEvents.Num() + PerPage - 1) / PerPage));
		SetSharedReportRange(
		    FText::FromString(Warnings + (bDesktopUi ? TEXT("\n") : TEXT(" | ")) + Page +
		                      (bDesktopUi ? TEXT(" | latest32 decisions | newest first") : TEXT(""))));
		return;
	}
	if (PalettePage == ECityPalettePage::DisasterHistory)
	{
		const auto& State = Simulation->GetDisasterSnapshot();
		if (State.Events.IsEmpty())
		{
			SetSharedReportText(FText::FromString(TEXT("No natural disaster events recorded")));
			SetSharedReportRange(FText::FromString(TEXT("History starts here; ordinary fires are separate")));
			return;
		}
		ReportPeriodIndex = ReportPeriodIndex == INDEX_NONE
		                        ? State.Events.Num() - 1
		                        : FMath::Clamp(ReportPeriodIndex, 0, State.Events.Num() - 1);
		const auto& E = State.Events[ReportPeriodIndex];
		SetSharedReportText(FText::FromString(FString::Printf(
		    TEXT("%s %s | Day %lld\n%d buildings affected | %s"), FCityDisasterSimulation::Name(E.Kind),
		    E.bTest ? TEXT("test") : TEXT("random"), E.StartStep, E.Affected.Num(),
		    State.LastFixedStepId < E.StartStep ? TEXT("warning")
		    : State.LastFixedStepId < E.EndStep ? TEXT("active")
		                                        : TEXT("ended"))));
		SetSharedReportRange(FText::FromString(
		    E.Profile >= 2
		        ? FString::Printf(TEXT("Event %d/%d | Severity %d%% | %lld days"), ReportPeriodIndex + 1,
		                          State.Events.Num(), E.SeverityPercent, E.EndStep - E.StartStep)
		        : FString::Printf(TEXT("Event %d/%d | Original profile | %lld days"), ReportPeriodIndex + 1,
		                          State.Events.Num(), E.EndStep - E.StartStep)));
		return;
	}
	const auto& History = Simulation->GetReportHistory();
	auto Samples = History.Samples(ReportPeriod);
	// Category lists preserve the stable metric save order while grouping filled jobs with staff.
	static const TArray<ECityReportMetric> Categories[] = {
	    {ECityReportMetric::Population, ECityReportMetric::FilledJobs, ECityReportMetric::Homeless,
	     ECityReportMetric::Unemployed, ECityReportMetric::ResidentEducated, ECityReportMetric::ResidentNone,
	     ECityReportMetric::ResidentBasic, ECityReportMetric::ResidentSecondary,
	     ECityReportMetric::ResidentHigher, ECityReportMetric::Arsons, ECityReportMetric::Arrests,
	     ECityReportMetric::Jailed, ECityReportMetric::RelocatedResidents,
	     ECityReportMetric::DepartedResidents},
	    {ECityReportMetric::FilledJobs, ECityReportMetric::StaffEducated, ECityReportMetric::StaffNone,
	     ECityReportMetric::StaffBasic, ECityReportMetric::StaffSecondary, ECityReportMetric::StaffHigher},
	    {ECityReportMetric::Treasury, ECityReportMetric::Income, ECityReportMetric::Upkeep,
	     ECityReportMetric::WagesDue, ECityReportMetric::WagesPaid, ECityReportMetric::MovingExpenses,
	     ECityReportMetric::DepartureMoney, ECityReportMetric::LoanPrincipal, ECityReportMetric::LoanArrears,
	     ECityReportMetric::LoanBorrowed, ECityReportMetric::LoanPrincipalPaid,
	     ECityReportMetric::LoanInterestPaid, ECityReportMetric::LoanInterestAccrued,
	     ECityReportMetric::PolicyDue, ECityReportMetric::PolicyAdministrationPaid,
	     ECityReportMetric::ReserveAssistancePaid, ECityReportMetric::SchoolBursariesPaid,
	     ECityReportMetric::PolicyRecipientPayments, ECityReportMetric::UnfundedPolicies},
	    {ECityReportMetric::GoodsProduced, ECityReportMetric::GoodsPurchased,
	     ECityReportMetric::GoodsExported, ECityReportMetric::Inventory},
	    {ECityReportMetric::PowerDemand, ECityReportMetric::PowerCapacity, ECityReportMetric::WaterDemand,
	     ECityReportMetric::WaterCapacity, ECityReportMetric::Pollution, ECityReportMetric::WastePiled,
	     ECityReportMetric::WasteCargo, ECityReportMetric::WasteStorage, ECityReportMetric::WasteGenerated,
	     ECityReportMetric::WasteCollected, ECityReportMetric::WasteDisposed,
	     ECityReportMetric::WasteRecycled, ECityReportMetric::WasteOverflow},
	    {ECityReportMetric::IllResidents, ECityReportMetric::SevereIllness, ECityReportMetric::Inpatients,
	     ECityReportMetric::RemotePatients, ECityReportMetric::MedicalLeave, ECityReportMetric::Recovered,
	     ECityReportMetric::HospitalCapacity}};
	const auto& Metrics = Categories[ReportCategory];
	ReportMetricIndex = (ReportMetricIndex % Metrics.Num() + Metrics.Num()) % Metrics.Num();
	const auto Metric = Metrics[ReportMetricIndex];
	if (Metric >= ECityReportMetric::PolicyDue)
		Samples.RemoveAll([](const auto& S) { return !S.bHasPolicies; });
	else if (Metric >= ECityReportMetric::LoanPrincipal)
		Samples.RemoveAll([](const auto& S) { return !S.bHasLoans; });
	else if (Metric >= ECityReportMetric::RelocatedResidents)
		Samples.RemoveAll([](const auto& S) { return !S.bHasMigration; });
	else if (Metric >= ECityReportMetric::WastePiled)
		Samples.RemoveAll([](const auto& S) { return !S.bHasWaste; });
	else if (Metric >= ECityReportMetric::IllResidents)
		Samples.RemoveAll([](const auto& S) { return !S.bHasHealth; });
	else if (Metric >= ECityReportMetric::Arsons)
		Samples.RemoveAll([](const auto& S) { return !S.bHasSafety; });
	if (Samples.IsEmpty())
	{
		ReportPeriodIndex = INDEX_NONE;
		SetSharedReportText(FText::FromString(
		    ReportPeriod == ECityReportPeriod::Hourly
		        ? TEXT("No hourly observations yet\nHourly charts appear after the next completed game hour.")
		        : TEXT("No settled days yet\nCharts appear after the first daily settlement\nat midnight.")));
		SetSharedReportRange(FText::FromString(TEXT("Existing cities have no backdated history")));
		return;
	}
	ReportPeriodIndex = ReportPeriodIndex == INDEX_NONE
	                        ? Samples.Num() - 1
	                        : FMath::Clamp(ReportPeriodIndex, 0, Samples.Num() - 1);
	const auto& Selected = Samples[ReportPeriodIndex];
	const FString Unit = FCityReportHistory::Unit(Metric);
	const FString Period = ReportPeriod == ECityReportPeriod::Hourly
	    ? FString::Printf(TEXT("Hour %lld (%s observation)"), Selected.MonthId,
	        Selected.HourStartStep == FCityCalendar::HourEndStep(Selected.MonthId - 1, Simulation->GetEconomyTuning().FixedStepsPerMonth)
	            ? TEXT("complete") : TEXT("partial"))
	    : FString::Printf(TEXT("%s %lld (%d/%d days observed)"),
	    FCityReportHistory::PeriodName(ReportPeriod), Selected.MonthId, Selected.ObservedDays,
	    FCityReportHistory::PeriodDays(ReportPeriod));
	SetSharedReportText(FText::FromString(FString::Printf(
	    TEXT("%s  |  %s\n%lld %s  -  %s"), *Period, FCityReportHistory::DisplayName(Metric),
	    Selected.Get(Metric), *Unit,
	    FCityReportHistory::IsFlow(Metric) ? TEXT("observed total") : ReportPeriod == ECityReportPeriod::Hourly ? TEXT("hourly observation") : TEXT("latest daily close"))));
	SharedReportChart = CityUiContent::ReportChart(Samples, Metric, ReportPeriodIndex, ReportPeriod);
	const double ZeroZ = 28 + SharedReportChart.Zero * 14;
	ReportZeroLine->SetRelativeLocation(FVector(1.1, 0, ZeroZ));
	ReportZeroLine->SetRelativeScale3D(FVector(.003, .60, .004));
	ReportZeroLine->SetVisibility(true);
	for (int32 I = 0; I < SharedReportChart.Bars.Num(); ++I)
	{
		auto* Bar = ReportBars[I].Get();
		const double ValueZ = 28 + SharedReportChart.Bars[I] * 14;
		const double Height = FMath::Max(.12, FMath::Abs(ValueZ - ZeroZ));
		Bar->SetRelativeLocation(FVector(1.4, 29 - I * 2.5, (ZeroZ + ValueZ) / 2));
		Bar->SetRelativeScale3D(FVector(.005, I == SharedReportChart.Selected ? .022 : .016, Height / 100));
		Bar->SetMaterial(0, I == SharedReportChart.Selected ? IndustrialDemandMaterial.Get()
		                                                    : CommercialDemandMaterial.Get());
		Bar->SetVisibility(true);
	}
	SetSharedReportRange(SharedReportChart.Range);
}

void ACityVRToolController::ShowReports(const ECityReportPeriod Period)
{
	if (bEntrySceneActive)
		return;
	ReportPeriod = Period;
	ReportPeriodIndex = INDEX_NONE;
	SetPaletteOpen(true);
	SetPalettePage(ECityPalettePage::Reports);
	UpdatePaletteAttachment();
}

FString ACityVRToolController::GetPaletteEntryHelp(const FCityPaletteEntry& Entry) const
{
	FString Help = Entry.Help;
	const FName Id = FCityPaletteModel::ConstructionDefinition(Entry.Action);
	if (Board && !Id.IsNone())
	{
		const FString Cost = Board->DescribeConstructionCost(Id);
		if (!Cost.IsEmpty())
			Help = Cost + TEXT("\n") + Help;
		if (PalettePage == ECityPalettePage::TechnologyFacilities)
			if (const auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
				if (const auto* Catalog = Simulation->GetBuildingCatalog())
					if (const auto* Definition = Catalog->FindDefinition(Id))
					{
						const auto& Services = Simulation->GetServiceSnapshot();
						Help += Definition->PrerequisitesMet(&Services) ? TEXT("\nBUILD READY") : TEXT("\nLOCKED");
						const FString Requirements = Definition->DescribePrerequisites(Services, Catalog);
						if (!Requirements.IsEmpty())
							Help += TEXT("\n") + Requirements;
					}
	}
	return Help;
}

FString ACityVRToolController::GetPaletteHelp() const
{
	if (bDesktopUi && PalettePage == ECityPalettePage::Graphics)
		return DesktopVideo.Description(FPlatformTime::Seconds());
	FString Help =
	    bDesktopUi ? TEXT("Select a tool; Esc cancels") : TEXT("Select a tool; release grip to use it");
	if (bDesktopUi && PalettePage == ECityPalettePage::Zone)
		Help = TEXT("Choose zoning, then paint on the map. Z cycles Residential / Commercial / Industrial; "
		            "Esc closes.");
	if (VisiblePaletteEntries.IsValidIndex(HoveredPaletteIndex))
	{
		Help = GetPaletteEntryHelp(VisiblePaletteEntries[HoveredPaletteIndex]);
	}
	if (PalettePage == ECityPalettePage::Taxes)
	{
		if (const auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		{
			const auto Budget = Simulation->GetProjectedBudget(ResidentialTaxControl.GetPendingValue(),
			                                                   CommercialTaxControl.GetPendingValue(),
			                                                   IndustrialTaxControl.GetPendingValue());
			Help = FString::Printf(
			    TEXT("Forecast net %s$%lld/day (current city)\n0-25%%; Apply. Higher tax slows demand"),
			    Budget.Net() >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Budget.Net()));
		}
	}
	if (PalettePage == ECityPalettePage::Reports)
		Help = bDesktopUi
		           ? TEXT("Settled history; first observed month may be partial. Esc closes")
		           : TEXT("Settled history; first observed month may be partial. Release grip to dismiss");
	if (FCityPaletteModel::IsEntryPage(PalettePage))
		Help = GetEntryMenuHelp();
	return Help;
}
