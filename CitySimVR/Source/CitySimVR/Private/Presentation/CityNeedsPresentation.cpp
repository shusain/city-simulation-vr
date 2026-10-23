#include "Presentation/CityNeedsPresentation.h"

#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
	FString RepeatCharacter(const TCHAR Character, const int32 Count)
	{
		return FString::ChrN(FMath::Max(0, Count), Character);
	}

	FString ConstraintLabel(const ECityDemandConstraint Constraint, const int64 RequiredFunds)
	{
		switch (Constraint)
		{
		case ECityDemandConstraint::None:
			return TEXT("READY");
		case ECityDemandConstraint::LowDemand:
			return TEXT("LOW DEMAND");
		case ECityDemandConstraint::OperatingShortfall:
			return TEXT("UNPAID BILLS");
		case ECityDemandConstraint::NoZonedFootprint:
			return TEXT("NO ZONE");
		case ECityDemandConstraint::NoRoad:
			return TEXT("NO ROAD");
		case ECityDemandConstraint::NoService:
			return TEXT("NO WATER");
		case ECityDemandConstraint::ServiceCapacity:
			return TEXT("WATER FULL");
		case ECityDemandConstraint::NoPower:
			return TEXT("NO POWER");
		case ECityDemandConstraint::PowerCapacity:
			return TEXT("POWER FULL");
		case ECityDemandConstraint::LowDesirability:
			return TEXT("LOW APPEAL");
		case ECityDemandConstraint::PollutionPressure:
			return TEXT("POLLUTION");
		case ECityDemandConstraint::InvalidTerrain:
			return TEXT("BAD TERRAIN");
		case ECityDemandConstraint::InsufficientFunds:
			return FString::Printf(TEXT("NEED $%lld"), RequiredFunds);
		case ECityDemandConstraint::MissingDefinition:
			return TEXT("CONTENT");
		default:
			return TEXT("BLOCKED");
		}
	}

	int64 NonOverflowingNet(const FCityEconomySnapshot& Economy)
	{
		return Economy.LastIncome >= Economy.LastUpkeep ? Economy.LastIncome - Economy.LastUpkeep
		                                                : -(Economy.LastUpkeep - Economy.LastIncome);
	}
} // namespace

FString FCityNeedsPresentation::BuildRatioBar(const int64 Value, const int64 Capacity, const int32 Width,
                                              const TCHAR* ZeroLabel)
{
	const int32 SafeWidth = FMath::Max(1, Width);
	if (Capacity <= 0)
	{
		FString Label = ZeroLabel ? FString(ZeroLabel) : FString(TEXT("ZERO"));
		Label = Label.Left(SafeWidth);
		return FString::Printf(TEXT("[%-*s]"), SafeWidth, *Label);
	}
	const int64 ClampedValue = FMath::Clamp<int64>(Value, 0, Capacity);
	const int32 Filled =
	    FMath::Clamp(static_cast<int32>((static_cast<long double>(ClampedValue) * SafeWidth) / Capacity), 0, SafeWidth);
	return FString::Printf(TEXT("[%s%s]"), *RepeatCharacter(TEXT('#'), Filled),
	                       *RepeatCharacter(TEXT('.'), SafeWidth - Filled));
}

FString FCityNeedsPresentation::BuildSignedBar(const int64 Value, const int64 AbsoluteBound, const int32 HalfWidth)
{
	const int32 SafeHalfWidth = FMath::Max(1, HalfWidth);
	const int64 SafeBound = FMath::Max<int64>(1, AbsoluteBound);
	const int64 Magnitude = Value >= 0 ? Value : -(Value + 1) + 1;
	const int32 Filled =
	    FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(
	                     (static_cast<long double>(FMath::Min(Magnitude, SafeBound)) * SafeHalfWidth) / SafeBound)),
	                 0, SafeHalfWidth);
	const FString Negative =
	    Value < 0 ? RepeatCharacter(TEXT('.'), SafeHalfWidth - Filled) + RepeatCharacter(TEXT('<'), Filled)
	              : RepeatCharacter(TEXT('.'), SafeHalfWidth);
	const FString Positive =
	    Value > 0 ? RepeatCharacter(TEXT('>'), Filled) + RepeatCharacter(TEXT('.'), SafeHalfWidth - Filled)
	              : RepeatCharacter(TEXT('.'), SafeHalfWidth);
	return FString::Printf(TEXT("[%s|%s]"), *Negative, *Positive);
}

