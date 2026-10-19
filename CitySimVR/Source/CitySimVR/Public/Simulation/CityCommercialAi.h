#pragma once
#include "CoreMinimal.h"

struct FCityPopulationSnapshot;
struct FCityServiceSnapshot;

// Paid, bounded administrative adoption. Physical production still requires industry/freight.
class CITYSIMVR_API FCityCommercialAi
{
  public:
	static constexpr int64 MonthlyFee = 5;
	static constexpr int32 ShopsPerCenter = 2;
	static int64 BillingPeriod(int64 SettlementDay);
	static FString Status(const FCityPopulationSnapshot& Population, const FCityServiceSnapshot& Services,
	                      uint64 ShopId);
	static TArray<uint64> Eligible(const FCityPopulationSnapshot& Population,
	                               const FCityServiceSnapshot& Services);
	static void Apply(const FCityPopulationSnapshot* Population, FCityServiceSnapshot& Services);
	static void Settle(FCityPopulationSnapshot& Population, const FCityServiceSnapshot* Services,
	                   int64 Month);
	static FString Review(const FCityPopulationSnapshot& Population, const FCityServiceSnapshot& Services);
};
