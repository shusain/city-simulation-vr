#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityVRToolController.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPresentationShadowsTest, "CitySim.Presentation.HudAndPlayerShadows",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPresentationShadowsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityPresentationShadowsTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACityVRToolController* Hud = World->SpawnActor<ACityVRToolController>();
	UClass* XrClass = LoadClass<APawn>(nullptr, TEXT("/Game/CitySim/XR/BP_CityXRPawn.BP_CityXRPawn_C"));
	APawn* Pawn = XrClass ? World->SpawnActor<APawn>(XrClass) : nullptr;
	if (!TestNotNull(TEXT("Gameplay XR pawn asset spawns"), Pawn))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	APlayerController* Player = World->SpawnActor<APlayerController>();
	Player->SetPawn(Pawn);
	AActor* WorldObject = World->SpawnActor<AActor>();
	UStaticMeshComponent* WorldMesh = NewObject<UStaticMeshComponent>(WorldObject);
	WorldObject->SetRootComponent(WorldMesh);
	WorldMesh->RegisterComponent();
	WorldMesh->SetCastShadow(true);
	WorldMesh->SetCastHiddenShadow(true);
	WorldMesh->SetCastContactShadow(true);
	WorldMesh->AttachToComponent(Pawn->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

	// Cover nested presentation components without modifying template asset defaults.
	UChildActorComponent* Child = NewObject<UChildActorComponent>(Pawn);
	Child->SetupAttachment(Pawn->GetRootComponent());
	Child->SetChildActorClass(AActor::StaticClass());
	Child->RegisterComponent();
	AActor* ChildActor = Child->GetChildActor();
	if (!TestNotNull(TEXT("Nested player visual actor spawns"), ChildActor))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UStaticMeshComponent* ChildMesh = NewObject<UStaticMeshComponent>(ChildActor);
	ChildActor->SetRootComponent(ChildMesh);
	ChildMesh->RegisterComponent();
	ChildMesh->SetCastShadow(true);
	ChildMesh->SetCastHiddenShadow(true);
	ChildMesh->SetCastContactShadow(true);
	const bool bChildVisible = ChildMesh->IsVisible();
	const ECollisionEnabled::Type ChildCollision = ChildMesh->GetCollisionEnabled();

	World->InitializeActorsForPlay(FURL());
	Hud->FindMotionControllers();
	TInlineComponentArray<UPrimitiveComponent*> HudPrimitives;
	Hud->GetComponents(HudPrimitives, true);
	TestTrue(TEXT("HUD coverage includes all panels, text and status bars"), HudPrimitives.Num() > 100);
	for (const UPrimitiveComponent* Primitive : HudPrimitives)
	{
		TestFalse(*FString::Printf(TEXT("HUD %s casts no shadow"), *Primitive->GetName()),
		          Primitive->CastShadow || Primitive->bCastHiddenShadow || Primitive->bCastContactShadow);
	}
	TInlineComponentArray<UPrimitiveComponent*> PawnPrimitives;
	Pawn->GetComponents(PawnPrimitives, true);
	int32 SkeletalMeshes = 0;
	for (const UPrimitiveComponent* Primitive : PawnPrimitives)
	{
		SkeletalMeshes += Primitive->IsA<USkeletalMeshComponent>() ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_PLAYER_SHADOW_CHECK Owner=%s Component=%s Class=%s"),
		       *Primitive->GetOwner()->GetName(), *Primitive->GetName(), *Primitive->GetClass()->GetName());
		TestFalse(*FString::Printf(TEXT("Player %s casts no shadow"), *Primitive->GetName()),
		          Primitive->CastShadow || Primitive->bCastHiddenShadow || Primitive->bCastContactShadow);
	}
	TestTrue(TEXT("Actual XR hand meshes are covered"), SkeletalMeshes >= 2);
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_PRESENTATION_SHADOW_CHECK HudPrimitives=%d PlayerPrimitives=%d HandMeshes=%d"),
	       HudPrimitives.Num(), PawnPrimitives.Num(), SkeletalMeshes);
	TestEqual(TEXT("Visual visibility is preserved"), ChildMesh->IsVisible(), bChildVisible);
	TestEqual(TEXT("Visual collision is preserved"), ChildMesh->GetCollisionEnabled(), ChildCollision);
	TestTrue(TEXT("Attached world object keeps all shadow flags"),
	         WorldMesh->CastShadow && WorldMesh->bCastHiddenShadow && WorldMesh->bCastContactShadow);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
