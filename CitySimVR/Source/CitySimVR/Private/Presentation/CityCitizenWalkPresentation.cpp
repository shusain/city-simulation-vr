#include "Presentation/CityBoardActor.h"
#include "Presentation/CityCitizenPresentation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"

void ACityBoardActor::RenderStreetWalkers(const TArray<FCityStreetWalkerSample>& Samples,
    TArray<FTransform>& Casual, TArray<FTransform>& Office, TArray<FTransform>& Worker)
{
	const uint64 Generation=SimulationSubsystem ? SimulationSubsystem->GetPresentationGeneration() : 0;
	const uint64 Network=SimulationSubsystem ? SimulationSubsystem->GetTrafficSnapshot().NetworkFingerprint : 0;
	if (Generation!=StreetWalkGeneration || Network!=StreetWalkNetwork)
	{
		StreetWalkStates.Reset(); StreetWalkGeneration=Generation; StreetWalkNetwork=Network;
		StreetPoseClock=-1;
	}
	const double Clock=SimulationSubsystem ? (SimulationSubsystem->GetTrafficSnapshot().FixedStepId+
	    SimulationSubsystem->GetSimulationStepProgress())*2.4 : 0;
	const bool Paused=!SimulationSubsystem || SimulationSubsystem->IsSimulationPaused();
	const bool PoseDue=StreetPoseClock<0 || Clock<StreetPoseClock ||
	    Clock-StreetPoseClock >= (SimulationSubsystem ? SimulationSubsystem->GetSimulationSpeed() : 1)/(PLATFORM_ANDROID ? 15.0 : 30.0);
	if (PoseDue) StreetPoseClock=Clock;
	const FVector Eye=GetWorld()->GetFirstPlayerController() ?
	    GetWorld()->GetFirstPlayerController()->GetFocalLocation() : GetActorLocation();
	TArray<TPair<double,int32>> Nearest;
	TSet<uint64> Alive;
	for(int32 I=0;I<Samples.Num();++I)
	{
		const auto& Sample=Samples[I]; Alive.Add(Sample.Id);
		StreetWalkStates.FindOrAdd(Sample.Id).Sample(Sample.Identity,Sample.Trip,Sample.Transform.GetLocation(),Clock,Paused);
		if(bStreetSkeletalAnimation && DiagnosticOverlay==ECityDiagnosticOverlay::None && StreetWalkBlend && ParkCitizenSkeletonMeshes[Sample.Variant])
			Nearest.Emplace(FVector::DistSquared(Eye,BoardRoot->GetComponentTransform().TransformPosition(Sample.Transform.GetLocation())),I);
	}
	for(auto It=StreetWalkStates.CreateIterator();It;++It) if(!Alive.Contains(It.Key())) It.RemoveCurrent();
	Nearest.StableSort([](const auto& A,const auto& B){return A.Key<B.Key;});
	TSet<int32> Animated;
	for(int32 I=0;I<FMath::Min(Nearest.Num(),PLATFORM_ANDROID ? CityCitizenWalk::QuestCap : CityCitizenWalk::DesktopCap);++I)
		Animated.Add(Nearest[I].Value);
	int32 Count=0;
	for(int32 I=0;I<Samples.Num();++I)
	{
		const auto& Sample=Samples[I]; const auto& Motion=StreetWalkStates.FindChecked(Sample.Id);
		if(!Animated.Contains(I))
		{
			auto& Pool=Sample.Variant==1 ? Office : Sample.Variant==2 ? Worker : Casual;
			Pool.Add(Sample.Transform); continue;
		}
		if(StreetAnimatedWalkers.Num()<=Count)
		{
			auto* Component=NewObject<USkeletalMeshComponent>(this,*FString::Printf(TEXT("StreetWalker%d"),Count));
			Component->SetupAttachment(BoardRoot); Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCastShadow(false); Component->PrimaryComponentTick.bCanEverTick=false;
			Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			AddInstanceComponent(Component); Component->RegisterComponent(); StreetAnimatedWalkers.Add(Component);
			StreetAssignedIds.Add(0); StreetAssignedPhases.Add(-1); StreetAssignedWeights.Add(-1);
		}
		auto* Component=StreetAnimatedWalkers[Count].Get();
		const bool Reassigned=StreetAssignedIds[Count]!=Sample.Id || Component->GetSkeletalMeshAsset()!=ParkCitizenSkeletonMeshes[Sample.Variant];
		if(Reassigned || !Component->GetSingleNodeInstance())
		{
			Component->SetSkeletalMesh(ParkCitizenSkeletonMeshes[Sample.Variant]);
			Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Component->SetAnimation(StreetWalkBlend);
			Component->GetSingleNodeInstance()->SetPlaying(false);
			StreetAssignedIds[Count]=Sample.Id;
		}
		FTransform Transform=Sample.Transform;
		Transform.AddToTranslation(FVector(0,0,CityCitizenPresentation::StreetFootSupportCm-CityCitizenPresentation::StreetOriginOffsetCm));
		Transform.SetScale3D(FVector(CityCitizenWalk::MeshScale));
		Component->SetRelativeTransform(Transform); Component->SetForcedLOD(PLATFORM_ANDROID ? 2 : 0);
		Component->SetVisibility(true);
		if(Reassigned || (PoseDue && (!FMath::IsNearlyEqual(StreetAssignedPhases[Count],static_cast<float>(Motion.Phase),1.e-6f) ||
		    !FMath::IsNearlyEqual(StreetAssignedWeights[Count],Motion.WalkWeight,1.e-6f))))
		{
			auto* Instance=Component->GetSingleNodeInstance();
			Instance->SetBlendSpacePosition(FVector(Motion.WalkWeight,0,0));
			Component->SetPosition(Motion.Phase,false);
			Component->TickAnimation(0,false); Component->RefreshBoneTransforms(); Component->UpdateBounds();
			StreetAssignedPhases[Count]=Motion.Phase; StreetAssignedWeights[Count]=Motion.WalkWeight;
		}
		++Count;
	}
	for(int32 I=Count;I<StreetAnimatedWalkers.Num();++I) StreetAnimatedWalkers[I]->SetVisibility(false);
}