double FCityNeedsPresentation::NormalizeSignedValue(const int64 Value, const int64 AbsoluteBound)
{
	const int64 SafeBound = FMath::Max<int64>(1, AbsoluteBound);
	return FMath::Clamp(static_cast<double>(Value) / static_cast<double>(SafeBound), -1.0, 1.0);
}

double FCityNeedsPresentation::NormalizeRatioValue(const int64 Value, const int64 Capacity)
{
	if (Capacity <= 0)
	{
		return 0.0;
	}
	return FMath::Clamp(static_cast<double>(Value) / static_cast<double>(Capacity), 0.0, 1.0);
}

FString FCityNeedsPresentation::BuildPanel(const FCityNeedsSnapshot& Needs, const FCityEconomySnapshot& Economy,
                                           const int64, const int64, const FCityBudgetProjection* Budget)
{
	const int64 Net = NonOverflowingNet(Economy);
	const int64 NetMagnitude = Net >= 0 ? Net : -(Net + 1) + 1;
	FString Shortages;
	auto AddShortage = [&Shortages](const TCHAR* Label, const int32 Amount)
	{
		if (Amount <= 0)
		{
			return;
		}
		if (!Shortages.IsEmpty())
		{
			Shortages += TEXT(" ");
		}
		Shortages += FString::Printf(TEXT("%s:%d"), Label, Amount);
	};
	AddShortage(TEXT("HOME"), Needs.HousingShortage);
	AddShortage(TEXT("JOB"), Needs.JobShortage);
	AddShortage(TEXT("PWR"), Needs.PowerShortage);
	AddShortage(TEXT("WTR"), Needs.UnservedServiceDemand);
	AddShortage(TEXT("GOODS"), Needs.UnmetGoodsDemand);
	if (Shortages.IsEmpty())
	{
		Shortages = TEXT("NONE");
	}

	FString Panel = FString::Printf(
	    TEXT("CITY NEEDS\n") TEXT("GROW HOME:%s SHOP:%s IND:%s\n") TEXT("PRIVATE DEVELOPMENT  SHORT %s\n")
	        TEXT("LAST DAY: income $%lld / bills $%lld\nNET %s$%lld"),
	    *ConstraintLabel(Needs.ResidentialConstraint, Needs.ResidentialGrowthCost),
	    *ConstraintLabel(Needs.CommercialConstraint, Needs.CommercialGrowthCost),
	    *ConstraintLabel(Needs.IndustrialConstraint, Needs.IndustrialGrowthCost), *Shortages, Economy.LastIncome,
	    Economy.LastUpkeep, Net >= 0 ? TEXT("+") : TEXT("-"), NetMagnitude);
	if (Budget)
	{
		const int64 ForecastNet = Budget->Net();
		Panel += FString::Printf(TEXT(" | Forecast %s$%lld/day"), ForecastNet >= 0 ? TEXT("+") : TEXT("-"),
		                         FMath::Abs(ForecastNet));
		Panel += FString::Printf(TEXT("\nTax forecast: R $%lld | C $%lld | I $%lld"), Budget->ResidentialTax,
		                         Budget->CommercialTax, Budget->IndustrialTax);
	}
	if (Economy.bGrowthBlockedByShortfall)
	{
		Panel += FString::Printf(TEXT("\nUnpaid last day $%lld: growth paused"), Economy.UpkeepShortfall);
		if (Budget)
			Panel += FString::Printf(
			    TEXT("\nCosts/day: roads $%lld | utilities $%lld\nBuildings $%lld | public wages $%lld"),
			    Budget->RoadUpkeep, Budget->UtilityUpkeep, Budget->BuildingUpkeep, Budget->PublicPayroll);
		Panel += TEXT(
		    "\nCity > TAXES: raise income, then Apply\nOr remove unused roads/utilities\nRun time until a day is fully paid");
	}
	else if (Needs.ResidentialConstraint == ECityDemandConstraint::InsufficientFunds ||
	         Needs.CommercialConstraint == ECityDemandConstraint::InsufficientFunds ||
	         Needs.IndustrialConstraint == ECityDemandConstraint::InsufficientFunds)
	{
		Panel += TEXT("\nSave cash for construction; limit spending");
	}
	if (Needs.bHasWorkforceEducation)
	{
		const auto& Staff = Needs.WorkforceEducation;
		Panel += FString::Printf(TEXT("\nSTAFF: %d educated | %d uneducated\nBasic %d | Secondary %d | Higher %d"),
		                         Staff.Educated(), Staff.Uneducated, Staff.Basic, Staff.Secondary, Staff.Higher);
	}
	if (Needs.bHasDensityDemand)
		Panel += BuildDensitySummary(Needs);
	return Panel;
}

