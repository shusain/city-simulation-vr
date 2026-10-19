#include "Editor/CityCitizenWalkAssetLibrary.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif

UBlendSpace1D* UCityCitizenWalkAssetLibrary::CreateWalkBlend(UAnimSequence* Idle, UAnimSequence* Walk)
{
#if WITH_EDITOR
	if (!Idle || !Walk || Idle->GetSkeleton()!=Walk->GetSkeleton()) return nullptr;
	const TCHAR* Path=TEXT("/Game/CitySim/Art/Citizens/BS_Citizen_StreetWalk");
	if (LoadObject<UBlendSpace1D>(nullptr,*(FString(Path)+TEXT(".BS_Citizen_StreetWalk")))) return nullptr;
	auto* Package=CreatePackage(Path);
	auto* Blend=NewObject<UBlendSpace1D>(Package,TEXT("BS_Citizen_StreetWalk"),RF_Public|RF_Standalone);
	Blend->SetSkeleton(Walk->GetSkeleton());
	auto& Axis=const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));
	Axis.DisplayName=TEXT("WalkWeight"); Axis.Min=0; Axis.Max=1;
	Blend->AddSample(Idle,FVector(0,0,0)); Blend->AddSample(Walk,FVector(1,0,0));
	Blend->ValidateSampleData(); Blend->ResampleData(); Blend->PostEditChange();
	FAssetRegistryModule::AssetCreated(Blend); Package->MarkPackageDirty();
	return Blend;
#else
	return nullptr;
#endif
}

bool UCityCitizenWalkAssetLibrary::RepairWalkBlendReferences()
{
#if WITH_EDITOR
	const FString BlendPackage = TEXT("/Game/CitySim/Art/Citizens/BS_Citizen_StreetWalk");
	auto* Blend = LoadObject<UBlendSpace1D>(nullptr, *(BlendPackage + TEXT(".BS_Citizen_StreetWalk")));
	auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySim/Art/Parks/Animations/AS_Park_Idle.AS_Park_Idle"));
	auto* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySim/Art/Citizens/AS_Citizen_Walk.AS_Citizen_Walk"));
	if (!Blend || !Idle || !Walk || Blend->GetNumberOfBlendSamples() != 2 ||
		Idle->GetSkeleton() != Walk->GetSkeleton() || Blend->GetSkeleton() != Walk->GetSkeleton()) return false;
	FBlendSample BeforeIdle = Blend->GetBlendSample(0);
	const FBlendSample BeforeWalk = Blend->GetBlendSample(1);
	if (!BeforeIdle.SampleValue.Equals(FVector::ZeroVector) || !BeforeWalk.SampleValue.Equals(FVector(1,0,0)) || BeforeWalk.Animation != Walk) return false;
	if (BeforeIdle.Animation == Idle) return true;
	if (BeforeIdle.Animation) return false;
	Blend->Modify();
	if (!Blend->ReplaceSampleAnimation(0, Idle)) return false;
	Blend->ValidateSampleData();
	Blend->ResampleData();
	Blend->PostEditChange();
	BeforeIdle.Animation = Idle;
	if (Blend->GetNumberOfBlendSamples() != 2 || !(Blend->GetBlendSample(0) == BeforeIdle) || !(Blend->GetBlendSample(1) == BeforeWalk)) return false;
	Blend->MarkPackageDirty();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(BlendPackage, FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Blend->GetOutermost(), Blend, *Filename, SaveArgs);
#else
	return false;
#endif
}
