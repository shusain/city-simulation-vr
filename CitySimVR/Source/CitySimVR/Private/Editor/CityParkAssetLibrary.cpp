#include "Editor/CityParkAssetLibrary.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Presentation/CityBoardActor.h"

bool UCityParkAssetLibrary::BindMatchingParkSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton)
{
#if WITH_EDITOR
	if (!Mesh || !Skeleton || Mesh->GetRefSkeleton().GetNum() != 20 || Skeleton->GetReferenceSkeleton().GetNum() != 20)
		return false;
	const auto& A = Mesh->GetRefSkeleton();
	const auto& B = Skeleton->GetReferenceSkeleton();
	for (int32 Index = 0; Index < 20; ++Index)
		if (A.GetBoneName(Index) != B.GetBoneName(Index) || A.GetParentIndex(Index) != B.GetParentIndex(Index) ||
		    !A.GetRefBonePose()[Index].Equals(B.GetRefBonePose()[Index], .01))
			return false;
	Mesh->Modify();
	Mesh->SetSkeleton(Skeleton);
	Mesh->PostEditChange();
	return true;
#else
	return false;
#endif
}

bool UCityParkAssetLibrary::SetParkCaptureClock(ACityBoardActor* Board, const double Seconds)
{
#if WITH_EDITOR
	if (!Board || !FMath::IsFinite(Seconds) || Seconds < 0 ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityParkAnimationPreview")))
		return false;
	Board->UpdateParkActivity(0);
	Board->ParkActivitySeconds = Seconds;
	Board->ParkPoseAccumulator = 1;
	Board->UpdateParkActivity(0);
	return true;
#else
	return false;
#endif
}

bool UCityParkAssetLibrary::BindMatchingParkAnimation(UAnimSequence* Sequence, USkeleton* Skeleton)
{
#if WITH_EDITOR
	if (Sequence && Skeleton && Sequence->GetSkeleton() == Skeleton && Skeleton->GetReferenceSkeleton().GetNum() == 20)
		return true;
	if (!Sequence || !Sequence->GetSkeleton() || !Skeleton)
		return false;
	const auto& A = Sequence->GetSkeleton()->GetReferenceSkeleton();
	const auto& B = Skeleton->GetReferenceSkeleton();
	if (A.GetNum() != 20 || B.GetNum() != 20)
		return false;
	for (int32 Index = 0; Index < 20; ++Index)
		if (A.GetBoneName(Index) != B.GetBoneName(Index) || A.GetParentIndex(Index) != B.GetParentIndex(Index) ||
		    !A.GetRefBonePose()[Index].Equals(B.GetRefBonePose()[Index], .01))
			return false;
	Sequence->Modify();
	Sequence->SetSkeleton(Skeleton);
	Sequence->PostEditChange();
	return true;
#else
	return false;
#endif
}
