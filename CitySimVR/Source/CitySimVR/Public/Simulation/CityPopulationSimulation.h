#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityHousingHistory.h"

class FCityPlacementModel;
struct FCityEconomySnapshot;
struct FCityEconomyTuning;
struct FCityDevelopmentSnapshot;
struct FCityFireSnapshot;
struct FCityCrimeSnapshot;
class FCityWasteSimulation;
struct FCityWasteSnapshot;
struct FCityServiceSnapshot;
class FCityHealthSimulation;
struct FCityHealthSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityRoadNetworkSnapshot;
class UCityBuildingCatalog;

// These records are simulation data, not UObject/Actor state. Rendering and
// routing can later consume the trip intents without becoming authoritative.
enum class ECityCitizenActivity : uint8
{
	Home,
	Work,
	Shop
};
enum class ECityCitizenSchedule : uint8
{
	Home,
	ToWork,
	Work,
	ToShop,
	Shop,
	ToHome
};
enum class ECityHousingStatus : uint8
{
	Stable,
	Searching,
	CannotAfford,
	WaitingForCare,
	Settling,
	Recovering
};

enum class ECityEducationLevel : uint8
{
	None,
	Basic,
	Secondary,
	Higher
};
enum class ECityTripPurpose : uint8
{
	None,
	MoveInArrival,
	Commute,
	Purchase,
	FreightDelivery,
	FreightExport,
	OpeningDelivery,
	ReturnHome,
	FireResponse,
	PoliceResponse,
	EmergencyReturn,
	HealthWalkIn,
	HealthPickup,
	HealthAdmission,
	WastePickup,
	WasteReturn,
	HealthDriveIn,
	HealthReturnHome
};

enum class ECityTripVehicleType : uint8
{
	Walk,
	Car,
	FreightTruck,
	PoliceCar,
	FireEngine,
	Ambulance,
	GarbageTruck
};

// Current job assignments, including private and civic employers. Education
// describes the worker's qualification, independently of role, wage or commute.
struct CITYSIMVR_API FCityWorkforceEducationSnapshot
{
	int32 Uneducated = 0;
	int32 Basic = 0;
	int32 Secondary = 0;
	int32 Higher = 0;
	int32 Educated() const
	{
		return Basic + Secondary + Higher;
	}
	int32 Total() const
	{
		return Uneducated + Educated();
	}
};

struct CITYSIMVR_API FCityCitizenRecord
{
	uint64 Id = 0;
	uint64 JailStationId = 0;
	int64 JailReleaseStep = -1, ReformedUntilStep = 0;
	// Derived from authoritative health state; saved patients reconstruct this on load.
	bool bOnMedicalLeave = false;
	bool IsDetained() const
	{
		return JailStationId != 0;
	}
	uint64 HouseholdId = 0;
	uint64 HomeEntityId = 0;
	uint64 WorkplaceEntityId = 0;
	ECityEducationLevel Education = ECityEducationLevel::Basic;
	// Consecutive unemployment begins at this fixed step; -1 means employed.
	int64 UnemployedSinceStep = -1;
	int32 EducationMonths = 0;
	int64 LastEducationMonth = 0;
	// Monthly wellbeing authority; display/reconciliation never advances these clocks.
	int32 Happiness = 60, HappinessTarget = 60, HappinessConcerns = 0, DiscontentMonths = 0;
	int64 LastHappinessMonth = 0;
	int64 LastRelocationMonth = 0;
	ECityHousingStatus HousingStatus = ECityHousingStatus::Stable;
	bool bSkilledJob = false;
	bool IsHomeless() const
	{
		return HomeEntityId == 0;
	}
	int64 LastPolicyGrant = 0, LastPolicyGrantMonth = 0;
	int64 AvailableMoney = 0;
	int32 ConsumptionNeed = 0;
	ECityCitizenActivity Activity = ECityCitizenActivity::Home;
	ECityTripPurpose TripPurpose = ECityTripPurpose::None;
	ECityTripVehicleType TripVehicleType = ECityTripVehicleType::Walk;
	uint64 TripDestinationEntityId = 0;
	int64 NextActionStep = 0;
	uint64 CurrentLocationEntityId = 0;
	uint64 PendingShopEntityId = 0;
	ECityCitizenSchedule Schedule = ECityCitizenSchedule::Home;
	bool bShopBeforeWork = false;
};

struct CITYSIMVR_API FCityHouseholdRecord
{
	uint64 Id = 0;
	uint64 HomeEntityId = 0;
	int64 Balance = 0;
	int32 ConsumptionOrders = 0;
	int32 CompletedPurchases = 0;
	int32 PurchasesThisMonth = 0;
	// Daily goods units are stored at home; one bulk trip can carry seven units.
	// Current-format saves without this optional state enter the same starter buffer.
	bool bUsesStoredSupplies = false;
	int32 SupplyUnits = 0;
	int64 SupplyDay = -1;
	int64 LastEssentialPurchaseDay = -1;
	int64 LastComfortDay = -7;
	int32 ComfortTier = 0;
};

