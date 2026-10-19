#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"

void FCityPopulationSimulation::RefreshHouseholdBalances()
{
	TMap<uint64, int64> Balances;
	for (const auto& Citizen : Snapshot.Citizens)
		Balances.FindOrAdd(Citizen.HouseholdId) += Citizen.AvailableMoney;
	for (auto& Household : Snapshot.Households)
		Household.Balance = Balances.FindRef(Household.Id);
}

bool FCityPopulationSimulation::ValidatePurchaseReservations(const FCityPopulationSnapshot& State, FString& OutError)
{
	if (State.SalesThisMonth < 0 || State.LastMonthSales < 0 || State.SpendingThisMonth < 0 ||
	    State.LastMonthSpending < 0 || State.TotalArrivalPurchases < State.SalesThisMonth ||
	    State.PurchaseReservations.Num() > State.Citizens.Num() ||
	    (!State.bUsesPurchaseReservations && !State.PurchaseReservations.IsEmpty()))
	{
		OutError = TEXT("Invalid purchase ledger");
		return false;
	}
	TMap<uint64, const FCityCitizenRecord*> Citizens;
	TMap<uint64, const FCityBusinessLedgerRecord*> Shops;
	for (const auto& C : State.Citizens)
		Citizens.Add(C.Id, &C);
	for (const auto& B : State.Businesses)
	{
		if (B.SalesThisMonth < 0 || B.RetailTier < 0 || B.RetailTier > 2 || B.OpeningGraceUntilStep < -1)
		{
			OutError = TEXT("Invalid monthly sales");
			return false;
		}
		Shops.Add(B.EntityId, &B);
	}
	for (const auto& H : State.Households)
		if (H.PurchasesThisMonth < 0 || H.SupplyUnits < 0 || H.SupplyDay < -1 ||
		    (H.bUsesStoredSupplies && H.SupplyDay < 0) || H.LastEssentialPurchaseDay < -1 ||
		    H.LastComfortDay < -7 || H.ComfortTier < 0 || H.ComfortTier > 2)
		{
			OutError = TEXT("Invalid monthly purchases");
			return false;
		}
	TSet<uint64> Buyers;
	TMap<uint64, int64> LockedMoney;
	TMap<uint64, int32> LockedStock;
	for (const auto& Order : State.PurchaseReservations)
	{
		const auto* const* Buyer = Citizens.Find(Order.BuyerId);
		const auto* const* Shop = Shops.Find(Order.ShopId);
		if (!Buyer || !Shop || (*Shop)->bIndustrial || Order.Price < 0 || Order.Quantity < 1 || Order.Quantity > 7 || Order.ComfortTier < 0 || Order.ComfortTier > 2 ||
		    (Order.ComfortTier && Order.Quantity != 1) || Buyers.Contains(Order.BuyerId) ||
		    (*Buyer)->HouseholdId != Order.HouseholdId || (*Buyer)->PendingShopEntityId != Order.ShopId ||
		    (LockedStock.FindOrAdd(Order.ShopId) += Order.Quantity) > (*Shop)->Inventory)
		{
			OutError = TEXT("Invalid purchase reservation");
			return false;
		}
		Buyers.Add(Order.BuyerId);
		TSet<uint64> Contributors;
		int64 Total = 0;
		for (const auto& Contribution : Order.Contributions)
		{
			const auto* const* Member = Citizens.Find(Contribution.CitizenId);
			if (!Member || (*Member)->HouseholdId != Order.HouseholdId || Contribution.Amount <= 0 ||
			    Contribution.Amount > Order.Price - Total || Contributors.Contains(Contribution.CitizenId) ||
			    (LockedMoney.FindRef(Contribution.CitizenId) > (*Member)->AvailableMoney ||
			     Contribution.Amount > (*Member)->AvailableMoney - LockedMoney.FindRef(Contribution.CitizenId)))
			{
				OutError = TEXT("Invalid purchase funding");
				return false;
			}
			Contributors.Add(Contribution.CitizenId);
			Total += Contribution.Amount;
			LockedMoney.FindOrAdd(Contribution.CitizenId) += Contribution.Amount;
		}
		if (Total != Order.Price)
		{
			OutError = TEXT("Purchase funding does not match price");
			return false;
		}
	}
	return true;
}

