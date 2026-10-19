#pragma once
#include "CoreMinimal.h"

enum class ECityParkActivityProp : uint8;
enum class ECityParkMotion : uint8
{
	None,
	Idle,
	Walk,
	SkateCarve,
	SkatePush,
	SkateKickflip,
	Skate180,
	SkateLipStall,
	BaseballPitch,
	BaseballSwing,
	SoccerKick,
	SoccerRun,
	GoalkeeperSave,
	BasketballDribble,
	BasketballShot
};

struct FCityParkMotionSample
{
	FTransform Body;
	FTransform Props[2];
	ECityParkActivityProp Kinds[2];
	int32 PropCount = 0;
	float AnimationSeconds = 0;
};

class CITYSIMVR_API FCityParkMotion
{
public:
	static constexpr int32 ClipCount = 14;
	static constexpr int32 DesktopAnimationCap = 32;
	static constexpr int32 QuestAnimationCap = 8;
	static bool Sample(ECityParkMotion Motion, double Seconds, FCityParkMotionSample& Out);
	static bool IsSkate(ECityParkMotion Motion)
	{
		return Motion >= ECityParkMotion::SkateCarve && Motion <= ECityParkMotion::SkateLipStall;
	}
};