struct CITYSIMVR_API FCityBusinessLedgerRecord
{
	uint64 EntityId = 0;
	bool bIndustrial = false;
	// One imported setup consignment; distinct from stock available for sale.
	bool bInitialFreightReceived = false;
	int32 JobCapacity = 0;
	int32 SkilledJobCapacity = 0;
	int32 SkilledMinimumEducation = 2;
	int64 AiFeePaid = 0, AiPaidMonth = 0;
	// Last real contract receipt; kept through pause/outage, never fabricated from a toggle.
	uint64 AiCenterAtPayment = 0;
	int32 AiJobsAtPayment = 0, AiSpecialistsAtPayment = 0;
	int32 Inventory = 0;
	int32 PendingOrders = 0;
	int32 CompletedSales = 0;
	int32 SalesThisMonth = 0;
	int64 ExportedGoods = 0;
	int64 Balance = 0;
	int64 OpeningPayrollReserve = 0;
	int32 ProducedGoods = 0; // Last settled month, before warehouse overflow.
	int64 PayrollDue = 0;
	int64 PayrollPaid = 0;
	int32 RetailTier = 0;
	bool bHasReceivedGoods = false;
	int64 OpeningGraceUntilStep = -1;
};

struct CITYSIMVR_API FCityPublicEmployerRecord
{
	uint64 EntityId = 0;
	int32 JobCapacity = 0;
	int64 PayrollDue = 0;
	int64 PayrollPaid = 0;
};

// A trip exists independently of a currently occupied citizen.  This lets the
// city show arrivals, construction deliveries and freight before the later
// resident/business reconciliation makes a lot operational.
struct CITYSIMVR_API FCityTransitTripRecord
{
	uint64 Id = 0;
	uint64 OriginEntityId = 0;
	uint64 DestinationEntityId = 0;
	ECityTripPurpose Purpose = ECityTripPurpose::None;
	ECityTripVehicleType VehicleType = ECityTripVehicleType::Car;
	int32 Quantity = 0;
	int64 StartStep = 0;
	int64 ArrivalStep = 0;
	bool bCompleted = false;
	uint64 StationEntityId = 0;
	int64 IncidentId = 0;
	uint64 PatientCitizenId = 0;
	bool IsWasteJourney() const
	{
		return Purpose == ECityTripPurpose::WastePickup || Purpose == ECityTripPurpose::WasteReturn;
	}
	bool IsHealthJourney() const
	{
		return Purpose == ECityTripPurpose::HealthWalkIn || Purpose == ECityTripPurpose::HealthPickup ||
		       Purpose == ECityTripPurpose::HealthAdmission || Purpose == ECityTripPurpose::HealthDriveIn ||
		       Purpose == ECityTripPurpose::HealthReturnHome;
	}
	bool IsEmergency() const
	{
		return Purpose == ECityTripPurpose::FireResponse || Purpose == ECityTripPurpose::PoliceResponse ||
		       Purpose == ECityTripPurpose::EmergencyReturn || Purpose == ECityTripPurpose::HealthPickup ||
		       Purpose == ECityTripPurpose::HealthAdmission;
	}
};

struct FCityPurchaseContribution
{
	uint64 CitizenId = 0;
	int64 Amount = 0;
};
struct FCityPurchaseReservation
{
	uint64 BuyerId = 0;
	uint64 HouseholdId = 0;
	uint64 ShopId = 0;
	int64 Price = 0;
	TArray<FCityPurchaseContribution> Contributions;
	int32 Quantity = 1;
	int32 ComfortTier = 0; // Zero is essential; one/two are optional medium/high goods.
};

struct FCityHousingVacancyHold
{
	uint64 HomeEntityId = 0;
	int64 UntilMonth = 0;
};