void FCityPopulationSimulation::PrunePurchaseReservations(const FCityPlacementModel& City)
{
	TMap<uint64, FCityCitizenRecord*> Citizens;
	TMap<uint64, const FCityBusinessLedgerRecord*> Businesses;
	for (auto& Citizen : Snapshot.Citizens)
		Citizens.Add(Citizen.Id, &Citizen);
	for (const auto& Business : Snapshot.Businesses)
		Businesses.Add(Business.EntityId, &Business);
	// No money or stock moved at reservation, so cancellation only releases locks.
	Snapshot.PurchaseReservations.RemoveAll(
	    [&](const FCityPurchaseReservation& Order)
	    {
		    const auto* Buyer = Citizens.FindRef(Order.BuyerId);
		    const auto* Shop = Businesses.FindRef(Order.ShopId);
		    bool bValid = Buyer && Shop && !Shop->bIndustrial && Shop->PendingOrders > 0 &&
		                  Buyer->HouseholdId == Order.HouseholdId && Buyer->PendingShopEntityId == Order.ShopId &&
		                  City.GetEntities().Contains(Order.ShopId) && City.GetEntities().Contains(Buyer->HomeEntityId);
		    for (const auto& Contribution : Order.Contributions)
		    {
			    const auto* Member = Citizens.FindRef(Contribution.CitizenId);
			    bValid &= Member && Member->HouseholdId == Order.HouseholdId;
		    }
		    if (!bValid)
			    if (auto* Citizen = Citizens.FindRef(Order.BuyerId);
			        Citizen && Citizen->PendingShopEntityId == Order.ShopId)
				    Citizen->PendingShopEntityId = 0;
		    return !bValid;
	    });
}

void FCityPopulationSimulation::RetryPurchaseReservations(const FCityPlacementModel& City,
    const UCityBuildingCatalog* Catalog, const FCityRoadNetworkSnapshot& Network, const int64 Step,
    const FCityEconomyTuning& Tuning, const FCityDevelopmentSnapshot* Development)
{
    if (!Snapshot.bUsesPurchaseReservations) return;
    if (Development)
        for (auto& B : Snapshot.Businesses)
            if (const auto* State = Development->Find(B.EntityId)) B.RetailTier = static_cast<int32>(State->DensityLevel);
    PrunePurchaseReservations(City);
    ReservePurchases(Tuning, &City, Catalog, &Network, Step);
}

