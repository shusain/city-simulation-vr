#pragma once

#include "CoreMinimal.h"

struct FCityEconomySnapshot;
struct FCityNeedsSnapshot;
struct FCityBudgetProjection;

class CITYSIMVR_API FCityNeedsPresentation
{
public:
	static FString BuildRatioBar(int64 Value, int64 Capacity, int32 Width = 8, const TCHAR* ZeroLabel = TEXT("NO CAP"));
	static FString BuildSignedBar(int64 Value, int64 AbsoluteBound, int32 HalfWidth = 4);
	static double NormalizeRatioValue(int64 Value, int64 Capacity);
	static double NormalizeSignedValue(int64 Value, int64 AbsoluteBound);
	static FString BuildGuidance(const FCityNeedsSnapshot& Needs, const FCityEconomySnapshot& Economy);
	static FString BuildDensitySummary(const FCityNeedsSnapshot& Needs);
	static FString BuildPanel(const FCityNeedsSnapshot& Needs, const FCityEconomySnapshot& Economy,
	                          int64 ProjectedResidentialTax = 0, int64 ProjectedCommercialTax = 0,
	                          const FCityBudgetProjection* Budget = nullptr);
};
