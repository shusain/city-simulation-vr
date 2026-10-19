#include "Simulation/CityPopulationSimulation.h"

int32 FCityPopulationSimulation::StoredEssentialShortage(const FCityPopulationSnapshot& State, int64 Day)
{
	// -1 retains aggregate-only fixtures; delivered pantry stock, rather than
	// today's visits, provides essential coverage in actual cities.
	if (!State.Households.ContainsByPredicate([](const auto& H) { return H.bUsesStoredSupplies; })) return -1;
	if (Day < 0) Day = State.LastSettledMonthId;
	int64 Missing = 0;
	for (const auto& H : State.Households)
	{
		const int64 Available = H.bUsesStoredSupplies ? FMath::Max<int64>(0, H.SupplyUnits - FMath::Max<int64>(0, Day - H.SupplyDay) * H.ConsumptionOrders)
		                                                  : H.PurchasesThisMonth;
		Missing += FMath::Max<int64>(0, H.ConsumptionOrders - Available);
	}
	for (const auto& C : State.Citizens) if (C.IsHomeless()) Missing += C.ConsumptionNeed;
	return static_cast<int32>(FMath::Min<int64>(MAX_int32, Missing));
}

void FCityPopulationSimulation::RefreshHouseholdSupplies(const int64 Day)
{
	TMap<uint64, int32> Need;
	for (const auto& C : Snapshot.Citizens)
		Need.FindOrAdd(C.HouseholdId) += C.ConsumptionNeed;
	for (auto& H : Snapshot.Households)
	{
		H.ConsumptionOrders = Need.FindRef(H.Id);
		if (H.bUsesStoredSupplies && Day > H.SupplyDay)
		{
			// Daily consumption does not depend on trip frequency or rendering.
			const int64 Used = (Day - H.SupplyDay) * H.ConsumptionOrders;
			H.SupplyUnits = static_cast<int32>(FMath::Max<int64>(0, H.SupplyUnits - Used));
			H.SupplyDay = Day;
		}
	}
}

void FCityPopulationSimulation::EnableHouseholdSupplies(const int64 Day)
{
	RefreshHouseholdSupplies(Day);
	for (auto& H : Snapshot.Households)
		if (!H.bUsesStoredSupplies)
		{
			H.bUsesStoredSupplies = true;
			H.SupplyDay = Day;
			// New arrivals bring a small starter pantry. This is also the one-time,
			// saved transition for existing current-format cities, never a purchase.
			H.SupplyUnits = H.ConsumptionOrders * 3;
		}
}

int32 FCityPopulationSimulation::PreferredComfortTier(const FCityHouseholdRecord& Home,
    const int64 Funds, const int64 UnitPrice, const TArray<FCityCitizenRecord>& Citizens)
{
	if (!Home.bUsesStoredSupplies || Home.ConsumptionOrders <= 0 ||
	    Home.SupplyUnits < Home.ConsumptionOrders * 3)
		return 0;
	const int64 WeeklyEssentials = static_cast<int64>(Home.ConsumptionOrders) * 7 * FMath::Max<int64>(1, UnitPrice);
	if (Funds < WeeklyEssentials) return 0; // Keep a funded week of essentials before comfort.
	ECityEducationLevel Education = ECityEducationLevel::None;
	for (const auto& C : Citizens)
		if (C.HouseholdId == Home.Id && C.Education > Education) Education = C.Education;
	if (Funds >= WeeklyEssentials * 4 ||
	    (Education >= ECityEducationLevel::Higher && Funds >= WeeklyEssentials * 2)) return 2;
	return Education >= ECityEducationLevel::Secondary || Funds >= WeeklyEssentials * 2 ? 1 : 0;
}