void FCityPopulationSimulation::ReservePurchases(const FCityEconomyTuning& Tuning,
    const FCityPlacementModel* City, const UCityBuildingCatalog* Catalog,
    const FCityRoadNetworkSnapshot* Network, const int64 Step)
{
    const int64 Day = Network ? FCityCalendar::DayIndex(Step, Tuning.FixedStepsPerMonth) : Snapshot.LastSettledMonthId + 1;
    const int64 UnitPrice = FMath::Max<int64>(0, Tuning.HouseholdIncomePerGoodsPurchase);
    TMap<uint64, int64> Available;
    TMap<uint64, TArray<const FCityCitizenRecord*>> Members;
    TMap<uint64, FCityHouseholdRecord*> Homes;
    TMap<uint64, int32> HeldStock, HeldEssential, HeldComfort;
    for (const auto& C : Snapshot.Citizens)
    {
        Available.Add(C.Id, FMath::Max<int64>(0, C.AvailableMoney));
        Members.FindOrAdd(C.HouseholdId).Add(&C);
    }
    for (auto& H : Snapshot.Households)
    {
        H.ConsumptionOrders = 0;
        if (const auto* Group = Members.Find(H.Id))
            for (const auto* C : *Group) H.ConsumptionOrders += C->ConsumptionNeed;
        Homes.Add(H.Id, &H);
    }
    const auto EssentialRoom = [](const FCityHouseholdRecord& H) {
        return FMath::Max(0, H.bUsesStoredSupplies ? H.ConsumptionOrders * 8 - H.SupplyUnits
                                                 : H.ConsumptionOrders - H.PurchasesThisMonth);
    };
    // Preserve stable older finite holds while enforcing the current household
    // quota. A changed household size cannot create another funded basket.
    Snapshot.PurchaseReservations.RemoveAll([&](const auto& O) {
        const auto* H = Homes.FindRef(O.HouseholdId);
        const bool Keep = H && (O.ComfortTier ? H->bUsesStoredSupplies && H->SupplyUnits > H->ConsumptionOrders && !HeldComfort.FindRef(H->Id) && Day >= H->LastComfortDay + 7
                                             : O.Quantity <= EssentialRoom(*H) - HeldEssential.FindRef(H->Id));
        if (Keep)
        {
            (O.ComfortTier ? HeldComfort : HeldEssential).FindOrAdd(H->Id) += O.Quantity;
            HeldStock.FindOrAdd(O.ShopId) += O.Quantity;
            for (const auto& C : O.Contributions) Available.FindOrAdd(C.CitizenId) -= C.Amount;
        }
        else if (auto* Buyer = Snapshot.Citizens.FindByPredicate([&](const auto& C) { return C.Id == O.BuyerId; }))
            Buyer->PendingShopEntityId = 0;
        return !Keep;
    });
    TMap<uint64, bool> Restocking;
    for (const auto& H : Snapshot.Households)
        Restocking.Add(H.Id, !H.bUsesStoredSupplies || H.SupplyUnits <= H.ConsumptionOrders || HeldEssential.FindRef(H.Id) > 0);
    constexpr int32 Budget = 32;
    const int32 Count = Network ? FMath::Min(Budget, Snapshot.Citizens.Num()) : Snapshot.Citizens.Num();
    const int32 Start = Network && Count ? (Step % Snapshot.Citizens.Num()) * Budget % Snapshot.Citizens.Num() : 0;
    TMap<FCityCellCoord, TMap<FCityCellCoord, int32>> DistanceCache;
    TMap<uint64, FCityCellCoord> Portals;
    if (Network)
        for (const auto& B : Snapshot.Businesses)
            if (!B.bIndustrial)
            {
                FCityCellCoord Portal;
                if (FCityTrafficSimulation::FindPortal(*City, Catalog, B.EntityId, Portal)) Portals.Add(B.EntityId, Portal);
            }
    const auto DistancesFrom = [&](const uint64 Origin) -> const TMap<FCityCellCoord, int32>* {
        FCityCellCoord Portal;
        if (!Network || !FCityTrafficSimulation::FindPortal(*City, Catalog, Origin, Portal)) return nullptr;
        if (!DistanceCache.Contains(Portal)) Network->FindWalkingDistances(Portal, DistanceCache.Add(Portal));
        return DistanceCache.Find(Portal);
    };
    const int32 StepsPerDay = FMath::Max(1, Tuning.FixedStepsPerMonth);
    for (int32 Offset = 0; Offset < Count; ++Offset)
    {
        auto& Buyer = Snapshot.Citizens[(Start + Offset) % Snapshot.Citizens.Num()];
        auto* H = Homes.FindRef(Buyer.HouseholdId);
        if (!H || Buyer.bOnMedicalLeave || Buyer.IsDetained() || Buyer.IsHomeless() || Buyer.ConsumptionNeed <= 0) continue;
        const int32 Existing = Snapshot.PurchaseReservations.IndexOfByPredicate([&](const auto& O) { return O.BuyerId == Buyer.Id; });
        if (Existing != INDEX_NONE && (!Network || Buyer.Schedule == ECityCitizenSchedule::ToShop)) continue;
        if (Existing == INDEX_NONE && Buyer.Schedule == ECityCitizenSchedule::Shop) continue;
        int64 Funds = 0;
        for (const auto* C : Members.FindChecked(H->Id)) Funds += Available.FindRef(C->Id);
        const auto* OldOrder = Existing != INDEX_NONE ? &Snapshot.PurchaseReservations[Existing] : nullptr;
        const uint64 OldShop = OldOrder ? OldOrder->ShopId : 0;
        const int32 OldQuantity = OldOrder ? OldOrder->Quantity : 0;
        const int32 Room = EssentialRoom(*H) - HeldEssential.FindRef(H->Id);
        const bool Essential = OldOrder ? OldOrder->ComfortTier == 0 : Restocking.FindRef(H->Id) && Room > 0;
        const int32 Desired = OldOrder ? OldOrder->ComfortTier :
            Essential ? 0 : Day >= H->LastComfortDay + 7 && !HeldComfort.FindRef(H->Id)
                ? PreferredComfortTier(*H, Funds, UnitPrice, Snapshot.Citizens) : 0;
        if (!OldOrder && ((!Essential && !Desired) || (Essential && Funds < UnitPrice))) continue;
        const bool HomeAfterMorning = Buyer.Schedule == ECityCitizenSchedule::Home && Buyer.NextActionStep > Step &&
            FCityCalendar::DayIndex(Buyer.NextActionStep, StepsPerDay) > Day && Step % StepsPerDay >= FCityCalendar::Duration(6.0, StepsPerDay);
        const uint64 Origin = HomeAfterMorning || Buyer.Schedule == ECityCitizenSchedule::ToHome ? Buyer.HomeEntityId :
            (Buyer.Schedule == ECityCitizenSchedule::Home || Buyer.Schedule == ECityCitizenSchedule::ToWork) && Buyer.WorkplaceEntityId ? Buyer.WorkplaceEntityId :
            Buyer.CurrentLocationEntityId ? Buyer.CurrentLocationEntityId : Buyer.HomeEntityId;
        const auto* Distances = DistancesFrom(Origin);
        const FCityBusinessLedgerRecord* Shop = nullptr;
        int32 BestDistance = MAX_int32, Quantity = 0, Quality = 0;
        int64 Price = 0;
        for (const auto& B : Snapshot.Businesses)
        {
            const int32 Held = HeldStock.FindRef(B.EntityId) - (B.EntityId == OldShop ? OldQuantity : 0);
            if (!CanReserveShop(B, Held)) continue;
            const int32 Tier = Essential ? 0 : FMath::Min(Desired, B.RetailTier);
            if (!Essential && Tier <= 0) continue;
            if (OldOrder && Tier != OldOrder->ComfortTier) continue;
            const auto* Portal = Portals.Find(B.EntityId);
            const auto* Distance = Distances && Portal ? Distances->Find(*Portal) : nullptr;
            if (Network && !Distance) continue;
            const int64 CostPerUnit = UnitPrice * (Tier + 1);
            int32 Goods = FMath::Min(B.Inventory - Held, B.PendingOrders - B.SalesThisMonth - Held);
            Goods = OldOrder ? (Goods >= OldQuantity ? OldQuantity : 0) : FMath::Min(Goods, Essential ? FMath::Min(H->bUsesStoredSupplies ? 7 : 1, Room) : 1);
            if (!OldOrder && CostPerUnit > 0) Goods = static_cast<int32>(FMath::Min<int64>(Goods, Funds / CostPerUnit));
            if (Goods <= 0 || (!OldOrder && !Essential && Funds - CostPerUnit < static_cast<int64>(H->ConsumptionOrders) * 7 * UnitPrice)) continue;
            const int32 Cost = Distance ? *Distance : 0;
            if (!Shop || Tier > Quality || (Tier == Quality && (Cost < BestDistance || (Cost == BestDistance && B.EntityId < Shop->EntityId))))
            { Shop = &B; BestDistance = Cost; Quantity = Goods; Quality = Tier; Price = OldOrder ? OldOrder->Price : Goods * CostPerUnit; }
        }
        if (OldOrder)
        {
            auto& Order = Snapshot.PurchaseReservations[Existing];
            if (!Shop)
            {
                HeldStock.FindChecked(OldShop) -= OldQuantity;
                (Essential ? HeldEssential : HeldComfort).FindChecked(H->Id) -= OldQuantity;
                for (const auto& C : Order.Contributions) Available.FindChecked(C.CitizenId) += C.Amount;
                Buyer.PendingShopEntityId = 0;
                Snapshot.PurchaseReservations.RemoveAt(Existing);
                continue;
            }
            if (OldShop != Shop->EntityId)
            {
                HeldStock.FindChecked(OldShop) -= OldQuantity; HeldStock.FindOrAdd(Shop->EntityId) += OldQuantity;
                Order.ShopId = Buyer.PendingShopEntityId = Shop->EntityId;
            }
        }
        else
        {
            if (!Shop) continue;
            FCityPurchaseReservation Order;
            Order.BuyerId = Buyer.Id; Order.HouseholdId = H->Id; Order.ShopId = Shop->EntityId;
            Order.Price = Price; Order.Quantity = Quantity; Order.ComfortTier = Quality;
            int64 Remaining = Price;
            for (const auto* C : Members.FindChecked(H->Id))
            {
                const int64 Amount = FMath::Min(Remaining, Available.FindRef(C->Id));
                if (Amount > 0) { Order.Contributions.Add({C->Id, Amount}); Available.FindChecked(C->Id) -= Amount; Remaining -= Amount; }
                if (!Remaining) break;
            }
            HeldStock.FindOrAdd(Shop->EntityId) += Quantity;
            (Essential ? HeldEssential : HeldComfort).FindOrAdd(H->Id) += Quantity;
            Buyer.PendingShopEntityId = Shop->EntityId;
            Snapshot.PurchaseReservations.Add(MoveTemp(Order));
        }
        if (Network && HomeAfterMorning)
        {
            Buyer.TripPurpose = ECityTripPurpose::Purchase; Buyer.TripDestinationEntityId = Buyer.PendingShopEntityId;
            Buyer.TripVehicleType = ECityTripVehicleType::Walk; Buyer.Schedule = ECityCitizenSchedule::ToShop;
            Buyer.CurrentLocationEntityId = Buyer.HomeEntityId; Buyer.NextActionStep = Step;
            Buyer.bShopBeforeWork = false;
        }
        else if (Network && Buyer.Schedule == ECityCitizenSchedule::Home && Buyer.NextActionStep <= Step &&
                 Buyer.WorkplaceEntityId && Step % StepsPerDay <= FCityCalendar::Duration(8.0, StepsPerDay))
        {
            // A pre-work stop must lie on a short, nearly direct walking commute.
            // Work->shop->home remains the normal alternative for other routes.
            const auto* FromHome = DistancesFrom(Buyer.HomeEntityId);
            FCityCellCoord Work;
            const auto* ShopPortal = Portals.Find(Buyer.PendingShopEntityId);
            const int32* ToShop = FromHome && ShopPortal ? FromHome->Find(*ShopPortal) : nullptr;
            const int32* ToWork = FromHome && FCityTrafficSimulation::FindPortal(*City, Catalog, Buyer.WorkplaceEntityId, Work) ? FromHome->Find(Work) : nullptr;
            // Distances may have moved when a new origin field was inserted.
            const int32 HomeShopDistance = ToShop ? *ToShop : MAX_int32;
            const int32 HomeWorkDistance = ToWork ? *ToWork : -1;
            const auto* FromWork = DistancesFrom(Buyer.WorkplaceEntityId);
            const int32* WorkToShop = FromWork && ShopPortal ? FromWork->Find(*ShopPortal) : nullptr;
            Buyer.bShopBeforeWork = HomeShopDistance <= 3 && HomeWorkDistance >= 0 && WorkToShop && HomeShopDistance + *WorkToShop <= HomeWorkDistance + 2;
        }
    }
}

