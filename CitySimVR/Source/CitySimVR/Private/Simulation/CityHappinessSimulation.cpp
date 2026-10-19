#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityWasteSimulation.h"

namespace
{
	using FAssessment = FCityHappinessAssessment;
}

bool FCityHappinessEvaluator::ValidateCitizen(const FCityCitizenRecord& C, FString& Error)
{
	if (C.Happiness < 0 || C.Happiness > 100 || C.HappinessTarget < 0 || C.HappinessTarget > 100 ||
	    C.LastRelocationMonth < 0 || C.LastRelocationMonth > C.LastHappinessMonth ||
	    static_cast<uint8>(C.HousingStatus) > static_cast<uint8>(ECityHousingStatus::Recovering) ||
	    C.HappinessConcerns < 0 || (C.HappinessConcerns & ~ValidConcerns) || C.LastHappinessMonth < 0 ||
	    C.DiscontentMonths < 0 || C.DiscontentMonths > C.LastHappinessMonth ||
	    (C.DiscontentMonths > 0 && C.Happiness >= DiscontentThreshold))
	{
		Error = TEXT("Invalid citizen happiness or discontent history");
		return false;
	}
	return true;
}

FCityHappinessSummary FCityHappinessEvaluator::Summarize(const FCityPopulationSnapshot& P, uint64 HomeId)
{
	FCityHappinessSummary S;
	int64 Total = 0, Targets = 0;
	for (const auto& C : P.Citizens)
	{
		if (HomeId && C.HomeEntityId != HomeId)
			continue;
		++S.Residents;
		Total += C.Happiness;
		Targets += C.HappinessTarget;
		S.Discontented += C.Happiness < DiscontentThreshold;
		S.Sustained += C.DiscontentMonths >= SustainedMonths;
		S.Concerns |= C.HappinessConcerns;
	}
	if (S.Residents)
	{
		S.Average = static_cast<int32>(Total / S.Residents);
		S.Target = static_cast<int32>(Targets / S.Residents);
	}
	return S;
}

FString FCityHappinessEvaluator::DescribeConcerns(int32 Concerns)
{
	const TCHAR* Labels[] = {TEXT("no housing"),  TEXT("unreliable services"), TEXT("unmet purchases"),
	                         TEXT("low savings"), TEXT("pollution"),           TEXT("crime risk"),
	                         TEXT("illness"),     TEXT("uncollected waste"),   TEXT("tax burden")};
	TArray<FString> Reasons;
	for (int32 I = 0; I < UE_ARRAY_COUNT(Labels); ++I)
		if (Concerns & (1 << I))
			Reasons.Add(Labels[I]);
	return Reasons.IsEmpty() ? TEXT("no concerns recorded") : FString::Join(Reasons, TEXT(", "));
}