struct CITYSIMVR_API FCityPopulationSnapshot
{
	bool bCommercialAiPilot = false;
	uint64 NextCitizenId = 1;
	uint64 NextTransitTripId = 1;
	int64 LastSettledMonthId = 0;
	int64 DiscardedGoods = 0;
	bool bUsesPurchaseReservations = false;
	int32 SalesThisMonth = 0;
	int32 LastMonthSales = 0;
	int64 ExportsThisMonth = 0;
	int64 LastMonthExports = 0;
	int64 SpendingThisMonth = 0;
	int64 LastMonthSpending = 0;
	int64 TotalArrivalPurchases = 0;
	int64 LastMigrationMonth = 0, ImmigrationResumeMonth = 0;
	int64 RelocatedResidents = 0, DepartedResidents = 0, MovingExpenses = 0, DepartureMoney = 0;
	TArray<FCityHousingVacancyHold> HousingVacancyHolds;
	uint64 NextHousingEventId = 1;
	TArray<FCityHousingEvent> HousingEvents;
	TArray<FCityPurchaseReservation> PurchaseReservations;
	TArray<FCityCitizenRecord> Citizens;
	TArray<FCityHouseholdRecord> Households;
	TArray<FCityBusinessLedgerRecord> Businesses;
	TArray<FCityPublicEmployerRecord> PublicEmployers;
	int32 Workers(uint64 EntityId) const;
	int32 ActiveWorkers(uint64 EntityId) const;
	int32 StaffPercent(uint64 EntityId, int32 RequiredJobs = 0) const;
	FCityWorkforceEducationSnapshot BuildWorkforceEducation() const;
	TArray<FCityTransitTripRecord> TransitTrips;
};

struct CITYSIMVR_API FCityPopulationAggregate
{
	int32 Residents = 0;
	int32 HousedResidents = 0;
	int32 HomelessResidents = 0;
	int32 UnemployedResidents = 0;
	int32 EducationCounts[4] = {};
	int32 Jobs = 0;
	int32 EmployedResidents = 0;
	int32 HouseholdGoodsDemand = 0;
	int32 GoodsProduced = 0;
	int32 RetailCapacity = 0;
	int32 GoodsSold = 0;
	int64 HouseholdIncome = 0;
	int64 HouseholdSpending = 0;
};

struct FCityServiceSnapshot;