void FCityPopulationSimulation::CompletePurchase(const uint64 BuyerId, const uint64 ShopId, const int64 Day)
{
	const int32 Index = Snapshot.PurchaseReservations.IndexOfByPredicate(
	    [&](const auto& O) { return O.BuyerId == BuyerId && O.ShopId == ShopId; });
	if (Index == INDEX_NONE)
		return; // Includes historical prepaid visits and repeated arrival callbacks.
	const FCityPurchaseReservation Order = Snapshot.PurchaseReservations[Index];
	Snapshot.PurchaseReservations.RemoveAt(Index); // Failed eligibility releases the locks atomically too.
	auto* Shop = Snapshot.Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == ShopId; });
	if (!Shop || Shop->bIndustrial || Shop->PendingOrders - Shop->SalesThisMonth < Order.Quantity || Shop->Inventory < Order.Quantity)
		return;
	const auto* Home = Snapshot.Households.FindByPredicate([&](const auto& H) { return H.Id == Order.HouseholdId; });
	if (Order.ComfortTier && (!Home || Home->SupplyUnits <= Home->ConsumptionOrders ||
	    Day < Home->LastComfortDay + 7 || Shop->RetailTier < Order.ComfortTier)) return;
	if (!Order.ComfortTier && Home &&
	    (Home->bUsesStoredSupplies ? Home->SupplyUnits + Order.Quantity > Home->ConsumptionOrders * 8
	                             : Home->PurchasesThisMonth + Order.Quantity > Home->ConsumptionOrders))
		return; // A changed household need cannot charge an excess arriving order.
	for (const auto& Contribution : Order.Contributions)
	{
		const auto* Member =
		    Snapshot.Citizens.FindByPredicate([&](const auto& C) { return C.Id == Contribution.CitizenId; });
		if (!Member || Member->HouseholdId != Order.HouseholdId || Member->AvailableMoney < Contribution.Amount)
			return;
	}
	for (const auto& Contribution : Order.Contributions)
		Snapshot.Citizens.FindByPredicate([&](const auto& C) { return C.Id == Contribution.CitizenId; })
		    ->AvailableMoney -= Contribution.Amount;
	Shop->Inventory -= Order.Quantity;
	Shop->Balance += Order.Price;
	Shop->SalesThisMonth += Order.Quantity;
	Snapshot.SalesThisMonth += Order.Quantity;
	Snapshot.SpendingThisMonth += Order.Price;
	Snapshot.TotalArrivalPurchases += Order.Quantity;
	for (auto& Household : Snapshot.Households)
		if (Household.Id == Order.HouseholdId)
        {
            Household.PurchasesThisMonth += Order.Quantity;
            if (Order.ComfortTier) { Household.LastComfortDay = Day; Household.ComfortTier = Order.ComfortTier; }
            else if (Household.bUsesStoredSupplies) { Household.SupplyUnits += Order.Quantity; Household.LastEssentialPurchaseDay = Day; }
        }
	RefreshHouseholdBalances();
}
