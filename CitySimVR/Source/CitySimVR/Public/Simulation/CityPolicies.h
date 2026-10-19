#pragma once
#include "CoreMinimal.h"
#include "CityPolicies.generated.h"
class FCityPlacementModel;
struct FCityPopulationSnapshot;
struct FCityServiceSnapshot;
struct FCityEconomyTuning;

UENUM(BlueprintType)
enum class ECityPolicy : uint8
{
	ReserveAssistance,
	SchoolBursaries,
	Count UMETA(Hidden)
};
struct CITYSIMVR_API FCityPolicyQuote
{
	int64 Eligible = 0, Grants = 0;
	int64 Administration() const
	{
		return Eligible > 0 ? 25 : 0;
	}
	int64 Total() const
	{
		return Grants + Administration();
	}
};
struct CITYSIMVR_API FCityPolicyRecord
{
	bool bEnabled = false, bLastEnabled = false;
	int64 Eligible = 0, Recipients = 0, GrantsDue = 0, GrantsPaid = 0, AdministrationPaid = 0;
	int64 Due() const
	{
		return bLastEnabled && Eligible > 0 ? GrantsDue + 25 : 0;
	}
};
// Optional programs, not liabilities: all recipients funded or no charge/effect.
struct CITYSIMVR_API FCityPolicyState
{
	int64 LastSettledMonth = 0;
	FCityPolicyRecord Programs[2];
	static bool IsValid(ECityPolicy Policy)
	{
		return static_cast<uint8>(Policy) < 2;
	}
	static const TCHAR* Name(ECityPolicy Policy);
	static FCityPolicyQuote Quote(ECityPolicy Policy, const FCityPopulationSnapshot& Population,
	                              const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	                              bool bEducationAvailable = true);
	int64 Forecast(const FCityPopulationSnapshot& Population, const FCityServiceSnapshot& Services,
	               const FCityEconomyTuning& Tuning, bool bEducationAvailable = true) const;
	bool SetEnabled(ECityPolicy Policy, bool Enabled);
	bool SettleMonth(int64 Month, FCityPlacementModel& City, FCityPopulationSnapshot& Population,
	                 const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	                 bool bEducationAvailable);
	int64 Paid() const;
	int64 Due() const;
	int64 Unfunded() const;
	bool Validate(int64 SettledMonth, FString& Error) const;
	FString Summary(int64 Cash, int64 ForecastCost) const;
	FString Review(ECityPolicy Policy, bool Enable, const FCityPolicyQuote& Quote, int64 Cash,
	               int64 BudgetAfterChange) const;
};
