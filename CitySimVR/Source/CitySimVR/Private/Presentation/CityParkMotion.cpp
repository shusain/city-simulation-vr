#include "Presentation/CityParkMotion.h"
#include "Presentation/CityParkActivity.h"

namespace
{
	struct FAuthoredParkPose
	{
		FVector Position;
		FQuat Quaternion;
		FVector Scale;
		FTransform Transform() const
		{
			return FTransform(Quaternion.GetNormalized(), Position, Scale);
		}
	};
	struct FAuthoredParkFrame
	{
		FAuthoredParkPose Body;
		FAuthoredParkPose Props[2];
	};
	struct FAuthoredParkClip
	{
		const FAuthoredParkFrame* Frames;
		int32 Period;
		int32 PropCount;
		ECityParkActivityProp Kinds[2];
		bool OneShot;
	};
#include "CityParkMotionData.inl"

	FTransform Interpolate(const FAuthoredParkPose& A, const FAuthoredParkPose& B, const double Alpha)
	{
		FTransform Result;
		Result.Blend(A.Transform(), B.Transform(), Alpha);
		return Result;
	}
} // namespace

bool FCityParkMotion::Sample(const ECityParkMotion Motion, const double Seconds, FCityParkMotionSample& Out)
{
	const int32 Index = static_cast<int32>(Motion) - 1;
	if (Index < 0 || Index >= ClipCount || !FMath::IsFinite(Seconds) || Seconds < 0)
		return false;
	const auto& Clip = AuthoredParkClips[Index];
	const double Duration = Clip.Period / 24.0;
	const double Time = FMath::Fmod(Seconds, Duration + (Clip.OneShot ? .75 : 0));
	const double Frame = FMath::Clamp(Time * 24.0, 0.0, static_cast<double>(Clip.Period));
	const int32 Lower = FMath::Min(FMath::FloorToInt(Frame), Clip.Period - 1);
	const auto& A = Clip.Frames[Lower];
	const auto& B = Clip.Frames[Lower + 1];
	const double Alpha = Frame - Lower;
	Out.Body = Interpolate(A.Body, B.Body, Alpha);
	Out.AnimationSeconds = Frame / 24.0;
	Out.PropCount = Clip.PropCount;
	for (int32 I = 0; I < Clip.PropCount; ++I)
	{
		Out.Props[I] = Interpolate(A.Props[I], B.Props[I], Alpha);
		Out.Kinds[I] = Clip.Kinds[I];
	}
	if (Clip.OneShot && Time > Duration)
	{
		// Continue momentum during a short exit fade; recycle only while hidden.
		const FVector Travel(160 * (Time - Duration), 0, 0);
		const double Fade = 1 - FMath::Clamp((Time - Duration) / .3, 0.0, 1.0);
		Out.Body.AddToTranslation(Travel);
		Out.Body.SetScale3D(Out.Body.GetScale3D() * Fade);
		for (int32 I = 0; I < Clip.PropCount; ++I)
		{
			Out.Props[I].AddToTranslation(Travel);
			Out.Props[I].SetScale3D(Out.Props[I].GetScale3D() * Fade);
		}
	}
	return true;
}
