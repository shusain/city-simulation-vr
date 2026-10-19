#pragma once
#include "CoreMinimal.h"

// Bounded-memory frame percentiles; bucket upper bounds are accurate to .05ms.
struct FCityFrameTiming
{
	static constexpr double ResolutionMs = .05;
	static constexpr int32 LastBucket = 20000;
	TArray<uint64> Buckets;
	uint64 Count = 0;
	double SumMs = 0, PeakMs = 0;
	FCityFrameTiming()
	{
		Buckets.Init(0, LastBucket + 1);
	}
	void Add(double Ms)
	{
		if (!FMath::IsFinite(Ms) || Ms <= 0)
			return;
		++Count;
		SumMs += Ms;
		PeakMs = FMath::Max(PeakMs, Ms);
		++Buckets[FMath::Clamp(FMath::CeilToInt(Ms / ResolutionMs), 0, LastBucket)];
	}
	double Percentile(double Percent) const
	{
		if (!Count)
			return -1;
		const uint64 Rank =
		    FMath::Max<uint64>(1, FMath::CeilToDouble(Count * FMath::Clamp(Percent, 0.0, 100.0) / 100.0));
		uint64 Seen = 0;
		for (int32 Index = 0; Index <= LastBucket; ++Index)
		{
			Seen += Buckets[Index];
			if (Seen >= Rank)
				return Index == LastBucket ? PeakMs : Index * ResolutionMs;
		}
		return PeakMs;
	}
};

// Optional benchmark observer; normal play does not read the clock.
class FCityFrameTimingScope
{
public:
	explicit FCityFrameTimingScope(FCityFrameTiming* InTiming)
	    : Timing(InTiming), Start(InTiming ? FPlatformTime::Seconds() : 0)
	{
	}
	~FCityFrameTimingScope()
	{
		if (Timing)
			Timing->Add((FPlatformTime::Seconds() - Start) * 1000);
	}

private:
	FCityFrameTiming* Timing;
	double Start;
};