class CITYSIMVR_API FCityPopulationSimulation
{
  public:
	// Four calendar days is a readable short-haul delivery at the normal one
	// minute month rate, while leaving time for the player to watch it arrive.
	static constexpr int64 ConstructionArrivalSteps = 4;
	void Reset();
	void SyncHealthAvailability(const FCityHealthSnapshot& Health, bool bPrepareQueries = true);
	void UpdateWasteService(FCityWasteSimulation& Waste, int64 Step, int32 StepsPerMonth,
	                        const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                        const FCityRoadNetworkSnapshot& Network, const FCityServiceSnapshot& Services,
	                        const FCityDevelopmentSnapshot& Development, bool Unpaid);
	void UpdateHospitalCare(FCityHealthSimulation& Health, const FCityPlacementModel& City,
	                        const UCityBuildingCatalog* Catalog, const FCityRoadNetworkSnapshot& Network,
	                        const FCityServiceSnapshot& Services, int64 Step, int32 StepsPerMonth,
	                        uint32 Seed, bool bUnpaidUpkeep, bool bAdvance,
	                        const FCityEnvironmentSnapshot* Environment = nullptr,
	                        const FCityWasteSnapshot* Waste = nullptr, bool bPrepareQueries = true);
	void SettlePolicies(struct FCityPolicyState& Policies, FCityPlacementModel& City, int64 Month,
	                    const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	                    bool EducationAvailable);
	void AdvanceEducation(int64 Month, const FCityServiceSnapshot& Services,
	                      const FCityEconomyTuning& Tuning);
	void AdvanceHappiness(int64 Month, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                      const FCityEnvironmentSnapshot& Environment, const FCityEconomySnapshot& Economy,
	                      const FCityEconomyTuning& Tuning, const FCityHealthSnapshot& Health,
	                      const FCityWasteSnapshot& Waste);
	bool AdvanceHousingMigration(int64 Month, const FCityPlacementModel& City,
	                             const FCityServiceSnapshot& Services,
	                             const FCityEnvironmentSnapshot& Environment,
	                             const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
	                             const FCityHealthSnapshot& Health, const FCityWasteSnapshot& Waste,
	                             const FCityCrimeSnapshot& Crime);
	void Reconcile(const FCityPlacementModel& City, const FCityEconomySnapshot& Economy,
	               const FCityEconomyTuning& Tuning, const FCityServiceSnapshot* Services = nullptr,
	               bool bPrepareQueries = true);
	void AdvanceToMonth(const FCityPlacementModel& City, const FCityEconomySnapshot& Economy,
	                    const FCityEconomyTuning& Tuning, const FCityServiceSnapshot* Services = nullptr,
	                    bool bUseFreightInventory = false, bool bReservePurchases = false,
	                    bool bPrepareQueries = true, struct FCityForecastTiming* Timing = nullptr);
	static int32 ForecastGoodsSales(const FCityPlacementModel& City, const FCityEconomySnapshot& Economy,
	                                const FCityEconomyTuning& Tuning, const FCityServiceSnapshot& Services,
	                                const FCityPopulationSnapshot& Population,
	                                int64* OutPayrollPaid = nullptr, int32* OutGoodsProduced = nullptr,
	                                int64* OutPublicPayroll = nullptr, int32* OutEmployed = nullptr,
	                                struct FCityForecastTiming* Timing = nullptr,
	                                bool bPrepareQueries = true, int64* OutGoodsSpending = nullptr);
	static int32 StaffedOutput(int32 Capacity, int32 Jobs, int32 Workers);
	static bool CanFillJob(ECityEducationLevel Education, ECityEducationLevel Required);
	static int64 MonthlySalaryLimit(ECityEducationLevel Education, const FCityEconomyTuning& Tuning);
	static constexpr int32 FreightLoadSize = 20;
	static constexpr int64 FreightUnitPrice = 2;
	// The same retail reservation eligibility serves authority and diagnostics.
	static bool CanReserveShop(const FCityBusinessLedgerRecord& Shop, int32 HeldStock)
	{
		return !Shop.bIndustrial && Shop.PendingOrders - Shop.SalesThisMonth > HeldStock && Shop.Inventory > HeldStock;
	}
	void QueueFreight(int64 FixedStepId);
	void UpdateEmergencyResponses(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                              const FCityRoadNetworkSnapshot& Network, const FCityFireSnapshot& Fire,
	                              const FCityCrimeSnapshot& Crime, const FCityServiceSnapshot& Services,
	                              int64 Step, bool bUnpaidUpkeep);
	static int32 PublicJobs(FName DefinitionId, const FCityEconomyTuning& Tuning);
	static constexpr int32 NearbyFireResponseHops = 12;
	void PrunePurchaseReservations(const FCityPlacementModel& City);
	void RetryPurchaseReservations(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                               const FCityRoadNetworkSnapshot& Network, int64 Step,
	                               const FCityEconomyTuning& Tuning,
	                               const FCityDevelopmentSnapshot* Development = nullptr);
	void EnableHouseholdSupplies(int64 Day);
	static int32 StoredEssentialShortage(const FCityPopulationSnapshot& State, int64 Day = -1);
	static int32 PreferredComfortTier(const FCityHouseholdRecord& Home, int64 Funds, int64 UnitPrice,
	                                  const TArray<FCityCitizenRecord>& Citizens);
	static bool ValidatePurchaseReservations(const FCityPopulationSnapshot& State, FString& OutError);
	void AdvanceCitizenSchedules(const FCityPlacementModel& City, int64 FixedStepId,
	                             int32 StepsPerDay = FCityCalendar::DefaultStepsPerDay);
	void CompleteCitizenTrip(uint64 CitizenId, uint64 Origin, uint64 Destination, int64 FixedStepId, int32 StepsPerDay = FCityCalendar::DefaultStepsPerDay);
	void AdvanceTransitToStep(int64 FixedStepId);
	void CompleteTransitTrip(uint64 TripId, int64 FixedStepId);
	void PruneTransitTrips(const FCityPlacementModel& City);
	void QueueConstructionArrival(uint64 DestinationEntityId, int64 FixedStepId);
	void QueueBusinessOpenings(const FCityDevelopmentSnapshot& Development, int64 FixedStepId,
	                           int32 StepsPerDay = FCityCalendar::DefaultStepsPerDay);
	void MarkExistingBusinessesOpenForScenario();
	void SettleCommercialAi(const FCityServiceSnapshot& Services);
	void SetCommercialAiPilot(bool Enabled) { Snapshot.bCommercialAiPilot = Enabled; }
	bool RestoreState(const FCityPopulationSnapshot& InSnapshot, FString& OutError);
	const FCityPopulationSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}
	FCityPopulationAggregate BuildAggregate(const FCityEconomyTuning& Tuning) const;
	bool MatchesEconomy(const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning) const;
	// Call immediately after freight settlement, before care or traffic changes availability.
	// Production uses current workers; the economy's production forecast predates this day's care.
	bool MatchesSettledEconomy(const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning) const;

  private:
	friend class FCityHospitalServiceTest;
	friend class FCityPreparedPopulationQueriesTest;
	friend class FCityCrimeSimulation;
	static bool IsHome(const FName DefinitionId);
	static bool IsBusiness(const FName DefinitionId);
	void SettleMonth(const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
	                 bool bUseFreightInventory, bool bReservePurchases = false,
	                 int64 PublicPayrollBudget = MAX_int64, bool bPrepareQueries = true);
	void ReservePurchases(const FCityEconomyTuning& Tuning, const FCityPlacementModel* City = nullptr,
	                      const UCityBuildingCatalog* Catalog = nullptr,
	                      const FCityRoadNetworkSnapshot* Network = nullptr, int64 Step = 0);
	void CompletePurchase(uint64 BuyerId, uint64 ShopId, int64 Day);
	void RefreshHouseholdSupplies(int64 Day);
	void RefreshHouseholdBalances();
	FCityPopulationSnapshot Snapshot;
	struct FCityForecastTiming* ForecastTiming = nullptr;
};