FString FCityNeedsPresentation::BuildDensitySummary(const FCityNeedsSnapshot& Needs)
{
	FString Text;
	if (Needs.bHasDensityDemand)
	{
		Text += TEXT("\nDENSITY PREFERENCES: Low / Medium / High");
		for (const auto Type : {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
		                        ECityDevelopmentDemandType::Industrial})
		{
			const auto& Market = Needs.DensityFor(Type);
			Text += FString::Printf(TEXT("\n%s: %+d / %+d / %+d"),
			                        Type == ECityDevelopmentDemandType::Residential  ? TEXT("Homes")
			                        : Type == ECityDevelopmentDemandType::Commercial ? TEXT("Shops")
			                                                                         : TEXT("Industry"),
			                        Market.Balance(ECityDensityLevel::Low), Market.Balance(ECityDensityLevel::Medium),
			                        Market.Balance(ECityDensityLevel::High));
		}
		Text += FString::Printf(
		    TEXT(
		        "\n+ preference gap; - spare (places/jobs)\nNew low capacity R:%d C:%d I:%d\nMedium/High upgrade only if supply needed"),
		    Needs.ResidentialDensity.NewLowDemand(), Needs.CommercialDensity.NewLowDemand(),
		    Needs.IndustrialDensity.NewLowDemand());
		Text += FString::Printf(TEXT("\nWorking %d%% | skilled %d%%"), Needs.DensityEmploymentPercent,
		                        Needs.DensitySkilledPercent);
		if (Needs.bStarterHousingDemand)
			Text += TEXT("\nStarter households can arrive before jobs");
	}
	return Text;
}

FString FCityNeedsPresentation::BuildGuidance(const FCityNeedsSnapshot& Needs, const FCityEconomySnapshot& Economy)
{
	FString Text = FString::Printf(TEXT("GROWTH LIMITS\nHomes: %s\nShops: %s\nIndustry: %s"),
	                               *ConstraintLabel(Needs.ResidentialConstraint, Needs.ResidentialGrowthCost),
	                               *ConstraintLabel(Needs.CommercialConstraint, Needs.CommercialGrowthCost),
	                               *ConstraintLabel(Needs.IndustrialConstraint, Needs.IndustrialGrowthCost));
	Text += FString::Printf(TEXT("\nGrowth checks every %d game days\nDeliveries and utilities still required"),
	                        Needs.GrowthCheckIntervalDays);
	Text += BuildDensitySummary(Needs);
	Text +=
	    FString::Printf(TEXT("\nHousing offline/pending %d | jobs %d\nExisting and pending buildings reserve demand"),
	                    FMath::Max(0, Needs.BuiltHousingCapacity - Needs.HousingCapacity),
	                    FMath::Max(0, Needs.BuiltJobCapacity - Needs.JobCapacity));
	if (Economy.bGrowthBlockedByShortfall)
		Text += FString::Printf(TEXT("\nUnpaid bills $%lld: growth paused\nCity > Taxes: raise income or cut costs"),
		                        Economy.UpkeepShortfall);
	else if (Needs.ResidentialConstraint == ECityDemandConstraint::InsufficientFunds ||
	         Needs.CommercialConstraint == ECityDemandConstraint::InsufficientFunds ||
	         Needs.IndustrialConstraint == ECityDemandConstraint::InsufficientFunds)
		Text += TEXT("\nSave cash for construction; limit spending");
	return Text;
}
