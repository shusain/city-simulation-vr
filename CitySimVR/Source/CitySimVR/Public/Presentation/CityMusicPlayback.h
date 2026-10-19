#pragma once
#include "CoreMinimal.h"

struct FCityMusicVoice
{
	int32 Track = INDEX_NONE;
	double Offset = 0;
	float Gain = 0;
};
// Presentation clock only. The owner supplies wall time; city identity/speed never enter this state.
class CITYSIMVR_API FCityMusicPlayback
{
public:
	static constexpr double CrossfadeSeconds = 4;
	bool Configure(const TArray<double>& Durations);
	void Advance(double Seconds, bool bSuspended);
	FCityMusicVoice Voice(int32 Slot) const;
	int32 GetTrack() const
	{
		return Track;
	}
	double GetOffset() const
	{
		return Offset;
	}

private:
	TArray<double> Lengths;
	double Offset = 0, DoubleCycle = 0;
	int32 Track = 0, PrimarySlot = 0;
};
