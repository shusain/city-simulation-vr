#include "Simulation/CityPopulationSimulation.h"
#include "Commands/CityPlacementModel.h"

void FCityPopulationSimulation::AdvanceCitizenSchedules(const FCityPlacementModel& City, const int64 Step, const int32 StepsPerDay)
{
	TMap<uint64, const FCityBusinessLedgerRecord*> Businesses;
	for (const auto& Business : Snapshot.Businesses)
		Businesses.Add(Business.EntityId, &Business);
	for (FCityCitizenRecord& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained() || Citizen.bOnMedicalLeave)
			continue;
		auto ResetHome = [&]()
		{
			Citizen.CurrentLocationEntityId = Citizen.HomeEntityId;
			Citizen.Activity = ECityCitizenActivity::Home;
			Citizen.Schedule = ECityCitizenSchedule::Home;
			Citizen.TripPurpose = ECityTripPurpose::None;
			Citizen.TripDestinationEntityId = 0;
			Citizen.bShopBeforeWork = false;
			Citizen.NextActionStep = FCityCalendar::NextMorning(Step, Citizen.Id, StepsPerDay);
		};
		if (!City.GetEntities().Contains(Citizen.HomeEntityId))
			continue; // Homeless citizens retain their data; visible routes require a home portal.
		if (!Citizen.CurrentLocationEntityId || !City.GetEntities().Contains(Citizen.CurrentLocationEntityId))
			ResetHome();
		const auto* const* Workplace = Businesses.Find(Citizen.WorkplaceEntityId);
		const bool bPublicWork = Snapshot.PublicEmployers.ContainsByPredicate(
		    [&](const auto& E) { return E.EntityId == Citizen.WorkplaceEntityId && E.JobCapacity > 0; });
		const bool bCanWork = ((Workplace && (*Workplace)->JobCapacity > 0) || bPublicWork) &&
		                      City.GetEntities().Contains(Citizen.WorkplaceEntityId);
		const auto* const* Shop = Businesses.Find(Citizen.PendingShopEntityId);
		const bool bCanShop = Shop && !(*Shop)->bIndustrial && (*Shop)->PendingOrders > 0 &&
		                      City.GetEntities().Contains(Citizen.PendingShopEntityId);
		if (!bCanShop)
			Citizen.PendingShopEntityId = 0;
		if (Citizen.TripPurpose != ECityTripPurpose::None)
		{
			const bool bInvalidWork = Citizen.Schedule == ECityCitizenSchedule::ToWork &&
			                          (!bCanWork || Citizen.TripDestinationEntityId != Citizen.WorkplaceEntityId);
			const auto* const* Destination = Businesses.Find(Citizen.TripDestinationEntityId);
			const bool bInvalidShop =
			    Citizen.Schedule == ECityCitizenSchedule::ToShop &&
			    (!Destination || (*Destination)->bIndustrial || (*Destination)->PendingOrders <= 0);
			if (bInvalidWork || bInvalidShop || !City.GetEntities().Contains(Citizen.TripDestinationEntityId))
			{
				// Cancel without an arrival or transaction. Retry from the last real origin.
				Citizen.TripPurpose = ECityTripPurpose::None;
				Citizen.TripDestinationEntityId = 0;
				Citizen.Schedule = Citizen.CurrentLocationEntityId == Citizen.HomeEntityId ? ECityCitizenSchedule::Home
				                                                                           : ECityCitizenSchedule::Work;
				Citizen.NextActionStep = Step + 1;
			}
			continue;
		}
		if (Step < Citizen.NextActionStep)
			continue;
		auto Depart = [&](const uint64 Destination, const ECityTripPurpose Purpose, const ECityCitizenSchedule Schedule)
		{
			Citizen.TripDestinationEntityId = Destination;
			Citizen.TripPurpose = Purpose;
			Citizen.Schedule = Schedule;
			Citizen.TripVehicleType =
			    Purpose == ECityTripPurpose::Purchase || Citizen.Activity == ECityCitizenActivity::Shop
			        ? ECityTripVehicleType::Walk
			        : (Citizen.Id % 3 == 0 ? ECityTripVehicleType::Car : ECityTripVehicleType::Walk);
		};
		if (Citizen.Schedule == ECityCitizenSchedule::Home && bCanWork && bCanShop && Citizen.bShopBeforeWork)
			Depart(Citizen.PendingShopEntityId, ECityTripPurpose::Purchase, ECityCitizenSchedule::ToShop);
		else if ((Citizen.Schedule == ECityCitizenSchedule::Home ||
		          (Citizen.Schedule == ECityCitizenSchedule::Shop && Citizen.bShopBeforeWork)) && bCanWork)
			Depart(Citizen.WorkplaceEntityId, ECityTripPurpose::Commute, ECityCitizenSchedule::ToWork);
		else if (Citizen.Schedule != ECityCitizenSchedule::Shop && bCanShop)
			Depart(Citizen.PendingShopEntityId, ECityTripPurpose::Purchase, ECityCitizenSchedule::ToShop);
		else if (Citizen.CurrentLocationEntityId != Citizen.HomeEntityId)
			Depart(Citizen.HomeEntityId, ECityTripPurpose::ReturnHome, ECityCitizenSchedule::ToHome);
		else
			Citizen.NextActionStep = Step + 1;
	}
}

void FCityPopulationSimulation::CompleteCitizenTrip(const uint64 CitizenId, const uint64 Origin,
                                                    const uint64 Destination, const int64 Step, const int32 StepsPerDay)
{
	FCityCitizenRecord* Citizen = Snapshot.Citizens.FindByPredicate([&](const auto& C) { return C.Id == CitizenId; });
	if (!Citizen || Citizen->TripPurpose == ECityTripPurpose::None || Citizen->CurrentLocationEntityId != Origin ||
	    Citizen->TripDestinationEntityId != Destination)
		return;
	switch (Citizen->Schedule)
	{
	case ECityCitizenSchedule::ToWork:
		Citizen->bShopBeforeWork = false;
		Citizen->Activity = ECityCitizenActivity::Work;
		Citizen->Schedule = ECityCitizenSchedule::Work;
		Citizen->NextActionStep = Step + FCityCalendar::Duration(6.0, StepsPerDay);
		break;
	case ECityCitizenSchedule::ToShop:
		CompletePurchase(CitizenId, Destination, FCityCalendar::DayIndex(Step, StepsPerDay));
		Citizen->Activity = ECityCitizenActivity::Shop;
		Citizen->Schedule = ECityCitizenSchedule::Shop;
		Citizen->NextActionStep = Step + FCityCalendar::Duration(0.5, StepsPerDay);
		if (Citizen->PendingShopEntityId == Destination)
			Citizen->PendingShopEntityId = 0;
		break;
	case ECityCitizenSchedule::ToHome:
		Citizen->bShopBeforeWork = false;
		Citizen->Activity = ECityCitizenActivity::Home;
		Citizen->Schedule = ECityCitizenSchedule::Home;
		Citizen->NextActionStep = FCityCalendar::NextMorning(Step, Citizen->Id, StepsPerDay);
		break;
	default:
		return;
	}
	Citizen->CurrentLocationEntityId = Destination;
	Citizen->TripPurpose = ECityTripPurpose::None;
	Citizen->TripDestinationEntityId = 0;
}
