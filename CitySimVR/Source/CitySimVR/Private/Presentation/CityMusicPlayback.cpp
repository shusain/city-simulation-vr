#include "Presentation/CityMusicPlayback.h"

bool FCityMusicPlayback::Configure(const TArray<double>& Durations)
{
	Lengths.Reset();
	Offset = DoubleCycle = 0;
	Track = PrimarySlot = 0;
	if (Durations.Num() < 2)
		return false;
	for (double Duration : Durations)
	{
		if (!FMath::IsFinite(Duration) || Duration <= CrossfadeSeconds * 2)
			return false;
		DoubleCycle += (Duration - CrossfadeSeconds) * 2;
	}
	Lengths = Durations;
	return true;
}
void FCityMusicPlayback::Advance(double Seconds, bool bSuspended)
{
	if (bSuspended || Lengths.IsEmpty() || !FMath::IsFinite(Seconds) || Seconds <= 0)
		return;
	// Removing two complete playlist cycles preserves both track identity and player-slot parity.
	Offset += FMath::Fmod(Seconds, DoubleCycle);
	while (Offset >= Lengths[Track])
	{
		Offset -= Lengths[Track] - CrossfadeSeconds;
		Track = (Track + 1) % Lengths.Num();
		PrimarySlot = 1 - PrimarySlot;
	}
}
FCityMusicVoice FCityMusicPlayback::Voice(int32 Slot) const
{
	if (Lengths.IsEmpty() || Slot < 0 || Slot > 1)
		return {};
	const double NextOffset = Offset - (Lengths[Track] - CrossfadeSeconds);
	const float Blend = FMath::Clamp(NextOffset / CrossfadeSeconds, 0.0, 1.0);
	if (Slot == PrimarySlot)
		return {Track, Offset, 1.f - Blend};
	if (NextOffset >= 0)
		return {(Track + 1) % Lengths.Num(), NextOffset, Blend};
	return {};
}
