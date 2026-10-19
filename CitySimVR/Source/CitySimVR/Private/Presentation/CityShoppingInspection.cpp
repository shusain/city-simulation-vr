#include "Presentation/CityConditionIndicators.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityTrafficSimulation.h"

FString FCityConditionIndicators::ShoppingBlocker(const UCitySimulationSubsystem& Sim, uint64 Id)
{
	const auto& People = Sim.GetPopulationSnapshot();
	const auto* Home = People.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == Id; });
	if (!Home || (Home->bUsesStoredSupplies ? Home->SupplyUnits > 0 : Home->CompletedPurchases >= Home->ConsumptionOrders))
		return {};
	const auto& City = Sim.GetPlacementModel();
	const auto& Traffic = Sim.GetTrafficSnapshot();
	const FCityPurchaseReservation* Order = nullptr;
	TMap<uint64, int32> Held;
	int64 Funds = 0;
	for (const auto& C : People.Citizens) if (C.HouseholdId == Home->Id) Funds += C.AvailableMoney;
	for (const auto& O : People.PurchaseReservations)
	{
		Held.FindOrAdd(O.ShopId) += O.Quantity;
		if (O.HouseholdId == Home->Id)
		{
			Funds -= O.Price;
			if (!O.ComfortTier && (!Order || O.BuyerId < Order->BuyerId)) Order = &O;
		}
	}
	if (Order)
	{
		const auto* Buyer = People.Citizens.FindByPredicate([&](const auto& C) { return C.Id == Order->BuyerId; });
		if (!Buyer) return TEXT("Buyer unavailable");
		if (Buyer->Schedule == ECityCitizenSchedule::Work) return TEXT("Buyer at work");
		if (Buyer->Schedule == ECityCitizenSchedule::ToWork) return TEXT("Buyer commuting");
		if (Buyer->Schedule == ECityCitizenSchedule::ToHome) return TEXT("Buyer returning home");
		const auto Matches = [&](const auto& V) { return !V.bTransitTrip && V.SourceId == Buyer->Id && V.DestinationEntityId == Order->ShopId; };
		if (Traffic.WaitingDepartures.ContainsByPredicate(Matches)) return TEXT("Departure blocked by traffic");
		if (Traffic.Vehicles.ContainsByPredicate(Matches)) return TEXT("Shopping travel");
		FCityCellCoord Start, End;
		TMap<FCityCellCoord, int32> Distances;
		const uint64 Origin = Buyer->CurrentLocationEntityId ? Buyer->CurrentLocationEntityId : Id;
		if (!FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Origin, Start) ||
		    !FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Order->ShopId, End)) return TEXT("No shopping route");
		Sim.GetRoadNetworkSnapshot().FindWalkingDistances(Start, Distances);
		if (!Distances.Contains(End)) return TEXT("No shopping route");
		return TEXT("Departure pending");
	}
	if (Funds < FMath::Max<int64>(0, Sim.GetEconomyTuning().HouseholdIncomePerGoodsPurchase)) return TEXT("Insufficient available cash");
	bool Stock = false, Slots = false, Eligible = false, Reachable = false;
	FCityCellCoord Start;
	TMap<FCityCellCoord, int32> Distances;
	if (FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), Id, Start)) Sim.GetRoadNetworkSnapshot().FindWalkingDistances(Start, Distances);
	for (const auto& B : People.Businesses)
	{
		if (B.bIndustrial) continue;
		Stock |= B.Inventory > Held.FindRef(B.EntityId);
		Slots |= B.PendingOrders - B.SalesThisMonth > Held.FindRef(B.EntityId);
		if (!FCityPopulationSimulation::CanReserveShop(B, Held.FindRef(B.EntityId))) continue;
		Eligible = true;
		FCityCellCoord End;
		Reachable |= FCityTrafficSimulation::FindPortal(City, Sim.GetBuildingCatalog(), B.EntityId, End) && Distances.Contains(End);
	}
	if (!Stock) return TEXT("No available shop stock");
	if (!Slots || !Eligible) return TEXT("No available shop capacity");
	return Reachable ? TEXT("Allocation pending") : TEXT("No shopping route");
}