FCityHappinessAssessment FCityHappinessEvaluator::AssessHome(
    const FCityHouseholdRecord& H, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
    const FCityEnvironmentSnapshot& Environment, const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
    const FCityWasteSnapshot& Waste)
{
	FAssessment A;
	const auto* Home = City.GetEntities().Find(H.HomeEntityId);
	if (!Home)
		return A;
	A.Penalize(Services.GetFailure(H.HomeEntityId) != ECityOperationFailure::None ? 25
	           : Economy.HasOperatingUpkeepShortfall()                            ? 10
	                                                                              : 0,
	           ECityHappinessConcern::Services);
	// Completed visits, not projected supply or reserved purchases, meet needs.
	const int32 Orders = FMath::Max(0, H.ConsumptionOrders);
	const int32 Unmet = H.bUsesStoredSupplies ? (H.SupplyUnits > 0 ? 0 : Orders)
	                                         : Orders - FMath::Clamp(H.CompletedPurchases, 0, Orders);
	A.Penalize(Orders > 0 ? static_cast<int32>(static_cast<int64>(Unmet) * 30 / Orders) : 0,
	           ECityHappinessConcern::Purchases);
	if (H.bUsesStoredSupplies && H.SupplyUnits > 0 && H.ComfortTier > 0 &&
	    Economy.LastBilledMonthId - H.LastComfortDay < 7)
		A.Target += H.ComfortTier == 2 ? 8 : 5;
	const int64 Buffer = static_cast<int64>(Orders) * FMath::Max(0, Tuning.HouseholdIncomePerGoodsPurchase);
	A.Penalize(Buffer > 0 && H.Balance < Buffer ? 10 : 0, ECityHappinessConcern::Reserves);
	const auto Appeal = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    City, Services, Environment, Home->Anchor, Home->FootprintSize, Tuning);
	A.Target += FMath::Clamp(Appeal.ParkBonus / 3, 0, 10);
	A.Penalize(FMath::Clamp(Appeal.PollutionPenalty, 0, 20), ECityHappinessConcern::Pollution);
	A.Penalize(FMath::Clamp(Appeal.CrimePenalty, 0, 15), ECityHappinessConcern::Safety);
	A.Penalize(FMath::Clamp(Waste.Pressure(H.HomeEntityId, Tuning.FixedStepsPerMonth), 0, 10),
	           ECityHappinessConcern::Waste);
	A.Penalize(FMath::Clamp((Economy.ResidentialTaxBasisPoints - 1000) / 100, 0, 15), ECityHappinessConcern::Taxes);
	return A;
}

void FCityPopulationSimulation::AdvanceHappiness(int64 Month, const FCityPlacementModel& City,
                                                 const FCityServiceSnapshot& Services,
                                                 const FCityEnvironmentSnapshot& Environment,
                                                 const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
                                                 const FCityHealthSnapshot& Health, const FCityWasteSnapshot& Waste)
{
	// Only the completed live billing phase calls this. No forecast/render/load path.
	if (Month <= 0)
		return;
	TMap<uint64, FAssessment> Homes;
	for (const auto& H : Snapshot.Households)
	{
		if (!City.GetEntities().Contains(H.HomeEntityId))
			continue;
		const auto A = FCityHappinessEvaluator::AssessHome(H, City, Services, Environment, Economy, Tuning, Waste);
		Homes.Add(H.HomeEntityId, A);
	}
	TMap<uint64, const FCityHealthPatient*> Patients;
	for (const auto& P : Health.Patients)
		Patients.Add(P.CitizenId, &P);
	for (auto& C : Snapshot.Citizens)
	{
		if (Month <= C.LastHappinessMonth)
			continue;
		FAssessment A;
		if (const auto* Home = Homes.Find(C.HomeEntityId))
			A = *Home;
		else if (!C.IsDetained())
		{
			A.Penalize(35, ECityHappinessConcern::Housing);
			A.Penalize(30, ECityHappinessConcern::Purchases);
			A.Penalize(C.AvailableMoney <= 0 ? 10 : 0, ECityHappinessConcern::Reserves);
		}
		if (Services.EducationCitizenIds.Contains(C.Id) || Services.HigherEducationCitizenIds.Contains(C.Id))
			A.Target += 5;
		if (const auto* const* Patient = Patients.Find(C.Id))
		{
			const bool Treated = (*Patient)->ReceivingCare();
			A.Penalize(((*Patient)->Illness == ECityIllness::Severe ? 20 : 10) - (Treated ? 5 : 0),
			           ECityHappinessConcern::Health);
		}
		C.HappinessTarget = FMath::Clamp(A.Target, 0, 100);
		C.HappinessConcerns = A.Concerns;
		C.Happiness += FMath::Clamp(C.HappinessTarget - C.Happiness, -FCityHappinessEvaluator::MaximumMonthlyChange,
		                            FCityHappinessEvaluator::MaximumMonthlyChange);
		// A direct clock jump supplies one observation, not fabricated missing months.
		C.DiscontentMonths =
		    C.Happiness < FCityHappinessEvaluator::DiscontentThreshold
		        ? (Month == C.LastHappinessMonth + 1
		               ? static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(C.DiscontentMonths) + 1))
		               : 1)
		        : 0;
		C.LastHappinessMonth = Month;
	}
}
