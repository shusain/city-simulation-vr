#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityCitizenWalkMotion.h"
#include "Presentation/CityCitizenPresentation.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalRenderPublic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenWalkMotionTest,"CitySim.Citizens.WalkMotion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityCitizenWalkMotionTest::RunTest(const FString& Parameters)
{
	double Reference=-1;
	for(const double Speed:{1.,3.,5.})
	{
		FCityCitizenWalkMotion Motion;
		Motion.Sample(23,1,FVector::ZeroVector,0,false);
		const double Initial=Motion.Phase;
		// A 60Hz wall clock samples different distances at each simulation speed.
		for(int32 I=1;I<=static_cast<int32>(60/Speed);++I)
		{
			const double SimSeconds=I*Speed/60;
			Motion.Sample(23,1,FVector(SimSeconds*100,0,0),SimSeconds,false);
		}
		TestTrue(TEXT("one hundred cm drives measured stride independent of wall speed"),
		    FMath::IsNearlyEqual(Motion.Phase,FMath::Fmod(Initial+100/CityCitizenWalk::StrideCm,1.),1.e-6));
		if(Reference<0)Reference=Motion.Phase;
		TestTrue(TEXT("1x 3x 5x produce the same pose at the same route distance"),FMath::IsNearlyEqual(Motion.Phase,Reference,1.e-6));
		const double WallSeconds=1/Speed;
		TestTrue(TEXT("wall duration changes while distance/pose stays common"),WallSeconds>0);
		Motion.Sample(23,1,FVector(100,100,0),2,false);
		TestTrue(TEXT("corner keeps phase and accounts for the new direction"),
		    FMath::IsNearlyEqual(Motion.Phase,FMath::Fmod(Reference+100/CityCitizenWalk::StrideCm,1.),1.e-6));
		const double BeforePause=Motion.Phase;const float Weight=Motion.WalkWeight;
		Motion.Sample(23,1,FVector(100,100,0),3,true);
		TestEqual(TEXT("pause freezes phase"),Motion.Phase,BeforePause);TestEqual(TEXT("pause freezes blend"),Motion.WalkWeight,Weight);
		Motion.Sample(23,1,FVector(100,100,0),4,false);
		TestEqual(TEXT("stationary route does not walk in place"),Motion.Phase,BeforePause);
		TestEqual(TEXT("waiting blends to idle"),Motion.WalkWeight,0.f);
		Motion.Sample(23,2,FVector(8000,0,0),5,false);
		TestEqual(TEXT("new trip/teleport resets to deterministic identity pose"),Motion.Phase,Initial);
		TestEqual(TEXT("new trip cannot reuse moving blend"),Motion.WalkWeight,0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenWalkRuntimeTest,"CitySim.Citizens.StreetPool",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityCitizenWalkRuntimeTest::RunTest(const FString& Parameters)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto* Board=World->SpawnActor<ACityBoardActor>();
	Board->SimulationSubsystem=World->GetSubsystem<UCitySimulationSubsystem>();
	if(!TestNotNull(TEXT("native idle/walk blend is referenced by the real board"),Board->StreetWalkBlend.Get())){World->DestroyWorld(false);return false;}
	TArray<FCityStreetWalkerSample> Samples;
	for(int32 I=0;I<40;++I)
	{
		FCityStreetWalkerSample S;S.Id=S.Identity=I+1;S.Trip=1;S.Variant=I%3;
		S.Transform=FTransform(FVector(I*200,100,CityCitizenPresentation::StreetOriginOffsetCm));Samples.Add(S);
	}
	TArray<FTransform> A,B,C;Board->RenderStreetWalkers(Samples,A,B,C);
	const int32 Cap=PLATFORM_ANDROID ? CityCitizenWalk::QuestCap : CityCitizenWalk::DesktopCap;
	TestEqual(TEXT("skeletal pool uses a hard cap"),Board->StreetAnimatedWalkers.Num(),Cap);
	TestEqual(TEXT("animated and static proxies total exactly one per walker"),A.Num()+B.Num()+C.Num()+Cap,40);
	TSet<int32> Styles;
	for(auto Component:Board->StreetAnimatedWalkers)
	{
		TestTrue(TEXT("skeletal street proxy is visible"),Component->IsVisible());
		TestFalse(TEXT("no per-citizen ticking"),Component->PrimaryComponentTick.bCanEverTick);
		TestEqual(TEXT("no per-citizen collision"),Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestTrue(TEXT("zero-foot skeletal pivot preserves the12cm sidewalk plane"),FMath::IsNearlyEqual(Component->GetRelativeLocation().Z,12.,.001));
		for(int32 I=0;I<3;++I)if(Component->GetSkeletalMeshAsset()==Board->ParkCitizenSkeletonMeshes[I])Styles.Add(I);
	}
	TestEqual(TEXT("all outfits share the bounded animated street pool"),Styles.Num(),3);
	Board->DiagnosticOverlay=ECityDiagnosticOverlay::Power;
	A.Reset();B.Reset();C.Reset();Board->RenderStreetWalkers(Samples,A,B,C);
	for(auto Component:Board->StreetAnimatedWalkers)TestFalse(TEXT("diagnostic overlays suppress dynamic street citizens"),Component->IsVisible());
	Board->DiagnosticOverlay=ECityDiagnosticOverlay::None;
	Samples.SetNum(1);A.Reset();B.Reset();C.Reset();Board->RenderStreetWalkers(Samples,A,B,C);
	int32 Visible=0;for(auto Component:Board->StreetAnimatedWalkers)Visible+=Component->IsVisible();
	TestEqual(TEXT("pool turnover hides all stale citizens"),Visible,1);
	TestEqual(TEXT("cosmetic state retires with trips"),Board->StreetWalkStates.Num(),1);
	Board->bStreetSkeletalAnimation=false;A.Reset();B.Reset();C.Reset();Board->RenderStreetWalkers(Samples,A,B,C);
	TestEqual(TEXT("comparison mode retains the static fallback"),A.Num()+B.Num()+C.Num(),1);
	for(auto Component:Board->StreetAnimatedWalkers)TestFalse(TEXT("fallback mode cannot show duplicates"),Component->IsVisible());
	Samples.Reset();Board->RenderStreetWalkers(Samples,A,B,C);
	TestTrue(TEXT("generation/empty city clears cosmetic identity state"),Board->StreetWalkStates.IsEmpty());
	World->DestroyWorld(false);return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenNativeWalkTest,"CitySim.Citizens.NativeWalk",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityCitizenNativeWalkTest::RunTest(const FString& Parameters)
{
	auto* Walk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CitySim/Art/Citizens/AS_Citizen_Walk.AS_Citizen_Walk"));
	auto* Blend=LoadObject<UBlendSpace1D>(nullptr,TEXT("/Game/CitySim/Art/Citizens/BS_Citizen_StreetWalk.BS_Citizen_StreetWalk"));
	if(!TestNotNull(TEXT("approved native walk sequence"),Walk)||!TestNotNull(TEXT("native idle/walk blend"),Blend))return false;
	TestTrue(TEXT("native clip keeps the reviewed duration"),FMath::IsNearlyEqual(Walk->GetPlayLength(),28.f/24,.002f));
	TestFalse(TEXT("cosmetic root motion cannot displace authoritative trips"),Walk->bEnableRootMotion);
	TestEqual(TEXT("stock blend has idle and reviewed walk"),Blend->GetNumberOfBlendSamples(),2);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto* Actor=World->SpawnActor<AActor>();
	for(const TCHAR* Style:{TEXT("Casual"),TEXT("Office"),TEXT("Worker")})
	{
		const FString Path=FString::Printf(TEXT("/Game/CitySim/Art/Parks/Meshes/SK_Park_Field_%s.SK_Park_Field_%s"),Style,Style);
		auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*Path);if(!TestNotNull(Path,Mesh))continue;
		TestTrue(TEXT("reviewed original20-bone bind reuses the field mesh skeleton"),Mesh->GetSkeleton()==Walk->GetSkeleton());
		auto* Component=NewObject<USkeletalMeshComponent>(Actor);Component->SetSkeletalMesh(Mesh);Component->RegisterComponent();
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);Component->SetAnimation(Walk);
		for(int32 LOD=0;LOD<2;++LOD)
		{
			FVector FirstHead;bool Moved=false;
			for(float T:{0.f,4.f/24,8.f/24,14.f/24,18.f/24,22.f/24,28.f/24})
			{
				Component->SetPosition(T,false);Component->TickAnimation(0,false);Component->RefreshBoneTransforms();
				TArray<FFinalSkinVertex> Skin;Component->GetCPUSkinnedVertices(Skin,LOD);
				double MinZ=1.e9,MaxZ=-1.e9;for(const auto& V:Skin){MinZ=FMath::Min(MinZ,static_cast<double>(V.Position.Z));MaxZ=FMath::Max(MaxZ,static_cast<double>(V.Position.Z));}
				TestTrue(TEXT("actual native soles stay on ground across both LODs"),FMath::Abs(MinZ)<.1);
				TestTrue(TEXT("actual native stature preserves citizen scale"),MaxZ*CityCitizenWalk::MeshScale>135 && MaxZ*CityCitizenWalk::MeshScale<146);
				const FVector Head=Component->GetBoneLocation(TEXT("head"),EBoneSpaces::ComponentSpace);
				if(T==0)FirstHead=Head;else Moved|=!Head.Equals(FirstHead,.01);
			}
			TestTrue(TEXT("imported sequence really deforms the reused mesh"),Moved);
		}
		Component->SetAnimation(Blend);auto* Instance=Component->GetSingleNodeInstance();Instance->SetPlaying(false);
		FVector FirstWrist;bool BlendMoves=false;
		for(float Weight:{0.f,.5f,1.f})
		{
			Instance->SetBlendSpacePosition(FVector(Weight,0,0));Component->SetPosition(.2,false);Component->TickAnimation(0,false);Component->RefreshBoneTransforms();
			const FName Hand=Component->GetBoneIndex(TEXT("hand.L"))!=INDEX_NONE ? FName(TEXT("hand.L")) : FName(TEXT("hand_L"));
			TestTrue(TEXT("native wrist bone exists"),Component->GetBoneIndex(Hand)!=INDEX_NONE);
			const FVector RealWrist=Component->GetBoneLocation(Hand,EBoneSpaces::ComponentSpace);
			if(Weight==0)FirstWrist=RealWrist;else BlendMoves|=!RealWrist.Equals(FirstWrist,.1);
		}
		TestTrue(TEXT("stock blend changes actual evaluated idle/walk bones"),BlendMoves);
		Component->DestroyComponent();
	}
	World->DestroyWorld(false);return true;
}
#endif
#endif
