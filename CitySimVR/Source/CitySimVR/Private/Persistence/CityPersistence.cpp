#include "Persistence/CityPersistence.h"
#include "Persistence/CityWastePersistence.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingMigration.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString Int64String(const int64 Value)
{
	return LexToString(Value);
}
FString UInt64String(const uint64 Value)
{
	return LexToString(Value);
}

bool ReadInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& OutValue, FString& OutError)
{
	FString Text;
	if (!Object.IsValid() || !Object->TryGetStringField(Field, Text) || !LexTryParseString(OutValue, *Text))
	{
		OutError = FString::Printf(TEXT("Missing or invalid integer field: %s"), Field);
		return false;
	}
	return true;
}

bool ReadUInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, uint64& OutValue,
                FString& OutError)
{
	FString Text;
	if (!Object.IsValid() || !Object->TryGetStringField(Field, Text) || !LexTryParseString(OutValue, *Text))
	{
		OutError = FString::Printf(TEXT("Missing or invalid unsigned integer field: %s"), Field);
		return false;
	}
	return true;
}

bool ReadInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& OutValue, FString& OutError)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number) ||
	    Number != FMath::TruncToDouble(Number) || Number < MIN_int32 || Number > MAX_int32)
	{
		OutError = FString::Printf(TEXT("Missing or invalid number field: %s"), Field);
		return false;
	}
	OutValue = static_cast<int32>(Number);
	return true;
}

bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& OutValue, FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetBoolField(Field, OutValue))
	{
		OutError = FString::Printf(TEXT("Missing or invalid boolean field: %s"), Field);
		return false;
	}
	return true;
}

TSharedRef<FJsonObject> WriteCell(const FCityCellCoord& Cell, const bool bWriteLevel = false)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("x"), Cell.X);
	Object->SetNumberField(TEXT("y"), Cell.Y);
	if (bWriteLevel)
		Object->SetNumberField(TEXT("level"), Cell.RoadLevel);
	return Object;
}

bool ReadCell(const TSharedPtr<FJsonObject>& Object, FCityCellCoord& OutCell, FString& OutError)
{
	return ReadInt32(Object, TEXT("x"), OutCell.X, OutError) &&
	       ReadInt32(Object, TEXT("y"), OutCell.Y, OutError);
}

TSharedRef<FJsonObject> WriteEconomy(const FCitySaveDocument& Document)
{
	const FCityEconomySnapshot& Value = Document.Economy;
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("fixedStepId"), Int64String(Value.FixedStepId));
	Object->SetStringField(TEXT("monthId"), Int64String(Value.MonthId));
	Object->SetStringField(TEXT("lastBilledMonthId"), Int64String(Value.LastBilledMonthId));
	Object->SetNumberField(TEXT("occupiedHomes"), Value.OccupiedHomes);
	Object->SetNumberField(TEXT("builtShops"), Value.BuiltShops);
	Object->SetNumberField(TEXT("operationalShops"), Value.OperationalShops);
	Object->SetNumberField(TEXT("commercialOutput"), Value.CommercialOutput);
	Object->SetNumberField(TEXT("builtIndustries"), Value.BuiltIndustries);
	Object->SetNumberField(TEXT("operationalIndustries"), Value.OperationalIndustries);
	Object->SetNumberField(TEXT("industrialOutput"), Value.IndustrialOutput);
	Object->SetNumberField(TEXT("builtParks"), Value.BuiltParks);
	Object->SetNumberField(TEXT("builtPower"), Value.BuiltPower);
	Object->SetNumberField(TEXT("builtService"), Value.BuiltService);
	Object->SetNumberField(TEXT("residents"), Value.Residents);
	Object->SetNumberField(TEXT("jobs"), Value.Jobs);
	Object->SetNumberField(TEXT("employedResidents"), Value.EmployedResidents);
	Object->SetNumberField(TEXT("householdGoodsDemand"), Value.HouseholdGoodsDemand);
	Object->SetNumberField(TEXT("goodsProduced"), Value.GoodsProduced);
	Object->SetNumberField(TEXT("retailCapacity"), Value.RetailCapacity);
	Object->SetNumberField(TEXT("goodsSold"), Value.GoodsSold);
	Object->SetNumberField(TEXT("unmetGoodsDemand"), Value.UnmetGoodsDemand);
	Object->SetStringField(TEXT("householdIncome"), Int64String(Value.HouseholdIncome));
	Object->SetStringField(TEXT("householdSpending"), Int64String(Value.HouseholdSpending));
	Object->SetStringField(TEXT("lastIncome"), Int64String(Value.LastIncome));
	Object->SetStringField(TEXT("lastResidentialTaxRevenue"), Int64String(Value.LastResidentialTaxRevenue));
	Object->SetStringField(TEXT("lastCommercialTaxRevenue"), Int64String(Value.LastCommercialTaxRevenue));
	Object->SetStringField(TEXT("lastIndustrialTaxRevenue"), Int64String(Value.LastIndustrialTaxRevenue));
	Object->SetNumberField(TEXT("residentialTaxBasisPoints"), Value.ResidentialTaxBasisPoints);
	Object->SetNumberField(TEXT("commercialTaxBasisPoints"), Value.CommercialTaxBasisPoints);
	Object->SetNumberField(TEXT("industrialTaxBasisPoints"), Value.IndustrialTaxBasisPoints);
	Object->SetStringField(TEXT("lastUpkeep"), Int64String(Value.LastUpkeep));
	Object->SetStringField(TEXT("residentialUpkeep"), Int64String(Value.ResidentialUpkeep));
	Object->SetStringField(TEXT("commercialUpkeep"), Int64String(Value.CommercialUpkeep));
	Object->SetStringField(TEXT("industrialUpkeep"), Int64String(Value.IndustrialUpkeep));
	Object->SetStringField(TEXT("parkUpkeep"), Int64String(Value.ParkUpkeep));
	Object->SetStringField(TEXT("civicUpkeep"), Int64String(Value.CivicUpkeep));
	Object->SetStringField(TEXT("utilityUpgradeUpkeep"), Int64String(Value.UtilityUpgradeUpkeep));
	Object->SetStringField(TEXT("publicPayrollDue"), Int64String(Value.PublicPayrollDue));
	Object->SetStringField(TEXT("lastPublicPayrollPaid"), Int64String(Value.LastPublicPayrollPaid));
	Object->SetStringField(TEXT("lastPublicPayrollDue"), Int64String(Value.LastPublicPayrollDue));
	Object->SetStringField(TEXT("upkeepShortfall"), Int64String(Value.UpkeepShortfall));
	Object->SetBoolField(TEXT("growthBlockedByShortfall"), Value.bGrowthBlockedByShortfall);
	{
		Object->SetStringField(TEXT("loanOriginalPrincipal"), Int64String(Value.Loan.OriginalPrincipal));
		Object->SetStringField(TEXT("loanPrincipal"), Int64String(Value.Loan.Principal));
		Object->SetStringField(TEXT("loanInterestDue"), Int64String(Value.Loan.InterestDue));
		Object->SetStringField(TEXT("loanStartedMonth"), Int64String(Value.Loan.StartedMonth));
		Object->SetStringField(TEXT("loanMonthsRemaining"), Int64String(Value.Loan.MonthsRemaining));
		Object->SetStringField(TEXT("loanPendingBorrowed"), Int64String(Value.Loan.PendingBorrowed));
		Object->SetStringField(TEXT("loanPendingPrincipalPaid"),
		                       Int64String(Value.Loan.PendingPrincipalPaid));
		Object->SetStringField(TEXT("loanPendingInterestPaid"), Int64String(Value.Loan.PendingInterestPaid));
		Object->SetStringField(TEXT("loanLastBorrowed"), Int64String(Value.Loan.LastBorrowed));
		Object->SetStringField(TEXT("loanLastPrincipalPaid"), Int64String(Value.Loan.LastPrincipalPaid));
		Object->SetStringField(TEXT("loanLastInterestPaid"), Int64String(Value.Loan.LastInterestPaid));
		Object->SetStringField(TEXT("loanLastInterestAccrued"), Int64String(Value.Loan.LastInterestAccrued));
	}
	{
		Object->SetStringField(TEXT("policyMonth"), Int64String(Value.Policies.LastSettledMonth));
		Object->SetBoolField(TEXT("policy0bEnabled"), Value.Policies.Programs[0].bEnabled);
		Object->SetBoolField(TEXT("policy0bLastEnabled"), Value.Policies.Programs[0].bLastEnabled);
		Object->SetStringField(TEXT("policy0Eligible"), Int64String(Value.Policies.Programs[0].Eligible));
		Object->SetStringField(TEXT("policy0Recipients"), Int64String(Value.Policies.Programs[0].Recipients));
		Object->SetStringField(TEXT("policy0GrantsDue"), Int64String(Value.Policies.Programs[0].GrantsDue));
		Object->SetStringField(TEXT("policy0GrantsPaid"), Int64String(Value.Policies.Programs[0].GrantsPaid));
		Object->SetStringField(TEXT("policy0AdministrationPaid"),
		                       Int64String(Value.Policies.Programs[0].AdministrationPaid));
		Object->SetBoolField(TEXT("policy1bEnabled"), Value.Policies.Programs[1].bEnabled);
		Object->SetBoolField(TEXT("policy1bLastEnabled"), Value.Policies.Programs[1].bLastEnabled);
		Object->SetStringField(TEXT("policy1Eligible"), Int64String(Value.Policies.Programs[1].Eligible));
		Object->SetStringField(TEXT("policy1Recipients"), Int64String(Value.Policies.Programs[1].Recipients));
		Object->SetStringField(TEXT("policy1GrantsDue"), Int64String(Value.Policies.Programs[1].GrantsDue));
		Object->SetStringField(TEXT("policy1GrantsPaid"), Int64String(Value.Policies.Programs[1].GrantsPaid));
		Object->SetStringField(TEXT("policy1AdministrationPaid"),
		                       Int64String(Value.Policies.Programs[1].AdministrationPaid));
	}
	Object->SetNumberField(TEXT("accumulatorSeconds"), Document.EconomyAccumulatorSeconds);
	Object->SetBoolField(TEXT("paused"), Document.bSimulationPaused);
	Object->SetNumberField(TEXT("speed"), Document.SimulationSpeed);
	return Object;
}

bool ReadEconomy(const TSharedPtr<FJsonObject>& Object, FCitySaveDocument& Document, FString& OutError)
{
	FCityEconomySnapshot& Value = Document.Economy;
	const bool bCoreValid =
	    ReadInt64(Object, TEXT("fixedStepId"), Value.FixedStepId, OutError) &&
	    ReadInt64(Object, TEXT("monthId"), Value.MonthId, OutError) &&
	    ReadInt64(Object, TEXT("lastBilledMonthId"), Value.LastBilledMonthId, OutError) &&
	    ReadInt32(Object, TEXT("occupiedHomes"), Value.OccupiedHomes, OutError) &&
	    ReadInt32(Object, TEXT("builtShops"), Value.BuiltShops, OutError) &&
	    ReadInt32(Object, TEXT("operationalShops"), Value.OperationalShops, OutError) &&
	    ReadInt32(Object, TEXT("builtPower"), Value.BuiltPower, OutError) &&
	    ReadInt32(Object, TEXT("builtService"), Value.BuiltService, OutError) &&
	    ReadInt32(Object, TEXT("residents"), Value.Residents, OutError) &&
	    ReadInt32(Object, TEXT("jobs"), Value.Jobs, OutError) &&
	    ReadInt64(Object, TEXT("lastIncome"), Value.LastIncome, OutError) &&
	    ReadInt64(Object, TEXT("lastUpkeep"), Value.LastUpkeep, OutError) &&
	    ReadInt64(Object, TEXT("upkeepShortfall"), Value.UpkeepShortfall, OutError) &&
	    ReadBool(Object, TEXT("growthBlockedByShortfall"), Value.bGrowthBlockedByShortfall, OutError) &&
	    Object->TryGetNumberField(TEXT("accumulatorSeconds"), Document.EconomyAccumulatorSeconds) &&
	    ReadBool(Object, TEXT("paused"), Document.bSimulationPaused, OutError) &&
	    ReadInt32(Object, TEXT("speed"), Document.SimulationSpeed, OutError);
	if (!bCoreValid)
	{
		return false;
	}
	if ((!(ReadInt64(Object, TEXT("policyMonth"), Value.Policies.LastSettledMonth, OutError) &&
	       ReadBool(Object, TEXT("policy0bEnabled"), Value.Policies.Programs[0].bEnabled, OutError) &&
	       ReadBool(Object, TEXT("policy0bLastEnabled"), Value.Policies.Programs[0].bLastEnabled, OutError) &&
	       ReadInt64(Object, TEXT("policy0Eligible"), Value.Policies.Programs[0].Eligible, OutError) &&
	       ReadInt64(Object, TEXT("policy0Recipients"), Value.Policies.Programs[0].Recipients, OutError) &&
	       ReadInt64(Object, TEXT("policy0GrantsDue"), Value.Policies.Programs[0].GrantsDue, OutError) &&
	       ReadInt64(Object, TEXT("policy0GrantsPaid"), Value.Policies.Programs[0].GrantsPaid, OutError) &&
	       ReadInt64(Object, TEXT("policy0AdministrationPaid"), Value.Policies.Programs[0].AdministrationPaid,
	                 OutError) &&
	       ReadBool(Object, TEXT("policy1bEnabled"), Value.Policies.Programs[1].bEnabled, OutError) &&
	       ReadBool(Object, TEXT("policy1bLastEnabled"), Value.Policies.Programs[1].bLastEnabled, OutError) &&
	       ReadInt64(Object, TEXT("policy1Eligible"), Value.Policies.Programs[1].Eligible, OutError) &&
	       ReadInt64(Object, TEXT("policy1Recipients"), Value.Policies.Programs[1].Recipients, OutError) &&
	       ReadInt64(Object, TEXT("policy1GrantsDue"), Value.Policies.Programs[1].GrantsDue, OutError) &&
	       ReadInt64(Object, TEXT("policy1GrantsPaid"), Value.Policies.Programs[1].GrantsPaid, OutError) &&
	       ReadInt64(Object, TEXT("policy1AdministrationPaid"), Value.Policies.Programs[1].AdministrationPaid,
	                 OutError)) ||
	     !Value.Policies.Validate(Value.LastBilledMonthId, OutError)))
		return false;
	{
		int64 Remaining = 0;
		if (!(ReadInt64(Object, TEXT("loanOriginalPrincipal"), Value.Loan.OriginalPrincipal, OutError) &&
		      ReadInt64(Object, TEXT("loanPrincipal"), Value.Loan.Principal, OutError) &&
		      ReadInt64(Object, TEXT("loanInterestDue"), Value.Loan.InterestDue, OutError) &&
		      ReadInt64(Object, TEXT("loanStartedMonth"), Value.Loan.StartedMonth, OutError) &&
		      ReadInt64(Object, TEXT("loanMonthsRemaining"), Remaining, OutError) &&
		      ReadInt64(Object, TEXT("loanPendingBorrowed"), Value.Loan.PendingBorrowed, OutError) &&
		      ReadInt64(Object, TEXT("loanPendingPrincipalPaid"), Value.Loan.PendingPrincipalPaid,
		                OutError) &&
		      ReadInt64(Object, TEXT("loanPendingInterestPaid"), Value.Loan.PendingInterestPaid, OutError) &&
		      ReadInt64(Object, TEXT("loanLastBorrowed"), Value.Loan.LastBorrowed, OutError) &&
		      ReadInt64(Object, TEXT("loanLastPrincipalPaid"), Value.Loan.LastPrincipalPaid, OutError) &&
		      ReadInt64(Object, TEXT("loanLastInterestPaid"), Value.Loan.LastInterestPaid, OutError) &&
		      ReadInt64(Object, TEXT("loanLastInterestAccrued"), Value.Loan.LastInterestAccrued, OutError)))
			return false;
		if (Remaining < 0 || Remaining > FCityLoanState::TermMonths)
			return false;
		Value.Loan.MonthsRemaining = static_cast<int32>(Remaining);
		if (!Value.Loan.Validate(Value.LastBilledMonthId, OutError))
			return false;
	}

	const bool bTaxValid =
	    ReadInt64(Object, TEXT("lastResidentialTaxRevenue"), Value.LastResidentialTaxRevenue, OutError) &&
	    ReadInt64(Object, TEXT("lastCommercialTaxRevenue"), Value.LastCommercialTaxRevenue, OutError) &&
	    ReadInt32(Object, TEXT("residentialTaxBasisPoints"), Value.ResidentialTaxBasisPoints, OutError) &&
	    ReadInt32(Object, TEXT("commercialTaxBasisPoints"), Value.CommercialTaxBasisPoints, OutError);
	if (!bTaxValid)
	{
		return false;
	}
	{
		if (!ReadInt32(Object, TEXT("industrialTaxBasisPoints"), Value.IndustrialTaxBasisPoints, OutError) ||
		    !ReadInt64(Object, TEXT("lastIndustrialTaxRevenue"), Value.LastIndustrialTaxRevenue, OutError))
			return false;
		if (Value.IndustrialTaxBasisPoints < 0 ||
		    Value.IndustrialTaxBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints ||
		    Value.LastIndustrialTaxRevenue < 0)
		{
			OutError = TEXT("Invalid industrial tax policy or revenue");
			return false;
		}
	}

	const bool bIndustryValid =
	    ReadInt32(Object, TEXT("builtIndustries"), Value.BuiltIndustries, OutError) &&
	    ReadInt32(Object, TEXT("operationalIndustries"), Value.OperationalIndustries, OutError) &&
	    ReadInt32(Object, TEXT("industrialOutput"), Value.IndustrialOutput, OutError);
	if (!bIndustryValid)
	{
		return bIndustryValid;
	}
	if (!ReadInt64(Object, TEXT("residentialUpkeep"), Value.ResidentialUpkeep, OutError))
	{
		return false;
	}

	const bool bDensityEconomyValid =
	    ReadInt32(Object, TEXT("commercialOutput"), Value.CommercialOutput, OutError) &&
	    ReadInt64(Object, TEXT("commercialUpkeep"), Value.CommercialUpkeep, OutError) &&
	    ReadInt64(Object, TEXT("industrialUpkeep"), Value.IndustrialUpkeep, OutError);
	if (!bDensityEconomyValid)
		return false;
	return ReadInt32(Object, TEXT("builtParks"), Value.BuiltParks, OutError) &&
	       ReadInt32(Object, TEXT("employedResidents"), Value.EmployedResidents, OutError) &&
	       ReadInt32(Object, TEXT("householdGoodsDemand"), Value.HouseholdGoodsDemand, OutError) &&
	       ReadInt32(Object, TEXT("goodsProduced"), Value.GoodsProduced, OutError) &&
	       ReadInt32(Object, TEXT("retailCapacity"), Value.RetailCapacity, OutError) &&
	       ReadInt32(Object, TEXT("goodsSold"), Value.GoodsSold, OutError) &&
	       ReadInt32(Object, TEXT("unmetGoodsDemand"), Value.UnmetGoodsDemand, OutError) &&
	       ReadInt64(Object, TEXT("householdIncome"), Value.HouseholdIncome, OutError) &&
	       ReadInt64(Object, TEXT("householdSpending"), Value.HouseholdSpending, OutError) &&
	       ReadInt64(Object, TEXT("parkUpkeep"), Value.ParkUpkeep, OutError) &&
	       (ReadInt64(Object, TEXT("civicUpkeep"), Value.CivicUpkeep, OutError)) &&
	       (ReadInt64(Object, TEXT("utilityUpgradeUpkeep"), Value.UtilityUpgradeUpkeep, OutError)) &&
	       ((ReadInt64(Object, TEXT("publicPayrollDue"), Value.PublicPayrollDue, OutError) &&
	         ReadInt64(Object, TEXT("lastPublicPayrollPaid"), Value.LastPublicPayrollPaid, OutError) &&
	         ReadInt64(Object, TEXT("lastPublicPayrollDue"), Value.LastPublicPayrollDue, OutError)));
}
} // namespace

FCitySaveDocument::FCitySaveDocument(const FCityGridDimensions Dimensions)
    : GridWidth(Dimensions.GetWidth()), GridHeight(Dimensions.GetHeight()), Environment(Dimensions)
{
	checkf(Dimensions.IsValid(), TEXT("Validate save dimensions before constructing city fields"));
	Placement.Dimensions = Terrain.Dimensions = Crime.Dimensions = Fire.Dimensions = Waste.Dimensions =
	    Disasters.Dimensions = Dimensions;
}

bool FCityPersistence::ValidatePlatform(const FCityGridDimensions Dimensions,
                                        const ECitySavePlatform Platform, FString& OutError)
{
	if (Dimensions.GetWidth() != Dimensions.GetHeight() ||
	    (Dimensions.GetWidth() != 64 && Dimensions.GetWidth() != 80))
	{
		OutError = TEXT("Supported save grids are square64 or80; city data is never cropped");
		return false;
	}
	if (Platform != ECitySavePlatform::Any && Platform != ECitySavePlatform::Desktop &&
	    Platform != ECitySavePlatform::PCVR && Platform != ECitySavePlatform::Quest)
	{
		OutError = TEXT("Unknown save platform policy");
		return false;
	}
	if (Dimensions.GetWidth() > 64 &&
	    (Platform == ECitySavePlatform::Quest || Platform == ECitySavePlatform::PCVR))
	{
		OutError = FString::Printf(TEXT("This %dx%d city requires Desktop. %s currently supports64x64; the "
		                                "original save is unchanged"),
		                           Dimensions.GetWidth(), Dimensions.GetHeight(),
		                           Platform == ECitySavePlatform::Quest ? TEXT("Quest") : TEXT("PCVR"));
		return false;
	}
	return true;
}

ECitySavePlatform FCityPersistence::NativePlatform(const bool bIsXR)
{
#if PLATFORM_ANDROID
	return ECitySavePlatform::Quest;
#else
	return bIsXR ? ECitySavePlatform::PCVR : ECitySavePlatform::Desktop;
#endif
}

bool FCitySaveDocument::ValidateDimensions(FString& OutError) const
{
	const auto Dimensions = GetDimensions();
	if (!FCityPersistence::ValidatePlatform(Dimensions, ECitySavePlatform::Any, OutError))
		return false;
	if (SchemaVersion != CurrentSchemaVersion || GridRulesVersion != CurrentGridRulesVersion)
	{
		OutError = TEXT("Per-city dimensions require schema60/grid rules1; earlier cities must be64x64");
		return false;
	}
	if (Placement.Dimensions != Dimensions || Terrain.Dimensions != Dimensions ||
	    Environment.Dimensions != Dimensions || Crime.Dimensions != Dimensions ||
	    Fire.Dimensions != Dimensions || Waste.Dimensions != Dimensions ||
	    Disasters.Dimensions != Dimensions || Disasters.Events.ContainsByPredicate([&](const auto& Event) {
		    return Event.Dimensions != Dimensions;
	    }))
	{
		OutError = TEXT("Save authorities/events have inconsistent city dimensions");
		return false;
	}
	return true;
}

bool FCityPersistence::Serialize(const FCitySaveDocument& Document, FString& OutJson, FString& OutError)
{
	if (!Document.ValidateDimensions(OutError))
		return false;
	const auto Dimensions = Document.GetDimensions();
	if (!Document.Economy.Policies.Validate(Document.Economy.LastBilledMonthId, OutError))
		return false;
	for (const auto& C : Document.Population.Citizens)
		if (C.LastPolicyGrant < 0 || C.LastPolicyGrant > 7 || C.LastPolicyGrantMonth < 0 ||
		    C.LastPolicyGrantMonth > Document.Economy.Policies.LastSettledMonth ||
		    (C.LastPolicyGrantMonth == 0 && C.LastPolicyGrant))
		{
			OutError = TEXT("Invalid citizen policy grant");
			return false;
		}
	if (!Document.Economy.Loan.Validate(Document.Economy.LastBilledMonthId, OutError))
		return false;
	if ((!FCityHousingMigration::Validate(Document.Population, OutError) ||
	     Document.Population.LastMigrationMonth > Document.Economy.LastBilledMonthId))
	{
		if (OutError.IsEmpty())
			OutError = TEXT("Future housing migration clock");
		return false;
	}
	if (!FCityHousingHistory::Validate(Document.Population, OutError, &Dimensions,
	                                   Document.Placement.NextEntityId))
		return false;
	for (const auto& C : Document.Population.Citizens)
		if (!FCityHappinessEvaluator::ValidateCitizen(C, OutError) ||
		    C.LastHappinessMonth > Document.Economy.LastBilledMonthId)
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Future citizen happiness clock");
			return false;
		}
	if (!Document.Terrain.IsValid(&OutError))
		return false;

	if (!Document.Identity.Validate(OutError) ||
	    !FCityGameRevision::IsValid(Document.LastSavedGameRevision) ||
	    !FCityFirstPlayableGuide::ValidateProgress(Document.Guidance, OutError))
		return false;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	if (Document.DifficultyRules.NewArrivalUneducatedPercent < 0 ||
	    Document.DifficultyRules.NewArrivalUneducatedPercent > 100)
	{
		OutError = TEXT("Invalid newcomer education percentage");
		return false;
	}

	if (!FCityHealthSimulation::ValidateState(Document.Health, OutError))
		return false;
	if (Document.Health.LastStep > Document.Economy.FixedStepId)
	{
		OutError = TEXT("Health clock exceeds economy clock");
		return false;
	}
	for (const auto& P : Document.Health.Patients)
	{
		if (!Document.Population.Citizens.ContainsByPredicate(
		        [&](const auto& C) { return C.Id == P.CitizenId; }) ||
		    (P.HospitalId && !Document.Placement.Entities.ContainsByPredicate([&](const auto& E) {
			    return E.EntityId == P.HospitalId && E.DefinitionId == TEXT("Hospital");
		    })))
		{
			OutError = TEXT("Unknown health citizen or hospital");
			return false;
		}
	}

	if (!FCityWasteSimulation::ValidateState(Document.Waste, Document.Economy.FixedStepId, OutError))
		return false;

	Root->SetObjectField(TEXT("waste"), FCityWastePersistence::Write(Document.Waste));
	Root->SetNumberField(TEXT("schemaVersion"), Document.SchemaVersion);
	Root->SetNumberField(TEXT("ruleNewArrivalUneducatedPercent"),
	                     Document.DifficultyRules.NewArrivalUneducatedPercent);
	Root->SetNumberField(TEXT("disasterFrequency"), static_cast<int32>(Document.Identity.DisasterFrequency));
	{
		auto State = Document.Disasters;

		if (State.LastFixedStepId != Document.Economy.FixedStepId ||
		    !FCityDisasterSimulation::ValidateState(State, Document.Economy.FixedStepId, OutError))
			return false;
		auto O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("lastStep"), Int64String(State.LastFixedStepId));
		O->SetStringField(TEXT("nextCheck"), Int64String(State.NextCheckStep));
		O->SetStringField(TEXT("random"), UInt64String(State.RandomState));
		O->SetStringField(TEXT("total"), Int64String(State.TotalEvents));
		TArray<TSharedPtr<FJsonValue>> Events;
		for (const auto& E : State.Events)
		{

			auto V = MakeShared<FJsonObject>();
			{
				{
					V->SetNumberField(TEXT("radiusSquared"), E.RadiusSquared);
					TArray<TSharedPtr<FJsonValue>> Distances;
					for (double Distance : E.TravelDistances)
						Distances.Add(MakeShared<FJsonValueNumber>(Distance));
					V->SetArrayField(TEXT("travelDistances"), Distances);
				}
				V->SetNumberField(TEXT("profile"), E.Profile);
				V->SetNumberField(TEXT("severity"), E.SeverityPercent);
				V->SetNumberField(TEXT("initialBuildings"), E.InitialBuildings);
				TArray<TSharedPtr<FJsonValue>> Plan;
				for (int32 Index = 0; Index < E.Path.Num(); ++Index)
				{
					auto P = MakeShared<FJsonObject>();
					P->SetStringField(TEXT("id"), UInt64String(E.PlannedTargets[Index]));
					P->SetNumberField(TEXT("x"), E.Path[Index].X);
					P->SetNumberField(TEXT("y"), E.Path[Index].Y);
					Plan.Add(MakeShared<FJsonValueObject>(P));
				}
				V->SetArrayField(TEXT("plan"), Plan);
			}
			V->SetStringField(TEXT("id"), Int64String(E.Id));
			V->SetNumberField(TEXT("kind"), static_cast<int32>(E.Kind));
			V->SetNumberField(TEXT("x"), E.Center.X);
			V->SetNumberField(TEXT("y"), E.Center.Y);
			V->SetNumberField(TEXT("direction"), E.Direction);
			V->SetStringField(TEXT("warning"), Int64String(E.WarningStep));
			V->SetStringField(TEXT("start"), Int64String(E.StartStep));
			V->SetStringField(TEXT("end"), Int64String(E.EndStep));
			V->SetBoolField(TEXT("test"), E.bTest);
			TArray<TSharedPtr<FJsonValue>> Affected;
			for (const uint64 Id : E.Affected)
				Affected.Add(MakeShared<FJsonValueString>(UInt64String(Id)));
			V->SetArrayField(TEXT("affected"), Affected);
			Events.Add(MakeShared<FJsonValueObject>(V));
		}
		O->SetArrayField(TEXT("events"), Events);
		Root->SetObjectField(TEXT("disasters"), O);
	}
	{
		if (!Document.Reports.Validate(Document.Economy.LastBilledMonthId, OutError))
			return false;
		TArray<TSharedPtr<FJsonValue>> Samples;
		for (const auto& Sample : Document.Reports.Months)
		{
			auto Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("month"), Int64String(Sample.MonthId));
			Object->SetBoolField(TEXT("safetyRecorded"), Sample.bHasSafety);
			Object->SetBoolField(TEXT("healthRecorded"), Sample.bHasHealth);
			Object->SetBoolField(TEXT("wasteRecorded"), Sample.bHasWaste);
			Object->SetBoolField(TEXT("migrationRecorded"), Sample.bHasMigration);
			Object->SetBoolField(TEXT("policiesRecorded"), Sample.bHasPolicies);
			Object->SetBoolField(TEXT("loansRecorded"), Sample.bHasLoans);
			Object->SetBoolField(TEXT("sentimentRecorded"), Sample.bHasSentiment);
			for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
				Object->SetStringField(FCityReportHistory::Name(static_cast<ECityReportMetric>(I)),
				                       Int64String(Sample.Values[I]));
			Samples.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("reports"), Samples);
		Root->SetStringField(TEXT("exportsThisMonth"), Int64String(Document.Population.ExportsThisMonth));
		Root->SetStringField(TEXT("lastMonthExports"), Int64String(Document.Population.LastMonthExports));
	}
	{
		Root->SetStringField(TEXT("cityId"), Document.Identity.Id);
		Root->SetStringField(TEXT("cityName"), Document.Identity.Name);
		Root->SetStringField(TEXT("createdGameRevision"), Document.Identity.CreatedGameRevision);
		Root->SetStringField(TEXT("lastSavedGameRevision"), Document.LastSavedGameRevision);
		Root->SetNumberField(TEXT("difficulty"), static_cast<int32>(Document.Identity.Difficulty));
		Root->SetNumberField(TEXT("rulesVersion"), Document.Identity.RulesVersion);
		Root->SetNumberField(TEXT("ruleCrimePopulationThreshold"),
		                     Document.DifficultyRules.CrimePopulationThreshold);
		Root->SetNumberField(TEXT("ruleFirePopulationThreshold"),
		                     Document.DifficultyRules.FirePopulationThreshold);
		Root->SetNumberField(TEXT("ruleCrimeGraceMonths"), Document.DifficultyRules.CrimeGraceMonths);
		Root->SetNumberField(TEXT("ruleFireGraceMonths"), Document.DifficultyRules.FireGraceMonths);
		Root->SetNumberField(TEXT("ruleCrimeCheckIntervalSteps"),
		                     Document.DifficultyRules.CrimeCheckIntervalSteps);
		Root->SetNumberField(TEXT("ruleFireCheckIntervalSteps"),
		                     Document.DifficultyRules.FireCheckIntervalSteps);
		Root->SetNumberField(TEXT("ruleFireRepairCostPercent"),
		                     Document.DifficultyRules.FireRepairCostPercent);
		Root->SetNumberField(TEXT("ruleFireRepairSteps"), Document.DifficultyRules.FireRepairSteps);
	}
	Root->SetNumberField(TEXT("gridWidth"), Document.GridWidth);
	Root->SetNumberField(TEXT("gridHeight"), Document.GridHeight);
	Root->SetNumberField(TEXT("gridRulesVersion"), Document.GridRulesVersion);
	Root->SetStringField(TEXT("funds"), Int64String(Document.Placement.Funds));
	Root->SetStringField(TEXT("revision"), UInt64String(Document.Placement.Revision));
	Root->SetStringField(TEXT("nextEntityId"), UInt64String(Document.Placement.NextEntityId));

	TArray<TSharedPtr<FJsonValue>> Zones;
	Zones.Reserve(Document.Placement.Zones.Num());
	for (const ECityZoneType Zone : Document.Placement.Zones)
	{
		Zones.Add(MakeShared<FJsonValueNumber>(static_cast<int32>(Zone)));
	}
	Root->SetArrayField(TEXT("zones"), Zones);

	TArray<TSharedPtr<FJsonValue>> Roads;
	for (const FCityCellCoord& Cell : Document.Placement.RoadCells)
	{
		TSharedRef<FJsonObject> Object = WriteCell(Cell, true);
		const FCityRoadCellRecord* Record = Document.Placement.RoadRecords.FindByPredicate(
		    [&Cell](const FCityRoadCellRecord& Candidate) { return Candidate.Cell == Cell; });
		Object->SetStringField(TEXT("definitionId"),
		                       Record ? Record->DefinitionId.ToString() : TEXT("LocalRoad"));
		Roads.Add(MakeShared<FJsonValueObject>(Object));
	}

	Root->SetArrayField(TEXT("roads"), Roads);
	{
		TArray<TSharedPtr<FJsonValue>> Spans;
		for (const auto& S : Document.Placement.RoadStructures)
		{
			auto O = MakeShared<FJsonObject>();
			O->SetObjectField(TEXT("start"), WriteCell(S.Start, true));
			O->SetObjectField(TEXT("end"), WriteCell(S.End, true));
			O->SetStringField(TEXT("definitionId"), S.DefinitionId.ToString());
			O->SetNumberField(TEXT("profile"), static_cast<int32>(S.Profile));
			O->SetNumberField(TEXT("elevationStep"), S.ElevationStep);
			O->SetNumberField(TEXT("deck"), S.DeckHeightCm);
			O->SetNumberField(TEXT("startHeight"), S.StartHeightCm);
			O->SetNumberField(TEXT("endHeight"), S.EndHeightCm);
			O->SetNumberField(TEXT("startRamp"), S.StartRampCells);
			O->SetNumberField(TEXT("endRamp"), S.EndRampCells);
			O->SetStringField(TEXT("cost"), Int64String(S.RecordedCost));
			Spans.Add(MakeShared<FJsonValueObject>(O));
		}
		Root->SetArrayField(TEXT("roadStructures"), Spans);
	}

	TArray<TSharedPtr<FJsonValue>> Buildings;
	for (const FCityPlacedEntity& Entity : Document.Placement.Entities)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("entityId"), UInt64String(Entity.EntityId));
		Object->SetStringField(TEXT("definitionId"), Entity.DefinitionId.ToString());
		Object->SetNumberField(TEXT("x"), Entity.Anchor.X);
		Object->SetNumberField(TEXT("y"), Entity.Anchor.Y);
		Object->SetNumberField(TEXT("quarterTurns"), Entity.QuarterTurnsClockwise);
		Object->SetNumberField(TEXT("width"), Entity.FootprintSize.X);
		Object->SetNumberField(TEXT("height"), Entity.FootprintSize.Y);
		Object->SetStringField(TEXT("recordedCost"), Int64String(Entity.RecordedPlacementCost));
		Buildings.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("buildings"), Buildings);
	Root->SetObjectField(TEXT("economy"), WriteEconomy(Document));

	TSharedRef<FJsonObject> Growth = MakeShared<FJsonObject>();
	Growth->SetStringField(TEXT("randomState"), UInt64String(Document.GrowthRandomState));
	Growth->SetStringField(TEXT("nextGrowthStep"), Int64String(Document.NextGrowthStep));
	Root->SetObjectField(TEXT("growth"), Growth);

	TSharedRef<FJsonObject> Scenario = MakeShared<FJsonObject>();
	Scenario->SetNumberField(TEXT("state"), Document.Scenario.State);
	Scenario->SetNumberField(TEXT("profitableMonthStreak"), Document.Scenario.ProfitableMonthStreak);
	Scenario->SetBoolField(TEXT("complete"), Document.Scenario.bComplete);
	Scenario->SetStringField(TEXT("lastObservedBilledMonthId"),
	                         Int64String(Document.Scenario.LastObservedBilledMonthId));
	Root->SetObjectField(TEXT("scenario"), Scenario);
	{
		FCityFireSnapshot State = Document.Fire;

		if (!FCityFireSimulation::ValidateState(State, Document.Economy.FixedStepId, OutError))
			return false;
		TSharedRef<FJsonObject> Fire = MakeShared<FJsonObject>();
		Fire->SetStringField(TEXT("lastFixedStepId"), Int64String(State.LastFixedStepId));
		Fire->SetStringField(TEXT("activationStep"), Int64String(State.ActivationStep));
		Fire->SetStringField(TEXT("graceEndStep"), Int64String(State.GraceEndStep));
		Fire->SetStringField(TEXT("nextIgnitionStep"), Int64String(State.NextIgnitionStep));
		Fire->SetStringField(TEXT("totalIncidents"), Int64String(State.TotalIncidents));
		Fire->SetStringField(TEXT("preventedIncidents"), Int64String(State.PreventedIncidents));
		Fire->SetStringField(TEXT("randomState"), UInt64String(State.RandomState));
		TArray<TSharedPtr<FJsonValue>> Incidents;
		for (const auto& Incident : State.RecentIncidents)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), Int64String(Incident.Id));
			Object->SetStringField(TEXT("startStep"), Int64String(Incident.StartStep));
			Object->SetStringField(TEXT("resolvedStep"), Int64String(Incident.ResolvedStep));
			Object->SetStringField(TEXT("entityId"), UInt64String(Incident.EntityId));
			Object->SetStringField(TEXT("spreadFromEntityId"), UInt64String(Incident.SpreadFromEntityId));
			{
				Object->SetBoolField(TEXT("arson"), Incident.bArson);
				Object->SetStringField(TEXT("offender"), UInt64String(Incident.OffenderCitizenId));
			}
			Object->SetNumberField(TEXT("responseSteps"), Incident.ResponseSteps);
			Object->SetBoolField(TEXT("test"), Incident.bTest);
			Incidents.Add(MakeShared<FJsonValueObject>(Object));
		}
		Fire->SetArrayField(TEXT("incidents"), Incidents);
		TArray<TSharedPtr<FJsonValue>> Damage;
		TArray<uint64> Ids;
		State.DamagedBuildings.GetKeys(Ids);
		Ids.Sort();
		for (const uint64 Id : Ids)
		{
			const auto& Item = State.DamagedBuildings.FindChecked(Id);
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("entityId"), UInt64String(Id));
			Object->SetNumberField(TEXT("damage"), Item.Damage);
			Object->SetNumberField(TEXT("cause"), Item.Cause);
			Object->SetStringField(TEXT("repairCompleteStep"), Int64String(Item.RepairCompleteStep));
			Damage.Add(MakeShared<FJsonValueObject>(Object));
		}
		Fire->SetArrayField(TEXT("damage"), Damage);
		Root->SetObjectField(TEXT("fire"), Fire);
	}

	{
		FCityCrimeSnapshot State = Document.Crime;

		if (!FCityCrimeSimulation::ValidateState(State, Document.Economy.FixedStepId, OutError))
			return false;
		TSharedRef<FJsonObject> Crime = MakeShared<FJsonObject>();
		Crime->SetStringField(TEXT("lastFixedStepId"), Int64String(State.LastFixedStepId));
		Crime->SetStringField(TEXT("activationStep"), Int64String(State.ActivationStep));
		Crime->SetStringField(TEXT("graceEndStep"), Int64String(State.GraceEndStep));
		Crime->SetStringField(TEXT("randomState"), UInt64String(State.RandomState));
		Crime->SetNumberField(TEXT("previousHousedResidents"), State.PreviousHousedResidents);
		Crime->SetNumberField(TEXT("homelessResidents"), State.HomelessResidents);
		Crime->SetStringField(TEXT("totalIncidents"), Int64String(State.TotalIncidents));
		Crime->SetStringField(TEXT("preventedIncidents"), Int64String(State.PreventedIncidents));
		{
			Crime->SetStringField(TEXT("lastArsonStep"), Int64String(State.LastArsonStep));
			Crime->SetStringField(TEXT("totalArsons"), Int64String(State.TotalArsons));
			Crime->SetStringField(TEXT("totalArrests"), Int64String(State.TotalArrests));
		}
		TArray<TSharedPtr<FJsonValue>> Incidents;
		for (const auto& Incident : State.RecentIncidents)
		{
			TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
			Record->SetStringField(TEXT("id"), Int64String(Incident.Id));
			Record->SetStringField(TEXT("entityId"), UInt64String(Incident.EntityId));
			Record->SetStringField(TEXT("startStep"), Int64String(Incident.StartStep));
			Record->SetStringField(TEXT("endStep"), Int64String(Incident.EndStep));
			{
				Record->SetBoolField(TEXT("arson"), Incident.bArson);
				Record->SetStringField(TEXT("offender"), UInt64String(Incident.OffenderCitizenId));
				Record->SetStringField(TEXT("arrestStation"), UInt64String(Incident.ArrestStationId));
				Record->SetStringField(TEXT("captureStep"), Int64String(Incident.CaptureStep));
				Record->SetNumberField(TEXT("arsonOutcome"), static_cast<int32>(Incident.ArsonOutcome));
			}
			Record->SetBoolField(TEXT("test"), Incident.bTest);
			Incidents.Add(MakeShared<FJsonValueObject>(Record));
		}
		Crime->SetArrayField(TEXT("incidents"), Incidents);
		Root->SetObjectField(TEXT("crime"), Crime);
	}

	{
		TSharedRef<FJsonObject> Guidance = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Completed;
		for (const FName Id : Document.Guidance.CompletedIds)
			Completed.Add(MakeShared<FJsonValueString>(Id.ToString()));
		Guidance->SetArrayField(TEXT("completedIds"), Completed);
		Guidance->SetStringField(TEXT("currentId"), Document.Guidance.CurrentId.ToString());
		Root->SetObjectField(TEXT("guidance"), Guidance);
	}

	TSharedRef<FJsonObject> Terrain = MakeShared<FJsonObject>();
	Terrain->SetNumberField(TEXT("seed"), Document.Terrain.Seed);
	Terrain->SetNumberField(TEXT("generatorVersion"), Document.Terrain.GeneratorVersion);
	{
		Terrain->SetNumberField(TEXT("waterRulesVersion"), Document.Terrain.WaterRulesVersion);
		Terrain->SetNumberField(TEXT("sceneryVersion"), Document.Terrain.SceneryVersion);
	}
	Terrain->SetNumberField(TEXT("broadElevationAmplitudeCm"), Document.Terrain.BroadElevationAmplitudeCm);
	Terrain->SetNumberField(TEXT("hillAmplitudeCm"), Document.Terrain.HillAmplitudeCm);
	Terrain->SetNumberField(TEXT("ridgeAmplitudeCm"), Document.Terrain.RidgeAmplitudeCm);
	Terrain->SetNumberField(TEXT("smoothingPasses"), Document.Terrain.SmoothingPasses);
	Terrain->SetNumberField(TEXT("quantizationCm"), Document.Terrain.QuantizationCm);
	Terrain->SetNumberField(TEXT("seaLevelCm"), Document.Terrain.SeaLevelCm);
	Terrain->SetNumberField(TEXT("starterHalfWidthCells"), Document.Terrain.StarterHalfWidthCells);
	Terrain->SetNumberField(TEXT("starterDepthCells"), Document.Terrain.StarterDepthCells);
	Root->SetObjectField(TEXT("terrain"), Terrain);

	TSharedRef<FJsonObject> Environment = MakeShared<FJsonObject>();
	Environment->SetStringField(TEXT("lastFixedStepId"), Int64String(Document.Environment.LastFixedStepId));
	Environment->SetNumberField(TEXT("totalPollution"), Document.Environment.TotalPollution);
	Environment->SetNumberField(TEXT("peakPollution"), Document.Environment.PeakPollution);
	Environment->SetStringField(TEXT("weatherRandomState"),
	                            UInt64String(Document.Environment.WeatherRandomState));
	Environment->SetStringField(TEXT("nextWeatherChangeStep"),
	                            Int64String(Document.Environment.NextWeatherChangeStep));
	Environment->SetNumberField(TEXT("season"), static_cast<int32>(Document.Environment.Season));
	Environment->SetNumberField(TEXT("temperature"), static_cast<int32>(Document.Environment.Temperature));
	Environment->SetNumberField(TEXT("windDirection"),
	                            static_cast<int32>(Document.Environment.WindDirection));
	Environment->SetNumberField(TEXT("windStrength"), Document.Environment.WindStrength);
	Environment->SetBoolField(TEXT("precipitation"), Document.Environment.bPrecipitation);
	TArray<TSharedPtr<FJsonValue>> Pollution;
	Pollution.Reserve(Document.Environment.Pollution.Num());
	for (const int32 Value : Document.Environment.Pollution)
	{
		Pollution.Add(MakeShared<FJsonValueNumber>(Value));
	}
	Environment->SetArrayField(TEXT("pollution"), Pollution);
	Root->SetObjectField(TEXT("environment"), Environment);

	TSharedRef<FJsonObject> Development = MakeShared<FJsonObject>();
	Development->SetStringField(TEXT("lastFixedStepId"), Int64String(Document.Development.LastFixedStepId));
	TArray<TSharedPtr<FJsonValue>> DevelopmentStates;
	for (const TPair<uint64, FCityDevelopmentState>& Pair : Document.Development.States)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("entityId"), UInt64String(Pair.Key));
		Object->SetNumberField(TEXT("stage"), static_cast<int32>(Pair.Value.Stage));
		Object->SetNumberField(TEXT("density"), static_cast<int32>(Pair.Value.DensityLevel));
		Object->SetStringField(TEXT("constructionCompleteStep"),
		                       Int64String(Pair.Value.ConstructionCompleteStep));
		Object->SetStringField(TEXT("operationalSinceStep"), Int64String(Pair.Value.OperationalSinceStep));
		Object->SetNumberField(TEXT("pendingCivicLevel"), Pair.Value.PendingCivicLevel);
		Object->SetStringField(TEXT("civicUpgradeCompleteStep"),
		                       Int64String(Pair.Value.CivicUpgradeCompleteStep));
		Object->SetStringField(TEXT("stationCompleteHalfDay"),
		                       Int64String(Pair.Value.StationCompleteHalfDay));
		DevelopmentStates.Add(MakeShared<FJsonValueObject>(Object));
	}
	Development->SetArrayField(TEXT("states"), DevelopmentStates);
	Root->SetObjectField(TEXT("development"), Development);

	TSharedRef<FJsonObject> Population = MakeShared<FJsonObject>();
	Population->SetBoolField(TEXT("commercialAiPilot"), Document.Population.bCommercialAiPilot);
	Population->SetStringField(TEXT("nextCitizenId"), UInt64String(Document.Population.NextCitizenId));
	Population->SetStringField(TEXT("nextTransitTripId"),
	                           UInt64String(Document.Population.NextTransitTripId));
	Population->SetStringField(TEXT("lastSettledMonthId"),
	                           Int64String(Document.Population.LastSettledMonthId));
	Population->SetStringField(TEXT("discardedGoods"), Int64String(Document.Population.DiscardedGoods));
	{
		Population->SetStringField(TEXT("lastMigrationMonth"),
		                           Int64String(Document.Population.LastMigrationMonth));
		Population->SetStringField(TEXT("immigrationResumeMonth"),
		                           Int64String(Document.Population.ImmigrationResumeMonth));
		Population->SetStringField(TEXT("relocatedResidents"),
		                           Int64String(Document.Population.RelocatedResidents));
		Population->SetStringField(TEXT("departedResidents"),
		                           Int64String(Document.Population.DepartedResidents));
		Population->SetStringField(TEXT("movingExpenses"), Int64String(Document.Population.MovingExpenses));
		Population->SetStringField(TEXT("departureMoney"), Int64String(Document.Population.DepartureMoney));
		TArray<TSharedPtr<FJsonValue>> Holds;
		for (const auto& H : Document.Population.HousingVacancyHolds)
		{
			auto Hold = MakeShared<FJsonObject>();
			Hold->SetStringField(TEXT("home"), UInt64String(H.HomeEntityId));
			Hold->SetStringField(TEXT("untilMonth"), Int64String(H.UntilMonth));
			Holds.Add(MakeShared<FJsonValueObject>(Hold));
		}
		Population->SetArrayField(TEXT("housingVacancyHolds"), Holds);
	}
	Population->SetBoolField(TEXT("arrivalPurchases"), Document.Population.bUsesPurchaseReservations);
	{
		Population->SetStringField(TEXT("nextHousingEventId"),
		                           UInt64String(Document.Population.NextHousingEventId));
		TArray<TSharedPtr<FJsonValue>> Events;
		for (const auto& E : Document.Population.HousingEvents)
		{
			auto O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), UInt64String(E.Id));
			O->SetStringField(TEXT("month"), Int64String(E.Month));
			O->SetStringField(TEXT("sourceHome"), UInt64String(E.SourceHome));
			O->SetStringField(TEXT("destinationHome"), UInt64String(E.DestinationHome));
			O->SetObjectField(TEXT("sourceCell"), WriteCell(E.SourceCell));
			O->SetObjectField(TEXT("destinationCell"), WriteCell(E.DestinationCell));
			O->SetNumberField(TEXT("kind"), static_cast<uint8>(E.Kind));
			O->SetNumberField(TEXT("housingStatus"), E.HousingStatus);
			O->SetNumberField(TEXT("residents"), E.Residents);
			O->SetNumberField(TEXT("happiness"), E.Happiness);
			O->SetNumberField(TEXT("outlook"), E.Outlook);
			O->SetNumberField(TEXT("destinationOutlook"), E.DestinationOutlook);
			O->SetNumberField(TEXT("concerns"), E.Concerns);
			O->SetStringField(TEXT("money"), Int64String(E.Money));
			TArray<TSharedPtr<FJsonValue>> Ids;
			for (uint64 Id : E.CitizenIds)
				Ids.Add(MakeShared<FJsonValueString>(UInt64String(Id)));
			O->SetArrayField(TEXT("citizens"), Ids);
			Events.Add(MakeShared<FJsonValueObject>(O));
		}
		Population->SetArrayField(TEXT("housingEvents"), Events);
	}
	Population->SetNumberField(TEXT("salesThisMonth"), Document.Population.SalesThisMonth);
	Population->SetNumberField(TEXT("lastMonthSales"), Document.Population.LastMonthSales);
	Population->SetStringField(TEXT("spendingThisMonth"), Int64String(Document.Population.SpendingThisMonth));
	Population->SetStringField(TEXT("lastMonthSpending"), Int64String(Document.Population.LastMonthSpending));
	Population->SetStringField(TEXT("totalArrivalPurchases"),
	                           Int64String(Document.Population.TotalArrivalPurchases));
	TArray<TSharedPtr<FJsonValue>> Reservations;
	for (const auto& Order : Document.Population.PurchaseReservations)
	{
		auto Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("buyer"), UInt64String(Order.BuyerId));
		Object->SetStringField(TEXT("household"), UInt64String(Order.HouseholdId));
		Object->SetStringField(TEXT("shop"), UInt64String(Order.ShopId));
		Object->SetStringField(TEXT("price"), Int64String(Order.Price));
		Object->SetNumberField(TEXT("quantity"), Order.Quantity);
		Object->SetNumberField(TEXT("comfortTier"), Order.ComfortTier);
		TArray<TSharedPtr<FJsonValue>> Contributions;
		for (const auto& Contribution : Order.Contributions)
		{
			auto Funding = MakeShared<FJsonObject>();
			Funding->SetStringField(TEXT("citizen"), UInt64String(Contribution.CitizenId));
			Funding->SetStringField(TEXT("amount"), Int64String(Contribution.Amount));
			Contributions.Add(MakeShared<FJsonValueObject>(Funding));
		}
		Object->SetArrayField(TEXT("contributions"), Contributions);
		Reservations.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("purchaseReservations"), Reservations);
	TArray<TSharedPtr<FJsonValue>> Citizens;
	for (const FCityCitizenRecord& Record : Document.Population.Citizens)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), UInt64String(Record.Id));
		Object->SetStringField(TEXT("householdId"), UInt64String(Record.HouseholdId));
		Object->SetStringField(TEXT("homeEntityId"), UInt64String(Record.HomeEntityId));
		Object->SetNumberField(TEXT("education"), static_cast<uint8>(Record.Education));
		Object->SetStringField(TEXT("unemployedSinceStep"), Int64String(Record.UnemployedSinceStep));
		{
			Object->SetStringField(TEXT("jailStation"), UInt64String(Record.JailStationId));
			Object->SetStringField(TEXT("jailReleaseStep"), Int64String(Record.JailReleaseStep));
			Object->SetStringField(TEXT("reformedUntilStep"), Int64String(Record.ReformedUntilStep));
		}
		Object->SetNumberField(TEXT("educationMonths"), Record.EducationMonths);
		Object->SetStringField(TEXT("lastEducationMonth"), Int64String(Record.LastEducationMonth));
		{
			Object->SetNumberField(TEXT("happiness"), Record.Happiness);
			Object->SetNumberField(TEXT("happinessTarget"), Record.HappinessTarget);
			Object->SetNumberField(TEXT("happinessConcerns"), Record.HappinessConcerns);
			Object->SetNumberField(TEXT("discontentMonths"), Record.DiscontentMonths);
			Object->SetStringField(TEXT("lastHappinessMonth"), Int64String(Record.LastHappinessMonth));
		}
		{
			Object->SetStringField(TEXT("lastRelocationMonth"), Int64String(Record.LastRelocationMonth));
			Object->SetNumberField(TEXT("housingStatus"), static_cast<uint8>(Record.HousingStatus));
		}
		Object->SetBoolField(TEXT("skilledJob"), Record.bSkilledJob);
		Object->SetStringField(TEXT("workplaceEntityId"), UInt64String(Record.WorkplaceEntityId));
		{
			Object->SetStringField(TEXT("policyGrant"), Int64String(Record.LastPolicyGrant));
			Object->SetStringField(TEXT("policyGrantMonth"), Int64String(Record.LastPolicyGrantMonth));
		}
		Object->SetStringField(TEXT("money"), Int64String(Record.AvailableMoney));
		Object->SetNumberField(TEXT("need"), Record.ConsumptionNeed);
		Object->SetNumberField(TEXT("activity"), static_cast<int32>(Record.Activity));
		Object->SetNumberField(TEXT("trip"), static_cast<int32>(Record.TripPurpose));
		Object->SetNumberField(TEXT("vehicle"), static_cast<int32>(Record.TripVehicleType));
		Object->SetStringField(TEXT("destination"), UInt64String(Record.TripDestinationEntityId));
		Object->SetStringField(TEXT("nextAction"), Int64String(Record.NextActionStep));
		Object->SetStringField(TEXT("currentLocation"), UInt64String(Record.CurrentLocationEntityId));
		Object->SetStringField(TEXT("pendingShop"), UInt64String(Record.PendingShopEntityId));
		Object->SetNumberField(TEXT("schedule"), static_cast<int32>(Record.Schedule));
		Object->SetBoolField(TEXT("shopBeforeWork"), Record.bShopBeforeWork);
		Citizens.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("citizens"), Citizens);
	TArray<TSharedPtr<FJsonValue>> Households;
	for (const FCityHouseholdRecord& Record : Document.Population.Households)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), UInt64String(Record.Id));
		Object->SetStringField(TEXT("homeEntityId"), UInt64String(Record.HomeEntityId));
		Object->SetStringField(TEXT("balance"), Int64String(Record.Balance));
		Object->SetNumberField(TEXT("orders"), Record.ConsumptionOrders);
		Object->SetNumberField(TEXT("purchases"), Record.CompletedPurchases);
		Object->SetNumberField(TEXT("purchasesThisMonth"), Record.PurchasesThisMonth);
		Object->SetBoolField(TEXT("storedSupplies"), Record.bUsesStoredSupplies);
		Object->SetNumberField(TEXT("supplyUnits"), Record.SupplyUnits);
		Object->SetStringField(TEXT("supplyDay"), Int64String(Record.SupplyDay));
		Object->SetStringField(TEXT("lastEssentialDay"), Int64String(Record.LastEssentialPurchaseDay));
		Object->SetStringField(TEXT("lastComfortDay"), Int64String(Record.LastComfortDay));
		Object->SetNumberField(TEXT("comfortTier"), Record.ComfortTier);
		Households.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("households"), Households);
	TArray<TSharedPtr<FJsonValue>> Businesses;
	for (const FCityBusinessLedgerRecord& Record : Document.Population.Businesses)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("entityId"), UInt64String(Record.EntityId));
		Object->SetBoolField(TEXT("industrial"), Record.bIndustrial);
		Object->SetNumberField(TEXT("skilledJobs"), Record.SkilledJobCapacity);
		Object->SetNumberField(TEXT("skilledMinimumEducation"), Record.SkilledMinimumEducation);
		Object->SetStringField(TEXT("aiFeePaid"), Int64String(Record.AiFeePaid));
		Object->SetStringField(TEXT("aiPaidMonth"), Int64String(Record.AiPaidMonth));
        Object->SetStringField(TEXT("aiCenterAtPayment"), UInt64String(Record.AiCenterAtPayment));
        Object->SetNumberField(TEXT("aiJobsAtPayment"), Record.AiJobsAtPayment);
        Object->SetNumberField(TEXT("aiSpecialistsAtPayment"), Record.AiSpecialistsAtPayment);
		Object->SetNumberField(TEXT("jobs"), Record.JobCapacity);
		Object->SetNumberField(TEXT("inventory"), Record.Inventory);
		Object->SetNumberField(TEXT("orders"), Record.PendingOrders);
		Object->SetNumberField(TEXT("sales"), Record.CompletedSales);
		Object->SetNumberField(TEXT("salesThisMonth"), Record.SalesThisMonth);
		Object->SetStringField(TEXT("balance"), Int64String(Record.Balance));
		Object->SetStringField(TEXT("openingPayrollReserve"), Int64String(Record.OpeningPayrollReserve));
		Object->SetBoolField(TEXT("initialFreightReceived"), Record.bInitialFreightReceived);
		Object->SetNumberField(TEXT("retailTier"), Record.RetailTier);
		Object->SetBoolField(TEXT("receivedGoods"), Record.bHasReceivedGoods);
		Object->SetStringField(TEXT("openingGraceUntil"), Int64String(Record.OpeningGraceUntilStep));
		Object->SetStringField(TEXT("exportedGoods"), Int64String(Record.ExportedGoods));
		Object->SetNumberField(TEXT("producedGoods"), Record.ProducedGoods);
		Object->SetStringField(TEXT("payrollDue"), Int64String(Record.PayrollDue));
		Object->SetStringField(TEXT("payrollPaid"), Int64String(Record.PayrollPaid));
		Businesses.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("businesses"), Businesses);
	TArray<TSharedPtr<FJsonValue>> PublicEmployers;
	for (const auto& Employer : Document.Population.PublicEmployers)
	{
		auto Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("entityId"), UInt64String(Employer.EntityId));
		Object->SetNumberField(TEXT("jobs"), Employer.JobCapacity);
		Object->SetStringField(TEXT("payrollDue"), Int64String(Employer.PayrollDue));
		Object->SetStringField(TEXT("payrollPaid"), Int64String(Employer.PayrollPaid));
		PublicEmployers.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("publicEmployers"), PublicEmployers);
	TArray<TSharedPtr<FJsonValue>> TransitTrips;
	for (const FCityTransitTripRecord& Record : Document.Population.TransitTrips)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), UInt64String(Record.Id));
		Object->SetStringField(TEXT("origin"), UInt64String(Record.OriginEntityId));
		Object->SetStringField(TEXT("destination"), UInt64String(Record.DestinationEntityId));
		Object->SetNumberField(TEXT("purpose"), static_cast<int32>(Record.Purpose));
		Object->SetNumberField(TEXT("vehicle"), static_cast<int32>(Record.VehicleType));
		Object->SetNumberField(TEXT("quantity"), Record.Quantity);
		Object->SetStringField(TEXT("start"), Int64String(Record.StartStep));
		Object->SetStringField(TEXT("arrival"), Int64String(Record.ArrivalStep));
		Object->SetBoolField(TEXT("completed"), Record.bCompleted);
		Object->SetStringField(TEXT("station"), UInt64String(Record.StationEntityId));
		Object->SetStringField(TEXT("incident"), Int64String(Record.IncidentId));
		Object->SetStringField(TEXT("patientCitizen"), UInt64String(Record.PatientCitizenId));
		TransitTrips.Add(MakeShared<FJsonValueObject>(Object));
	}
	Population->SetArrayField(TEXT("transitTrips"), TransitTrips);
	Root->SetObjectField(TEXT("population"), Population);
	TSharedRef<FJsonObject> Traffic = MakeShared<FJsonObject>();
	Traffic->SetStringField(TEXT("fixedStepId"), Int64String(Document.Traffic.FixedStepId));
	Traffic->SetStringField(TEXT("networkFingerprint"), UInt64String(Document.Traffic.NetworkFingerprint));
	auto WriteVehicles = [](const TArray<FCityTrafficVehicleState>& Records, const bool bWaiting) {
		TArray<TSharedPtr<FJsonValue>> Vehicles;
		for (const FCityTrafficVehicleState& Vehicle : Records)
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("id"), UInt64String(Vehicle.Id));
			if (bWaiting)
				Object->SetStringField(TEXT("requestedStep"), Int64String(Vehicle.DepartureRequestedStep));
			Object->SetNumberField(TEXT("routeLegIndex"), Vehicle.RouteLegIndex);
			Object->SetNumberField(TEXT("distanceUnits"), Vehicle.DistanceUnits);
			Object->SetNumberField(TEXT("speedUnitsPerStep"), Vehicle.SpeedUnitsPerStep);
			Object->SetNumberField(TEXT("lengthUnits"), Vehicle.LengthUnits);
			Object->SetBoolField(TEXT("completed"), Vehicle.bCompleted);
			Object->SetNumberField(TEXT("startDistance"), Vehicle.StartDistanceUnits);
			Object->SetNumberField(TEXT("endDistance"), Vehicle.EndDistanceUnits);
			Object->SetNumberField(TEXT("vehicleType"), static_cast<int32>(Vehicle.VehicleType));
			Object->SetStringField(TEXT("sourceId"), UInt64String(Vehicle.SourceId));
			Object->SetStringField(TEXT("originEntityId"), UInt64String(Vehicle.OriginEntityId));
			Object->SetStringField(TEXT("destinationEntityId"), UInt64String(Vehicle.DestinationEntityId));
			Object->SetStringField(TEXT("completedStep"), Int64String(Vehicle.CompletedStep));
			Object->SetBoolField(TEXT("transitTrip"), Vehicle.bTransitTrip);
			Object->SetBoolField(TEXT("returning"), Vehicle.bReturning);
			{
				Object->SetBoolField(TEXT("responding"), Vehicle.bEmergencyResponding);
				Object->SetBoolField(TEXT("yielding"), Vehicle.bYielding);
				Object->SetNumberField(TEXT("pedestrianTrack"), Vehicle.PedestrianTrack);
			}
			TArray<TSharedPtr<FJsonValue>> Legs;
			for (const FCityTrafficRouteLeg& Leg : Vehicle.Route)
			{
				TSharedRef<FJsonObject> LegObject = MakeShared<FJsonObject>();
				LegObject->SetNumberField(TEXT("edgeId"), Leg.EdgeId);
				LegObject->SetBoolField(TEXT("reverse"), Leg.bReverse);
				LegObject->SetNumberField(TEXT("laneIndex"), Leg.LaneIndex);
				Legs.Add(MakeShared<FJsonValueObject>(LegObject));
			}
			Object->SetArrayField(TEXT("route"), Legs);
			Vehicles.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Vehicles;
	};
	Traffic->SetArrayField(TEXT("vehicles"), WriteVehicles(Document.Traffic.Vehicles, false));
	Traffic->SetArrayField(TEXT("waitingDepartures"), WriteVehicles(Document.Traffic.WaitingDepartures, true));
	Root->SetObjectField(TEXT("traffic"), Traffic);

	{
		auto H = MakeShared<FJsonObject>();
		H->SetStringField(TEXT("step"), Int64String(Document.Health.LastStep));
		H->SetStringField(TEXT("introduced"), Int64String(Document.Health.IntroductionStep));
		H->SetStringField(TEXT("checked"), Int64String(Document.Health.LastCheckStep));
		H->SetStringField(TEXT("recovered"), Int64String(Document.Health.TotalRecovered));
		TArray<TSharedPtr<FJsonValue>> Patients;
		for (const auto& P : Document.Health.Patients)
		{
			auto V = MakeShared<FJsonObject>();
			V->SetStringField(TEXT("citizen"), UInt64String(P.CitizenId));
			V->SetStringField(TEXT("hospital"), UInt64String(P.HospitalId));
			V->SetNumberField(TEXT("illness"), static_cast<uint8>(P.Illness));
			V->SetNumberField(TEXT("care"), static_cast<uint8>(P.Care));
			V->SetNumberField(TEXT("risk"), P.ExposureRisk);
			V->SetStringField(TEXT("onset"), Int64String(P.OnsetStep));
			V->SetStringField(TEXT("treatment"), Int64String(P.TreatmentSteps));
			V->SetStringField(TEXT("treatedAt"), Int64String(P.LastTreatmentStep));
			Patients.Add(MakeShared<FJsonValueObject>(V));
		}
		H->SetArrayField(TEXT("patients"), Patients);
		Root->SetObjectField(TEXT("health"), H);
	}
	OutJson.Reset();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("Could not serialize city save JSON");
		return false;
	}
	return true;
}

bool FCityPersistence::Deserialize(const FString& Json, FCitySaveDocument& OutDocument, FString& OutError,
                                   const ECitySavePlatform Platform, ECitySaveReadFailure* OutFailure)
{
	if (OutFailure)
		*OutFailure = ECitySaveReadFailure::InvalidData;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Save is malformed or truncated JSON");
		return false;
	}
	FCitySaveDocument Document;
	if (!ReadInt32(Root, TEXT("schemaVersion"), Document.SchemaVersion, OutError))
		return false;
	const int32 SourceSchema = Document.SchemaVersion;
	if (SourceSchema != FCitySaveDocument::CurrentSchemaVersion && SourceSchema != FCitySaveDocument::PrePantrySchemaVersion && SourceSchema != FCitySaveDocument::PreOutpatientSchemaVersion && SourceSchema != FCitySaveDocument::PreAiSchemaVersion)
	{
		if (OutFailure)
			*OutFailure = ECitySaveReadFailure::UnsupportedSchema;
		OutError = FString::Printf(TEXT("Incompatible save schema %d; expected %d"), Document.SchemaVersion,
		                           FCitySaveDocument::CurrentSchemaVersion);
		return false;
	}
	Document.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	// Reject unsupported formats before allocating dimension-driven state.
	int32 Width = 0, Height = 0, GridRules = FCitySaveDocument::CurrentGridRulesVersion;
	if (!ReadInt32(Root, TEXT("gridWidth"), Width, OutError) ||
	    !ReadInt32(Root, TEXT("gridHeight"), Height, OutError))
		return false;
	const FCityGridDimensions Dimensions(Width, Height);
	if (!ValidatePlatform(Dimensions, ECitySavePlatform::Any, OutError))
	{
		if (OutFailure)
			*OutFailure = ECitySaveReadFailure::UnsupportedGrid;
		return false;
	}
	if (!ReadInt32(Root, TEXT("gridRulesVersion"), GridRules, OutError))
		return false;
	if (GridRules != FCitySaveDocument::CurrentGridRulesVersion)
	{
		if (OutFailure)
			*OutFailure = ECitySaveReadFailure::UnsupportedGrid;
		OutError = TEXT("Unsupported grid rules version");
		return false;
	}
	if (!ValidatePlatform(Dimensions, Platform, OutError))
	{
		if (OutFailure)
			*OutFailure = ECitySaveReadFailure::UnsupportedPlatform;
		return false;
	}

	Document = FCitySaveDocument(Dimensions);

	Document.GridRulesVersion = GridRules;
	{
		int32 Frequency = -1;
		if (!ReadInt32(Root, TEXT("disasterFrequency"), Frequency, OutError) || Frequency < 0 ||
		    Frequency > 3)
		{
			OutError = TEXT("Invalid disaster frequency");
			return false;
		}
		Document.Identity.DisasterFrequency = static_cast<ECityDisasterFrequency>(Frequency);
	}
	{
		int32 Preset = -1;
		if (!Root->TryGetStringField(TEXT("cityId"), Document.Identity.Id) ||
		    !Root->TryGetStringField(TEXT("cityName"), Document.Identity.Name) ||
		    !Root->TryGetStringField(TEXT("createdGameRevision"), Document.Identity.CreatedGameRevision) ||
		    !Root->TryGetStringField(TEXT("lastSavedGameRevision"), Document.LastSavedGameRevision) ||
		    !FCityGameRevision::IsValid(Document.LastSavedGameRevision) ||
		    !ReadInt32(Root, TEXT("difficulty"), Preset, OutError) || Preset < 0 || Preset > 2 ||
		    !ReadInt32(Root, TEXT("rulesVersion"), Document.Identity.RulesVersion, OutError))
		{
			OutError = TEXT("Missing or invalid city identity/difficulty");
			return false;
		}
		Document.Identity.Difficulty = static_cast<ECityDifficulty>(Preset);
		if (!Document.Identity.Validate(OutError))
			return false;
		if (!ReadInt32(Root, TEXT("ruleCrimePopulationThreshold"),
		               Document.DifficultyRules.CrimePopulationThreshold, OutError) ||
		    Document.DifficultyRules.CrimePopulationThreshold < 1 ||
		    Document.DifficultyRules.CrimePopulationThreshold > 100000)
		{
			OutError = TEXT("Invalid saved difficulty rule: CrimePopulationThreshold");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleFirePopulationThreshold"),
		               Document.DifficultyRules.FirePopulationThreshold, OutError) ||
		    Document.DifficultyRules.FirePopulationThreshold < 1 ||
		    Document.DifficultyRules.FirePopulationThreshold > 100000)
		{
			OutError = TEXT("Invalid saved difficulty rule: FirePopulationThreshold");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleCrimeGraceMonths"), Document.DifficultyRules.CrimeGraceMonths,
		               OutError) ||
		    Document.DifficultyRules.CrimeGraceMonths < 0 || Document.DifficultyRules.CrimeGraceMonths > 365)
		{
			OutError = TEXT("Invalid saved difficulty rule: CrimeGraceMonths");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleFireGraceMonths"), Document.DifficultyRules.FireGraceMonths,
		               OutError) ||
		    Document.DifficultyRules.FireGraceMonths < 0 || Document.DifficultyRules.FireGraceMonths > 365)
		{
			OutError = TEXT("Invalid saved difficulty rule: FireGraceMonths");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleCrimeCheckIntervalSteps"),
		               Document.DifficultyRules.CrimeCheckIntervalSteps, OutError) ||
		    Document.DifficultyRules.CrimeCheckIntervalSteps < 1 ||
		    Document.DifficultyRules.CrimeCheckIntervalSteps > 365)
		{
			OutError = TEXT("Invalid saved difficulty rule: CrimeCheckIntervalSteps");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleFireCheckIntervalSteps"),
		               Document.DifficultyRules.FireCheckIntervalSteps, OutError) ||
		    Document.DifficultyRules.FireCheckIntervalSteps < 1 ||
		    Document.DifficultyRules.FireCheckIntervalSteps > 365)
		{
			OutError = TEXT("Invalid saved difficulty rule: FireCheckIntervalSteps");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleFireRepairCostPercent"),
		               Document.DifficultyRules.FireRepairCostPercent, OutError) ||
		    Document.DifficultyRules.FireRepairCostPercent < 1 ||
		    Document.DifficultyRules.FireRepairCostPercent > 100)
		{
			OutError = TEXT("Invalid saved difficulty rule: FireRepairCostPercent");
			return false;
		}
		if (!ReadInt32(Root, TEXT("ruleFireRepairSteps"), Document.DifficultyRules.FireRepairSteps,
		               OutError) ||
		    Document.DifficultyRules.FireRepairSteps < 1 || Document.DifficultyRules.FireRepairSteps > 365)
		{
			OutError = TEXT("Invalid saved difficulty rule: FireRepairSteps");
			return false;
		}
	}
	if ((!ReadInt32(Root, TEXT("ruleNewArrivalUneducatedPercent"),
	                Document.DifficultyRules.NewArrivalUneducatedPercent, OutError) ||
	     Document.DifficultyRules.NewArrivalUneducatedPercent < 0 ||
	     Document.DifficultyRules.NewArrivalUneducatedPercent > 100))
	{
		OutError = TEXT("Missing or invalid newcomer education percentage");
		return false;
	}
	if (!ReadInt64(Root, TEXT("funds"), Document.Placement.Funds, OutError) ||
	    !ReadUInt64(Root, TEXT("revision"), Document.Placement.Revision, OutError) ||
	    !ReadUInt64(Root, TEXT("nextEntityId"), Document.Placement.NextEntityId, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ZoneValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("zones"), ZoneValues) || ZoneValues->Num() != Dimensions.GetCellCount())
	{
		OutError = TEXT("Save has invalid zone dimensions");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *ZoneValues)
	{
		const double Number = Value.IsValid() && Value->Type == EJson::Number ? Value->AsNumber() : -1.0;
		if (Number < 0.0 || Number > 3.0 || Number != FMath::TruncToDouble(Number))
		{
			OutError = TEXT("Save has an invalid zone value");
			return false;
		}
		Document.Placement.Zones.Add(static_cast<ECityZoneType>(static_cast<int32>(Number)));
	}

	const TArray<TSharedPtr<FJsonValue>>* RoadValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("roads"), RoadValues) ||
	    RoadValues->Num() > Dimensions.GetCellCount() * 2)
	{
		OutError = TEXT("Save is missing roads");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *RoadValues)
	{
		FCityCellCoord Cell;
		if (!ReadCell(Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr, Cell,
		              OutError))
		{
			return false;
		}
		{
			const auto O = Value->AsObject();
			if (!ReadInt32(O, TEXT("level"), Cell.RoadLevel, OutError) || Cell.RoadLevel < 0 ||
			    Cell.RoadLevel > 1)
				return false;
		}
		if (!Dimensions.IsValidCell(Cell))
		{
			OutError = TEXT("Road coordinate exceeds save grid");
			return false;
		}
		Document.Placement.RoadCells.Add(Cell);
		FString RoadDefinitionId = TEXT("LocalRoad");
		const TSharedPtr<FJsonObject> RoadObject =
		    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		if ((!RoadObject.IsValid() ||
		     !RoadObject->TryGetStringField(TEXT("definitionId"), RoadDefinitionId) ||
		     RoadDefinitionId.IsEmpty()))
		{
			OutError = TEXT("Save has an invalid road definition");
			return false;
		}
		Document.Placement.RoadRecords.Add({Cell, FName(*RoadDefinitionId)});
	}

	{
		const TArray<TSharedPtr<FJsonValue>>* Spans = nullptr;
		if (!Root->TryGetArrayField(TEXT("roadStructures"), Spans) ||
		    Spans->Num() > Dimensions.GetCellCount() / 3)
		{
			OutError = TEXT("Missing or excessive road structures");
			return false;
		}
		for (const auto& V : *Spans)
		{
			auto O = V.IsValid() && V->Type == EJson::Object ? V->AsObject() : nullptr;
			const TSharedPtr<FJsonObject>* A = nullptr;
			const TSharedPtr<FJsonObject>* B = nullptr;
			FCityRoadStructure S;
			int32 Profile = 0;
			if (!O || !O->TryGetObjectField(TEXT("start"), A) || !O->TryGetObjectField(TEXT("end"), B) ||
			    !ReadCell(*A, S.Start, OutError) || !ReadCell(*B, S.End, OutError) ||
			    !ReadInt32(O, TEXT("profile"), Profile, OutError) || Profile < 2 || Profile > 3 ||
			    !ReadInt32(O, TEXT("deck"), S.DeckHeightCm, OutError) ||
			    !ReadInt32(O, TEXT("startHeight"), S.StartHeightCm, OutError) ||
			    !ReadInt32(O, TEXT("endHeight"), S.EndHeightCm, OutError) ||
			    !ReadInt32(O, TEXT("startRamp"), S.StartRampCells, OutError) ||
			    !ReadInt32(O, TEXT("endRamp"), S.EndRampCells, OutError) ||
			    !ReadInt64(O, TEXT("cost"), S.RecordedCost, OutError))
				return false;
			if (!ReadInt32(*A, TEXT("level"), S.Start.RoadLevel, OutError))
				return false;
			if (!ReadInt32(*B, TEXT("level"), S.End.RoadLevel, OutError))
				return false;
			if (!Dimensions.IsValidCell(S.Start) || !Dimensions.IsValidCell(S.End))
			{
				OutError = TEXT("Span endpoint exceeds save grid");
				return false;
			}
			if ((!ReadInt32(O, TEXT("elevationStep"), S.ElevationStep, OutError) || S.ElevationStep < 0 ||
			     S.ElevationStep > FCityPlacementModel::MaximumRoadElevationStep))
				return false;
			{
				FString Definition;
				if (!O->TryGetStringField(TEXT("definitionId"), Definition) || Definition.IsEmpty())
				{
					OutError = TEXT("Missing raised span road type");
					return false;
				}
				S.DefinitionId = FName(*Definition);
			}
			S.Profile = static_cast<ECityRoadProfile>(Profile);
			Document.Placement.RoadStructures.Add(S);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* BuildingValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("buildings"), BuildingValues) ||
	    BuildingValues->Num() > Dimensions.GetCellCount())
	{
		OutError = TEXT("Save is missing buildings");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *BuildingValues)
	{
		const TSharedPtr<FJsonObject> Object =
		    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FCityPlacedEntity Entity;
		FString DefinitionId;
		if (!ReadUInt64(Object, TEXT("entityId"), Entity.EntityId, OutError) || !Object.IsValid() ||
		    !Object->TryGetStringField(TEXT("definitionId"), DefinitionId) || DefinitionId.IsEmpty() ||
		    !ReadInt32(Object, TEXT("x"), Entity.Anchor.X, OutError) ||
		    !ReadInt32(Object, TEXT("y"), Entity.Anchor.Y, OutError) ||
		    !ReadInt32(Object, TEXT("quarterTurns"), Entity.QuarterTurnsClockwise, OutError) ||
		    !ReadInt32(Object, TEXT("width"), Entity.FootprintSize.X, OutError) ||
		    !ReadInt32(Object, TEXT("height"), Entity.FootprintSize.Y, OutError) ||
		    !ReadInt64(Object, TEXT("recordedCost"), Entity.RecordedPlacementCost, OutError))
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Save has an invalid building record");
			return false;
		}
		Entity.DefinitionId = FName(*DefinitionId);

		Document.Placement.Entities.Add(Entity);
	}

	const TSharedPtr<FJsonObject>* EconomyObject = nullptr;
	const TSharedPtr<FJsonObject>* GrowthObject = nullptr;
	const TSharedPtr<FJsonObject>* ScenarioObject = nullptr;
	uint64 RandomState = 0;
	if (!Root->TryGetObjectField(TEXT("economy"), EconomyObject) ||
	    !ReadEconomy(*EconomyObject, Document, OutError) ||
	    !Root->TryGetObjectField(TEXT("growth"), GrowthObject) ||
	    !ReadUInt64(*GrowthObject, TEXT("randomState"), RandomState, OutError) || RandomState > MAX_uint32 ||
	    !ReadInt64(*GrowthObject, TEXT("nextGrowthStep"), Document.NextGrowthStep, OutError) ||
	    !Root->TryGetObjectField(TEXT("scenario"), ScenarioObject) ||
	    !ReadInt32(*ScenarioObject, TEXT("state"), Document.Scenario.State, OutError) ||
	    !ReadInt32(*ScenarioObject, TEXT("profitableMonthStreak"), Document.Scenario.ProfitableMonthStreak,
	               OutError) ||
	    !ReadBool(*ScenarioObject, TEXT("complete"), Document.Scenario.bComplete, OutError))
	{
		if (OutError.IsEmpty())
			OutError = TEXT("Save is missing simulation state");
		return false;
	}
	if (!ReadInt64(*ScenarioObject, TEXT("lastObservedBilledMonthId"),
	               Document.Scenario.LastObservedBilledMonthId, OutError))
	{
		return false;
	}
	{
		const TSharedPtr<FJsonObject>* GuidanceObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Completed = nullptr;
		FString CurrentId;
		if (!Root->TryGetObjectField(TEXT("guidance"), GuidanceObject) ||
		    !(*GuidanceObject)->TryGetArrayField(TEXT("completedIds"), Completed) ||
		    Completed->Num() > FCityFirstPlayableGuide::MilestoneIds().Num() ||
		    !(*GuidanceObject)->TryGetStringField(TEXT("currentId"), CurrentId) || CurrentId.Len() > 32)
		{
			OutError = TEXT("Missing or invalid guided milestone state");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Completed)
		{
			FString Id;
			if (!Value.IsValid() || !Value->TryGetString(Id) || Id.IsEmpty() || Id.Len() > 32)
			{
				OutError = TEXT("Invalid guided milestone ID");
				return false;
			}
			Document.Guidance.CompletedIds.Add(FName(*Id));
		}
		Document.Guidance.CurrentId = FName(*CurrentId);

		// An accepted pre-health guide could legitimately end after its first15 milestones.

		if (!Document.Identity.Validate(OutError) ||
		    !FCityGameRevision::IsValid(Document.LastSavedGameRevision) ||
		    !FCityFirstPlayableGuide::ValidateProgress(Document.Guidance, OutError))
			return false;
	}

	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Incidents = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Damage = nullptr;
		int64 Seed = 0;
		if (!Root->TryGetObjectField(TEXT("fire"), Object) ||
		    !ReadInt64(*Object, TEXT("lastFixedStepId"), Document.Fire.LastFixedStepId, OutError) ||
		    !ReadInt64(*Object, TEXT("activationStep"), Document.Fire.ActivationStep, OutError) ||
		    !ReadInt64(*Object, TEXT("graceEndStep"), Document.Fire.GraceEndStep, OutError) ||
		    !ReadInt64(*Object, TEXT("nextIgnitionStep"), Document.Fire.NextIgnitionStep, OutError) ||
		    !ReadInt64(*Object, TEXT("totalIncidents"), Document.Fire.TotalIncidents, OutError) ||
		    !ReadInt64(*Object, TEXT("preventedIncidents"), Document.Fire.PreventedIncidents, OutError) ||
		    !ReadInt64(*Object, TEXT("randomState"), Seed, OutError) || Seed <= 0 || Seed > MAX_uint32 ||
		    !(*Object)->TryGetArrayField(TEXT("incidents"), Incidents) ||
		    Incidents->Num() > FCityFireSimulation::MaximumRecentIncidents ||
		    !(*Object)->TryGetArrayField(TEXT("damage"), Damage) ||
		    Damage->Num() > (Dimensions.GetCellCount()))
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Missing or invalid fire state");
			return false;
		}
		Document.Fire.RandomState = static_cast<uint32>(Seed);
		for (const auto& Value : *Incidents)
		{
			const auto Item = Value->AsObject();
			FCityFireIncident Incident;
			if (!ReadInt64(Item, TEXT("id"), Incident.Id, OutError) ||
			    !ReadUInt64(Item, TEXT("entityId"), Incident.EntityId, OutError) ||
			    !ReadUInt64(Item, TEXT("spreadFromEntityId"), Incident.SpreadFromEntityId, OutError) ||
			    !ReadInt64(Item, TEXT("startStep"), Incident.StartStep, OutError) ||
			    !ReadInt64(Item, TEXT("resolvedStep"), Incident.ResolvedStep, OutError) ||
			    !ReadInt32(Item, TEXT("responseSteps"), Incident.ResponseSteps, OutError))
				return false;
			if (!ReadBool(Item, TEXT("test"), Incident.bTest, OutError))
				return false;
			if ((!ReadBool(Item, TEXT("arson"), Incident.bArson, OutError) ||
			     !ReadUInt64(Item, TEXT("offender"), Incident.OffenderCitizenId, OutError)))
				return false;
			Document.Fire.RecentIncidents.Add(Incident);
		}
		for (const auto& Value : *Damage)
		{
			const auto Item = Value->AsObject();
			FCityFireDamage Record;
			uint64 Id = 0;
			if (!ReadUInt64(Item, TEXT("entityId"), Id, OutError) ||
			    !ReadInt32(Item, TEXT("damage"), Record.Damage, OutError) ||
			    !ReadInt64(Item, TEXT("repairCompleteStep"), Record.RepairCompleteStep, OutError) ||
			    Document.Fire.DamagedBuildings.Contains(Id))
			{
				if (OutError.IsEmpty())
					OutError = TEXT("Duplicate fire damage");
				return false;
			}
			if ((!ReadInt32(Item, TEXT("cause"), Record.Cause, OutError) || Record.Cause < 0 ||
			     Record.Cause > 2))
				return false;
			Document.Fire.DamagedBuildings.Add(Id, Record);
		}
		if (!FCityFireSimulation::ValidateState(Document.Fire, Document.Economy.FixedStepId, OutError))
			return false;
	}

	{
		const TSharedPtr<FJsonObject>* Crime = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Incidents = nullptr;
		int64 Seed = 0;
		if (!Root->TryGetObjectField(TEXT("crime"), Crime) ||
		    !ReadInt64(*Crime, TEXT("lastFixedStepId"), Document.Crime.LastFixedStepId, OutError) ||
		    !ReadInt64(*Crime, TEXT("activationStep"), Document.Crime.ActivationStep, OutError) ||
		    !ReadInt64(*Crime, TEXT("graceEndStep"), Document.Crime.GraceEndStep, OutError) ||
		    !ReadInt64(*Crime, TEXT("randomState"), Seed, OutError) || Seed <= 0 || Seed > MAX_uint32 ||
		    !ReadInt32(*Crime, TEXT("previousHousedResidents"), Document.Crime.PreviousHousedResidents,
		               OutError) ||
		    !ReadInt32(*Crime, TEXT("homelessResidents"), Document.Crime.HomelessResidents, OutError) ||
		    !ReadInt64(*Crime, TEXT("totalIncidents"), Document.Crime.TotalIncidents, OutError) ||
		    !ReadInt64(*Crime, TEXT("preventedIncidents"), Document.Crime.PreventedIncidents, OutError) ||
		    !(*Crime)->TryGetArrayField(TEXT("incidents"), Incidents) ||
		    Incidents->Num() > FCityCrimeSimulation::MaximumRecentIncidents)
		{
			OutError = TEXT("Missing or invalid crime state");
			return false;
		}
		Document.Crime.RandomState = static_cast<uint32>(Seed);
		if ((!ReadInt64(*Crime, TEXT("lastArsonStep"), Document.Crime.LastArsonStep, OutError) ||
		     !ReadInt64(*Crime, TEXT("totalArsons"), Document.Crime.TotalArsons, OutError) ||
		     !ReadInt64(*Crime, TEXT("totalArrests"), Document.Crime.TotalArrests, OutError)))
			return false;
		for (const auto& Value : *Incidents)
		{
			const auto Record = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FCityCrimeIncident Incident;
			if (!ReadInt64(Record, TEXT("id"), Incident.Id, OutError) ||
			    !ReadUInt64(Record, TEXT("entityId"), Incident.EntityId, OutError) ||
			    !ReadInt64(Record, TEXT("startStep"), Incident.StartStep, OutError) ||
			    !ReadInt64(Record, TEXT("endStep"), Incident.EndStep, OutError))
				return false;
			if (!ReadBool(Record, TEXT("test"), Incident.bTest, OutError))
				return false;
			{
				int32 Outcome = 0;
				if (!ReadBool(Record, TEXT("arson"), Incident.bArson, OutError) ||
				    !ReadUInt64(Record, TEXT("offender"), Incident.OffenderCitizenId, OutError) ||
				    !ReadUInt64(Record, TEXT("arrestStation"), Incident.ArrestStationId, OutError) ||
				    !ReadInt64(Record, TEXT("captureStep"), Incident.CaptureStep, OutError) ||
				    !ReadInt32(Record, TEXT("arsonOutcome"), Outcome, OutError) || Outcome < 0 || Outcome > 5)
					return false;
				Incident.ArsonOutcome = static_cast<ECityArsonOutcome>(Outcome);
			}
			Document.Crime.RecentIncidents.Add(Incident);
		}
		if (!FCityCrimeSimulation::ValidateState(Document.Crime, Document.Economy.FixedStepId, OutError))
			return false;
	}
	// Older cities acquire teaching history from their live evaluators after load.
	Document.GrowthRandomState = static_cast<uint32>(RandomState);
	{
		const TSharedPtr<FJsonObject>* TerrainObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("terrain"), TerrainObject) ||
		    !ReadInt32(*TerrainObject, TEXT("seed"), Document.Terrain.Seed, OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("broadElevationAmplitudeCm"),
		               Document.Terrain.BroadElevationAmplitudeCm, OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("hillAmplitudeCm"), Document.Terrain.HillAmplitudeCm, OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("ridgeAmplitudeCm"), Document.Terrain.RidgeAmplitudeCm,
		               OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("smoothingPasses"), Document.Terrain.SmoothingPasses, OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("quantizationCm"), Document.Terrain.QuantizationCm, OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("starterHalfWidthCells"), Document.Terrain.StarterHalfWidthCells,
		               OutError) ||
		    !ReadInt32(*TerrainObject, TEXT("starterDepthCells"), Document.Terrain.StarterDepthCells,
		               OutError))
		{
			return false;
		}
		if (!ReadInt32(*TerrainObject, TEXT("seaLevelCm"), Document.Terrain.SeaLevelCm, OutError))
		{
			return false;
		}
		if (!ReadInt32(*TerrainObject, TEXT("generatorVersion"), Document.Terrain.GeneratorVersion, OutError))
			return false;
		if ((!ReadInt32(*TerrainObject, TEXT("waterRulesVersion"), Document.Terrain.WaterRulesVersion,
		                OutError) ||
		     !ReadInt32(*TerrainObject, TEXT("sceneryVersion"), Document.Terrain.SceneryVersion, OutError)))
			return false;
		if (!Document.Terrain.IsValid(&OutError))
		{
			return false;
		}
	}
	{
		const TSharedPtr<FJsonObject>* EnvironmentObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* PollutionValues = nullptr;
		if (!Root->TryGetObjectField(TEXT("environment"), EnvironmentObject) ||
		    !ReadInt64(*EnvironmentObject, TEXT("lastFixedStepId"), Document.Environment.LastFixedStepId,
		               OutError) ||
		    !ReadInt32(*EnvironmentObject, TEXT("totalPollution"), Document.Environment.TotalPollution,
		               OutError) ||
		    !ReadInt32(*EnvironmentObject, TEXT("peakPollution"), Document.Environment.PeakPollution,
		               OutError) ||
		    !(*EnvironmentObject)->TryGetArrayField(TEXT("pollution"), PollutionValues) ||
		    PollutionValues->Num() != Dimensions.GetCellCount())
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Save has invalid environment dimensions");
			return false;
		}
		Document.Environment.Pollution.Reset(Dimensions.GetCellCount());
		for (const TSharedPtr<FJsonValue>& Value : *PollutionValues)
		{
			const double Number = Value.IsValid() && Value->Type == EJson::Number ? Value->AsNumber() : -1.0;
			if (Number < 0.0 || Number > 1000.0 || Number != FMath::TruncToDouble(Number))
			{
				OutError = TEXT("Save has an invalid pollution value");
				return false;
			}
			Document.Environment.Pollution.Add(static_cast<int32>(Number));
		}
		{
			uint64 WeatherState = 0;
			int32 Season = 0, Temperature = 0, WindDirection = 0;
			if (!ReadUInt64(*EnvironmentObject, TEXT("weatherRandomState"), WeatherState, OutError) ||
			    !ReadInt64(*EnvironmentObject, TEXT("nextWeatherChangeStep"),
			               Document.Environment.NextWeatherChangeStep, OutError) ||
			    !ReadInt32(*EnvironmentObject, TEXT("season"), Season, OutError) ||
			    !ReadInt32(*EnvironmentObject, TEXT("temperature"), Temperature, OutError) ||
			    !ReadInt32(*EnvironmentObject, TEXT("windDirection"), WindDirection, OutError) ||
			    !ReadInt32(*EnvironmentObject, TEXT("windStrength"), Document.Environment.WindStrength,
			               OutError) ||
			    !(*EnvironmentObject)
			         ->TryGetBoolField(TEXT("precipitation"), Document.Environment.bPrecipitation))
			{
				if (OutError.IsEmpty())
					OutError = TEXT("Save has invalid weather state");
				return false;
			}
			Document.Environment.WeatherRandomState = static_cast<uint32>(WeatherState);
			Document.Environment.Season = static_cast<ECitySeason>(Season);
			Document.Environment.Temperature = static_cast<ECityTemperatureBand>(Temperature);
			Document.Environment.WindDirection = static_cast<ECityWindDirection>(WindDirection);
		}
		if (!Document.Environment.IsValid(&OutError))
		{
			return false;
		}
	}
	{
		const TSharedPtr<FJsonObject>* DevelopmentObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* StateValues = nullptr;
		if (!Root->TryGetObjectField(TEXT("development"), DevelopmentObject) ||
		    !ReadInt64(*DevelopmentObject, TEXT("lastFixedStepId"), Document.Development.LastFixedStepId,
		               OutError) ||
		    !(*DevelopmentObject)->TryGetArrayField(TEXT("states"), StateValues))
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Save is missing development state");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *StateValues)
		{
			const TSharedPtr<FJsonObject> Object =
			    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FCityDevelopmentState State;
			int32 Stage = 0;
			int32 Density = 0;
			if (!ReadUInt64(Object, TEXT("entityId"), State.EntityId, OutError) ||
			    !ReadInt32(Object, TEXT("stage"), Stage, OutError) || Stage < 0 || Stage > 1 ||
			    !ReadInt32(Object, TEXT("density"), Density, OutError) || Density < 0 || Density > 2 ||
			    !ReadInt64(Object, TEXT("constructionCompleteStep"), State.ConstructionCompleteStep,
			               OutError) ||
			    !ReadInt64(Object, TEXT("operationalSinceStep"), State.OperationalSinceStep, OutError) ||
			    Document.Development.States.Contains(State.EntityId))
			{
				if (OutError.IsEmpty())
					OutError = TEXT("Save has an invalid development record");
				return false;
			}
			State.Stage = static_cast<ECityDevelopmentStage>(Stage);
			State.DensityLevel = static_cast<ECityDensityLevel>(Density);

			if ((!ReadInt32(Object, TEXT("pendingCivicLevel"), State.PendingCivicLevel, OutError) ||
			     State.PendingCivicLevel < -1 || State.PendingCivicLevel > 2 ||
			     !ReadInt64(Object, TEXT("civicUpgradeCompleteStep"), State.CivicUpgradeCompleteStep,
			                OutError)))
				return false;
			if ((!ReadInt64(Object, TEXT("stationCompleteHalfDay"), State.StationCompleteHalfDay, OutError) ||
			     State.StationCompleteHalfDay < -1))
				return false;
			const auto* Station = Document.Placement.Entities.FindByPredicate(
			    [&State](const auto& Candidate) { return Candidate.EntityId == State.EntityId; });
			if (Station &&
			    (Station->DefinitionId == TEXT("Police") || Station->DefinitionId == TEXT("Fire")) &&
			    (State.Stage == ECityDevelopmentStage::Construction || State.HasCivicUpgrade()) &&
			    State.StationCompleteHalfDay < 0)
			{
				OutError = TEXT("Station construction is missing its half-day deadline");
				return false;
			}

			Document.Development.States.Add(State.EntityId, State);
		}
	}
	{
		const TSharedPtr<FJsonObject>* PopulationObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* CitizenValues = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* HouseholdValues = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* BusinessValues = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* TransitTripValues = nullptr;
		if (!Root->TryGetObjectField(TEXT("population"), PopulationObject) ||
		    !ReadUInt64(*PopulationObject, TEXT("nextCitizenId"), Document.Population.NextCitizenId,
		                OutError) ||
		    (!ReadUInt64(*PopulationObject, TEXT("nextTransitTripId"), Document.Population.NextTransitTripId,
		                 OutError)) ||
		    !ReadInt64(*PopulationObject, TEXT("lastSettledMonthId"), Document.Population.LastSettledMonthId,
		               OutError) ||
		    !(*PopulationObject)->TryGetArrayField(TEXT("citizens"), CitizenValues) ||
		    !(*PopulationObject)->TryGetArrayField(TEXT("households"), HouseholdValues) ||
		    !(*PopulationObject)->TryGetArrayField(TEXT("businesses"), BusinessValues))
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Save is missing population state");
			return false;
		}
		if ((SourceSchema == FCitySaveDocument::CurrentSchemaVersion || (*PopulationObject)->HasField(TEXT("commercialAiPilot"))) &&
		    !ReadBool(*PopulationObject, TEXT("commercialAiPilot"), Document.Population.bCommercialAiPilot, OutError))
			return false;
		if (!ReadInt64(*PopulationObject, TEXT("discardedGoods"), Document.Population.DiscardedGoods,
		               OutError))
			return false;
		{
			const TArray<TSharedPtr<FJsonValue>>* Holds = nullptr;
			if (!ReadInt64(*PopulationObject, TEXT("lastMigrationMonth"),
			               Document.Population.LastMigrationMonth, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("immigrationResumeMonth"),
			               Document.Population.ImmigrationResumeMonth, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("relocatedResidents"),
			               Document.Population.RelocatedResidents, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("departedResidents"),
			               Document.Population.DepartedResidents, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("movingExpenses"), Document.Population.MovingExpenses,
			               OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("departureMoney"), Document.Population.DepartureMoney,
			               OutError) ||
			    !(*PopulationObject)->TryGetArrayField(TEXT("housingVacancyHolds"), Holds) ||
			    Holds->Num() > Dimensions.GetCellCount())
				return false;
			for (const auto& V : *Holds)
			{
				const auto O = V.IsValid() && V->Type == EJson::Object ? V->AsObject() : nullptr;
				FCityHousingVacancyHold Hold;
				if (!ReadUInt64(O, TEXT("home"), Hold.HomeEntityId, OutError) ||
				    !ReadInt64(O, TEXT("untilMonth"), Hold.UntilMonth, OutError) ||
				    Hold.HomeEntityId >= Document.Placement.NextEntityId)
					return false;
				Document.Population.HousingVacancyHolds.Add(Hold);
			}
		}
		{
			const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
			if (!ReadUInt64(*PopulationObject, TEXT("nextHousingEventId"),
			                Document.Population.NextHousingEventId, OutError) ||
			    !(*PopulationObject)->TryGetArrayField(TEXT("housingEvents"), Events) ||
			    Events->Num() > FCityHousingHistory::MaximumEvents)
			{
				OutError = TEXT("Missing or invalid housing event history");
				return false;
			}
			for (const auto& V : *Events)
			{
				const auto O = V.IsValid() && V->Type == EJson::Object ? V->AsObject() : nullptr;
				FCityHousingEvent E;
				int32 Kind = 0, Status = 0;
				const TSharedPtr<FJsonObject>* Source = nullptr;
				const TSharedPtr<FJsonObject>* Destination = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
				if (!ReadUInt64(O, TEXT("id"), E.Id, OutError) ||
				    !ReadInt64(O, TEXT("month"), E.Month, OutError) ||
				    !ReadUInt64(O, TEXT("sourceHome"), E.SourceHome, OutError) ||
				    !ReadUInt64(O, TEXT("destinationHome"), E.DestinationHome, OutError) ||
				    !ReadInt32(O, TEXT("kind"), Kind, OutError) || Kind < 0 || Kind > 4 ||
				    !ReadInt32(O, TEXT("housingStatus"), Status, OutError) || Status < 0 || Status > 5 ||
				    !ReadInt32(O, TEXT("residents"), E.Residents, OutError) ||
				    E.Residents > Dimensions.GetCellCount() * 128 ||
				    !ReadInt32(O, TEXT("happiness"), E.Happiness, OutError) ||
				    !ReadInt32(O, TEXT("outlook"), E.Outlook, OutError) ||
				    !ReadInt32(O, TEXT("destinationOutlook"), E.DestinationOutlook, OutError) ||
				    !ReadInt32(O, TEXT("concerns"), E.Concerns, OutError) ||
				    !ReadInt64(O, TEXT("money"), E.Money, OutError) ||
				    !O->TryGetObjectField(TEXT("sourceCell"), Source) ||
				    !O->TryGetObjectField(TEXT("destinationCell"), Destination) ||
				    !ReadCell(*Source, E.SourceCell, OutError) ||
				    !ReadCell(*Destination, E.DestinationCell, OutError) ||
				    (E.SourceHome && !Dimensions.IsValidCell(E.SourceCell)) ||
				    (E.DestinationHome && !Dimensions.IsValidCell(E.DestinationCell)) ||
				    E.SourceHome >= Document.Placement.NextEntityId ||
				    E.DestinationHome >= Document.Placement.NextEntityId ||
				    !O->TryGetArrayField(TEXT("citizens"), Ids) ||
				    Ids->Num() > FCityHousingHistory::MaximumCitizenIds)
				{
					if (OutError.IsEmpty())
						OutError = TEXT("Invalid housing event fields");
					return false;
				}
				E.Kind = static_cast<ECityHousingEventKind>(Kind);
				E.HousingStatus = static_cast<uint8>(Status);
				for (const auto& Id : *Ids)
				{
					FString Text;
					uint64 Number = 0;
					if (!Id.IsValid() || !Id->TryGetString(Text) || !LexTryParseString(Number, *Text))
					{
						OutError = TEXT("Invalid housing event citizen");
						return false;
					}
					E.CitizenIds.Add(Number);
				}
				Document.Population.HousingEvents.Add(MoveTemp(E));
			}
		}
		if (!(*PopulationObject)->TryGetArrayField(TEXT("transitTrips"), TransitTripValues))
		{
			OutError = TEXT("Save is missing transit trips");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *CitizenValues)
		{
			const TSharedPtr<FJsonObject> Object =
			    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FCityCitizenRecord Record;
			int32 Activity = 0;
			int32 Trip = 0;
			int32 Vehicle = 0;
			if (!ReadUInt64(Object, TEXT("id"), Record.Id, OutError) ||
			    !ReadUInt64(Object, TEXT("householdId"), Record.HouseholdId, OutError) ||
			    !ReadUInt64(Object, TEXT("homeEntityId"), Record.HomeEntityId, OutError) ||
			    !ReadUInt64(Object, TEXT("workplaceEntityId"), Record.WorkplaceEntityId, OutError) ||
			    !ReadInt64(Object, TEXT("money"), Record.AvailableMoney, OutError) ||
			    !ReadInt32(Object, TEXT("need"), Record.ConsumptionNeed, OutError) ||
			    !ReadInt32(Object, TEXT("activity"), Activity, OutError) ||
			    !ReadInt32(Object, TEXT("trip"), Trip, OutError) ||
			    (!ReadInt32(Object, TEXT("vehicle"), Vehicle, OutError)) ||
			    !ReadUInt64(Object, TEXT("destination"), Record.TripDestinationEntityId, OutError) ||
			    !ReadInt64(Object, TEXT("nextAction"), Record.NextActionStep, OutError) || Activity < 0 ||
			    Activity > 2 || Trip < 0 || Trip > (7) || Vehicle < 0 || Vehicle > 2)
				return false;
			Record.Activity = static_cast<ECityCitizenActivity>(Activity);
			Record.TripPurpose = static_cast<ECityTripPurpose>(Trip);
			Record.TripVehicleType = static_cast<ECityTripVehicleType>(Vehicle);
			{
				int32 Schedule = 0;
				if (!ReadUInt64(Object, TEXT("currentLocation"), Record.CurrentLocationEntityId, OutError) ||
				    !ReadUInt64(Object, TEXT("pendingShop"), Record.PendingShopEntityId, OutError) ||
				    !ReadInt32(Object, TEXT("schedule"), Schedule, OutError) || Schedule < 0 || Schedule > 5)
					return false;
				Record.Schedule = static_cast<ECityCitizenSchedule>(Schedule);
				if ((SourceSchema >= FCitySaveDocument::PreOutpatientSchemaVersion || Object->HasField(TEXT("shopBeforeWork"))) && !ReadBool(Object, TEXT("shopBeforeWork"), Record.bShopBeforeWork, OutError)) return false;
			}
			{
				int32 Education = 0;
				if (!ReadInt32(Object, TEXT("education"), Education, OutError) || Education < 0 ||
				    Education > 3 ||
				    !ReadInt64(Object, TEXT("unemployedSinceStep"), Record.UnemployedSinceStep, OutError) ||
				    Record.UnemployedSinceStep < -1 ||
				    Record.UnemployedSinceStep > Document.Economy.FixedStepId)
				{
					OutError = TEXT("Invalid citizen education or unemployment clock");
					return false;
				}
				Record.Education = static_cast<ECityEducationLevel>(Education);
			}
			if ((!ReadUInt64(Object, TEXT("jailStation"), Record.JailStationId, OutError) ||
			     !ReadInt64(Object, TEXT("jailReleaseStep"), Record.JailReleaseStep, OutError) ||
			     !ReadInt64(Object, TEXT("reformedUntilStep"), Record.ReformedUntilStep, OutError) ||
			     (Record.IsDetained() && (Record.JailReleaseStep <= Document.Economy.FixedStepId ||
			                              Record.JailReleaseStep > Document.Economy.FixedStepId + 100000)) ||
			     Record.ReformedUntilStep > Document.Economy.FixedStepId + 100000))
				return false;
			{
				if (!ReadInt32(Object, TEXT("educationMonths"), Record.EducationMonths, OutError) ||
				    !ReadInt64(Object, TEXT("lastEducationMonth"), Record.LastEducationMonth, OutError) ||
				    !ReadBool(Object, TEXT("skilledJob"), Record.bSkilledJob, OutError) ||
				    Record.LastEducationMonth > Document.Economy.LastBilledMonthId)
					return false;
			}
			if ((!ReadInt64(Object, TEXT("policyGrant"), Record.LastPolicyGrant, OutError) ||
			     !ReadInt64(Object, TEXT("policyGrantMonth"), Record.LastPolicyGrantMonth, OutError) ||
			     Record.LastPolicyGrant < 0 || Record.LastPolicyGrant > 7 ||
			     Record.LastPolicyGrantMonth < 0 ||
			     Record.LastPolicyGrantMonth > Document.Economy.Policies.LastSettledMonth ||
			     (Record.LastPolicyGrantMonth == 0 && Record.LastPolicyGrant)))
				return false;
			{
				if (!ReadInt32(Object, TEXT("happiness"), Record.Happiness, OutError) ||
				    !ReadInt32(Object, TEXT("happinessTarget"), Record.HappinessTarget, OutError) ||
				    !ReadInt32(Object, TEXT("happinessConcerns"), Record.HappinessConcerns, OutError) ||
				    !ReadInt32(Object, TEXT("discontentMonths"), Record.DiscontentMonths, OutError) ||
				    !ReadInt64(Object, TEXT("lastHappinessMonth"), Record.LastHappinessMonth, OutError) ||
				    !FCityHappinessEvaluator::ValidateCitizen(Record, OutError) ||
				    Record.LastHappinessMonth > Document.Economy.LastBilledMonthId)
				{
					if (OutError.IsEmpty())
						OutError = TEXT("Future citizen happiness clock");
					return false;
				}
			}
			{
				int32 Status = 0;
				if (!ReadInt64(Object, TEXT("lastRelocationMonth"), Record.LastRelocationMonth, OutError) ||
				    !ReadInt32(Object, TEXT("housingStatus"), Status, OutError) || Status < 0 ||
				    Status > static_cast<int32>(ECityHousingStatus::Recovering))
					return false;
				Record.HousingStatus = static_cast<ECityHousingStatus>(Status);
				if (!FCityHappinessEvaluator::ValidateCitizen(Record, OutError))
					return false;
			}
			Document.Population.Citizens.Add(Record);
		}
		for (const TSharedPtr<FJsonValue>& Value : *HouseholdValues)
		{
			const TSharedPtr<FJsonObject> Object =
			    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FCityHouseholdRecord Record;
			if (!ReadUInt64(Object, TEXT("id"), Record.Id, OutError) ||
			    !ReadUInt64(Object, TEXT("homeEntityId"), Record.HomeEntityId, OutError) ||
			    !ReadInt64(Object, TEXT("balance"), Record.Balance, OutError) ||
			    !ReadInt32(Object, TEXT("orders"), Record.ConsumptionOrders, OutError) ||
			    !ReadInt32(Object, TEXT("purchases"), Record.CompletedPurchases, OutError))
				return false;
			if (!ReadInt32(Object, TEXT("purchasesThisMonth"), Record.PurchasesThisMonth, OutError))
				return false;
			if ((SourceSchema >= FCitySaveDocument::PreOutpatientSchemaVersion || Object->HasField(TEXT("storedSupplies"))) &&
			    (!ReadBool(Object, TEXT("storedSupplies"), Record.bUsesStoredSupplies, OutError) ||
			     !ReadInt32(Object, TEXT("supplyUnits"), Record.SupplyUnits, OutError) ||
			     !ReadInt64(Object, TEXT("supplyDay"), Record.SupplyDay, OutError) ||
			     !ReadInt64(Object, TEXT("lastEssentialDay"), Record.LastEssentialPurchaseDay, OutError) ||
			     !ReadInt64(Object, TEXT("lastComfortDay"), Record.LastComfortDay, OutError) ||
			     !ReadInt32(Object, TEXT("comfortTier"), Record.ComfortTier, OutError))) return false;
			Document.Population.Households.Add(Record);
		}
		for (const TSharedPtr<FJsonValue>& Value : *BusinessValues)
		{
			const TSharedPtr<FJsonObject> Object =
			    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			FCityBusinessLedgerRecord Record;
			if (!ReadUInt64(Object, TEXT("entityId"), Record.EntityId, OutError) ||
			    !ReadBool(Object, TEXT("industrial"), Record.bIndustrial, OutError) ||
			    !ReadInt32(Object, TEXT("jobs"), Record.JobCapacity, OutError) ||
			    !ReadInt32(Object, TEXT("inventory"), Record.Inventory, OutError) ||
			    !ReadInt32(Object, TEXT("orders"), Record.PendingOrders, OutError) ||
			    !ReadInt32(Object, TEXT("sales"), Record.CompletedSales, OutError) ||
			    !ReadInt64(Object, TEXT("balance"), Record.Balance, OutError) ||
			    !ReadInt64(Object, TEXT("openingPayrollReserve"), Record.OpeningPayrollReserve, OutError))
				return false;
			if (!ReadInt32(Object, TEXT("skilledJobs"), Record.SkilledJobCapacity, OutError))
				return false;
			{
				if (!ReadBool(Object, TEXT("initialFreightReceived"), Record.bInitialFreightReceived,
				              OutError))
					return false;
			} // Preserve businesses in existing cities.
			if (!ReadInt64(Object, TEXT("exportedGoods"), Record.ExportedGoods, OutError))
				return false;
			if ((!ReadInt64(Object, TEXT("payrollDue"), Record.PayrollDue, OutError) ||
			     !ReadInt64(Object, TEXT("payrollPaid"), Record.PayrollPaid, OutError)))
				return false;
			if (!ReadInt32(Object, TEXT("producedGoods"), Record.ProducedGoods, OutError))
				return false;
			if (!ReadInt32(Object, TEXT("salesThisMonth"), Record.SalesThisMonth, OutError))
				return false;
			if ((SourceSchema == FCitySaveDocument::CurrentSchemaVersion || Object->HasField(TEXT("skilledMinimumEducation"))) &&
			    (!ReadInt32(Object, TEXT("skilledMinimumEducation"), Record.SkilledMinimumEducation, OutError) ||
			    !ReadInt64(Object, TEXT("aiFeePaid"), Record.AiFeePaid, OutError) ||
			    !ReadInt64(Object, TEXT("aiPaidMonth"), Record.AiPaidMonth, OutError) ||
                !ReadUInt64(Object, TEXT("aiCenterAtPayment"), Record.AiCenterAtPayment, OutError) ||
                !ReadInt32(Object, TEXT("aiJobsAtPayment"), Record.AiJobsAtPayment, OutError) ||
                !ReadInt32(Object, TEXT("aiSpecialistsAtPayment"), Record.AiSpecialistsAtPayment, OutError)))
				return false;
			if ((SourceSchema >= FCitySaveDocument::PreOutpatientSchemaVersion || Object->HasField(TEXT("retailTier"))) &&
			    (!ReadInt32(Object, TEXT("retailTier"), Record.RetailTier, OutError) ||
			     !ReadBool(Object, TEXT("receivedGoods"), Record.bHasReceivedGoods, OutError) ||
			     !ReadInt64(Object, TEXT("openingGraceUntil"), Record.OpeningGraceUntilStep, OutError))) return false;
			if (!Object->HasField(TEXT("retailTier")))
			{
				Record.bHasReceivedGoods = Record.Inventory > 0 || Record.CompletedSales > 0 || Record.SalesThisMonth > 0;
				if (const auto* Existing = Document.Development.Find(Record.EntityId))
					Record.RetailTier = static_cast<int32>(Existing->DensityLevel);
			}
			Document.Population.Businesses.Add(Record);
		}
		{
			const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
			if (!ReadBool(*PopulationObject, TEXT("arrivalPurchases"),
			              Document.Population.bUsesPurchaseReservations, OutError) ||
			    !ReadInt32(*PopulationObject, TEXT("salesThisMonth"), Document.Population.SalesThisMonth,
			               OutError) ||
			    !ReadInt32(*PopulationObject, TEXT("lastMonthSales"), Document.Population.LastMonthSales,
			               OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("spendingThisMonth"),
			               Document.Population.SpendingThisMonth, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("lastMonthSpending"),
			               Document.Population.LastMonthSpending, OutError) ||
			    !ReadInt64(*PopulationObject, TEXT("totalArrivalPurchases"),
			               Document.Population.TotalArrivalPurchases, OutError) ||
			    !(*PopulationObject)->TryGetArrayField(TEXT("purchaseReservations"), Orders))
				return false;
			if (Orders->Num() > Document.Population.Citizens.Num())
			{
				OutError = TEXT("Too many purchase reservations");
				return false;
			}
			for (const auto& Value : *Orders)
			{
				const auto Object =
				    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
				FCityPurchaseReservation Order;
				const TArray<TSharedPtr<FJsonValue>>* Contributions = nullptr;
				if (!ReadUInt64(Object, TEXT("buyer"), Order.BuyerId, OutError) ||
				    !ReadUInt64(Object, TEXT("household"), Order.HouseholdId, OutError) ||
				    !ReadUInt64(Object, TEXT("shop"), Order.ShopId, OutError) ||
				    !ReadInt64(Object, TEXT("price"), Order.Price, OutError) ||
				    !Object->TryGetArrayField(TEXT("contributions"), Contributions))
					return false;
				if ((SourceSchema >= FCitySaveDocument::PreOutpatientSchemaVersion || Object->HasField(TEXT("quantity"))) &&
				    (!ReadInt32(Object, TEXT("quantity"), Order.Quantity, OutError) ||
				     !ReadInt32(Object, TEXT("comfortTier"), Order.ComfortTier, OutError))) return false;
				if (Contributions->Num() > Document.Population.Citizens.Num())
					return false;
				for (const auto& FundingValue : *Contributions)
				{
					const auto Funding = FundingValue.IsValid() && FundingValue->Type == EJson::Object
					                         ? FundingValue->AsObject()
					                         : nullptr;
					FCityPurchaseContribution Contribution;
					if (!ReadUInt64(Funding, TEXT("citizen"), Contribution.CitizenId, OutError) ||
					    !ReadInt64(Funding, TEXT("amount"), Contribution.Amount, OutError))
						return false;
					Order.Contributions.Add(Contribution);
				}
				Document.Population.PurchaseReservations.Add(MoveTemp(Order));
			}
			if (!FCityPopulationSimulation::ValidatePurchaseReservations(Document.Population, OutError))
				return false;
		}
		{
			const TArray<TSharedPtr<FJsonValue>>* Employers = nullptr;
			if (!(*PopulationObject)->TryGetArrayField(TEXT("publicEmployers"), Employers) ||
			    Employers->Num() > Dimensions.GetCellCount())
			{
				OutError = TEXT("Invalid civic employers");
				return false;
			}
			for (const auto& Value : *Employers)
			{
				const auto Object =
				    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
				FCityPublicEmployerRecord Employer;
				if (!ReadUInt64(Object, TEXT("entityId"), Employer.EntityId, OutError) ||
				    !ReadInt32(Object, TEXT("jobs"), Employer.JobCapacity, OutError) ||
				    !ReadInt64(Object, TEXT("payrollDue"), Employer.PayrollDue, OutError) ||
				    !ReadInt64(Object, TEXT("payrollPaid"), Employer.PayrollPaid, OutError))
					return false;
				Document.Population.PublicEmployers.Add(Employer);
			}
		}
		if (TransitTripValues)
		{
			for (const TSharedPtr<FJsonValue>& Value : *TransitTripValues)
			{
				const TSharedPtr<FJsonObject> Object =
				    Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
				FCityTransitTripRecord Record;
				int32 Purpose = 0;
				int32 Vehicle = 0;
				if (!ReadUInt64(Object, TEXT("id"), Record.Id, OutError) ||
				    !ReadUInt64(Object, TEXT("origin"), Record.OriginEntityId, OutError) ||
				    !ReadUInt64(Object, TEXT("destination"), Record.DestinationEntityId, OutError) ||
				    !ReadInt32(Object, TEXT("purpose"), Purpose, OutError) ||
				    !ReadInt32(Object, TEXT("vehicle"), Vehicle, OutError) ||
				    !ReadInt32(Object, TEXT("quantity"), Record.Quantity, OutError) ||
				    !ReadInt64(Object, TEXT("start"), Record.StartStep, OutError) ||
				    !ReadInt64(Object, TEXT("arrival"), Record.ArrivalStep, OutError) ||
				    !ReadBool(Object, TEXT("completed"), Record.bCompleted, OutError) || Purpose < 0 ||
				    Purpose > (SourceSchema >= FCitySaveDocument::PreAiSchemaVersion ? 17 : 15) || Vehicle < 0 || Vehicle > (6))
					return false;
				if ((!ReadUInt64(Object, TEXT("station"), Record.StationEntityId, OutError) ||
				     !ReadInt64(Object, TEXT("incident"), Record.IncidentId, OutError)))
					return false;
				if (!ReadUInt64(Object, TEXT("patientCitizen"), Record.PatientCitizenId, OutError))
					return false;
				Record.Purpose = static_cast<ECityTripPurpose>(Purpose);
				Record.VehicleType = static_cast<ECityTripVehicleType>(Vehicle);
				Document.Population.TransitTrips.Add(Record);
			}
		}

		{
			int32 Homeless = 0;
			for (const auto& Citizen : Document.Population.Citizens)
				Homeless += Citizen.IsHomeless() ? 1 : 0;
			if (Homeless != Document.Crime.HomelessResidents)
			{
				OutError = TEXT("Crime homelessness does not match the citizen census");
				return false;
			}
		}
	}
	{
		const TSharedPtr<FJsonObject>* TrafficObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Vehicles = nullptr;
		if (!Root->TryGetObjectField(TEXT("traffic"), TrafficObject) ||
		    !ReadInt64(*TrafficObject, TEXT("fixedStepId"), Document.Traffic.FixedStepId, OutError) ||
		    !ReadUInt64(*TrafficObject, TEXT("networkFingerprint"), Document.Traffic.NetworkFingerprint,
		                OutError) ||
		    !(*TrafficObject)->TryGetArrayField(TEXT("vehicles"), Vehicles))
		{
			OutError = TEXT("Missing or invalid traffic state");
			return false;
		}
		auto ReadVehicles = [&](const TArray<TSharedPtr<FJsonValue>>& Records, const bool bWaiting) {
			if (Records.Num() > 2048)
				return false;
			for (const TSharedPtr<FJsonValue>& Value : Records)
			{
				const TSharedPtr<FJsonObject> Object = Value->Type == EJson::Object ? Value->AsObject() : nullptr;
				FCityTrafficVehicleState Vehicle;
				const TArray<TSharedPtr<FJsonValue>>* Legs = nullptr;
				if (!ReadUInt64(Object, TEXT("id"), Vehicle.Id, OutError) ||
				    !ReadInt32(Object, TEXT("routeLegIndex"), Vehicle.RouteLegIndex, OutError) ||
				    !ReadInt32(Object, TEXT("distanceUnits"), Vehicle.DistanceUnits, OutError) ||
				    !ReadInt32(Object, TEXT("speedUnitsPerStep"), Vehicle.SpeedUnitsPerStep, OutError) ||
				    !ReadInt32(Object, TEXT("lengthUnits"), Vehicle.LengthUnits, OutError) ||
				    !ReadBool(Object, TEXT("completed"), Vehicle.bCompleted, OutError) ||
				    !Object->TryGetArrayField(TEXT("route"), Legs))
				{
					OutError = TEXT("Missing or invalid traffic vehicle");
					return false;
				}
				{
					int32 Type = 0;
					if (!ReadInt32(Object, TEXT("startDistance"), Vehicle.StartDistanceUnits, OutError) ||
					    !ReadInt32(Object, TEXT("endDistance"), Vehicle.EndDistanceUnits, OutError) ||
					    !ReadInt32(Object, TEXT("vehicleType"), Type, OutError) || Type < 0 || Type > (6) ||
					    !ReadUInt64(Object, TEXT("sourceId"), Vehicle.SourceId, OutError) ||
					    !ReadUInt64(Object, TEXT("originEntityId"), Vehicle.OriginEntityId, OutError) ||
					    !ReadUInt64(Object, TEXT("destinationEntityId"), Vehicle.DestinationEntityId, OutError) ||
					    !ReadInt64(Object, TEXT("completedStep"), Vehicle.CompletedStep, OutError) ||
					    !ReadBool(Object, TEXT("transitTrip"), Vehicle.bTransitTrip, OutError) ||
					    !ReadBool(Object, TEXT("returning"), Vehicle.bReturning, OutError))
						return false;
					Vehicle.VehicleType = static_cast<ECityTripVehicleType>(Type);
				}
				{
					if (!ReadBool(Object, TEXT("responding"), Vehicle.bEmergencyResponding, OutError) ||
					    !ReadBool(Object, TEXT("yielding"), Vehicle.bYielding, OutError))
						return false;
				}
				if (Object->HasField(TEXT("pedestrianTrack")) &&
				    !ReadInt32(Object, TEXT("pedestrianTrack"), Vehicle.PedestrianTrack, OutError))
					return false;
				for (const TSharedPtr<FJsonValue>& LegValue : *Legs)
				{
					const TSharedPtr<FJsonObject> LegObject =
					    LegValue->Type == EJson::Object ? LegValue->AsObject() : nullptr;
					FCityTrafficRouteLeg Leg;
					if (!ReadInt32(LegObject, TEXT("edgeId"), Leg.EdgeId, OutError) ||
					    !ReadBool(LegObject, TEXT("reverse"), Leg.bReverse, OutError) ||
					    !ReadInt32(LegObject, TEXT("laneIndex"), Leg.LaneIndex, OutError))
						return false;
					Vehicle.Route.Add(Leg);
				}
				if (bWaiting && !ReadInt64(Object, TEXT("requestedStep"), Vehicle.DepartureRequestedStep, OutError))
					return false;
				(bWaiting ? Document.Traffic.WaitingDepartures : Document.Traffic.Vehicles).Add(MoveTemp(Vehicle));
			}
			return true;
		};
		if (!ReadVehicles(*Vehicles, false))
			return false;
		// Additive format67 state: existing current cities have no queued routes yet.
		if ((*TrafficObject)->HasField(TEXT("waitingDepartures")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Waiting = nullptr;
			if (!(*TrafficObject)->TryGetArrayField(TEXT("waitingDepartures"), Waiting) || !ReadVehicles(*Waiting, true))
				return false;
		}
	}

	{
		const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
		if (!Root->TryGetArrayField(TEXT("reports"), Samples) ||
		    Samples->Num() > FCityReportHistory::MaximumMonths)
		{
			OutError = TEXT("Invalid report history");
			return false;
		}
		for (const auto& Value : *Samples)
		{
			FCityReportSample Sample;
			const TSharedPtr<FJsonObject>* ObjectPointer = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjectPointer))
			{
				OutError = TEXT("Invalid report sample object");
				return false;
			}
			const auto Object = *ObjectPointer;
			if (!ReadInt64(Object, TEXT("month"), Sample.MonthId, OutError))
				return false;
			if (!ReadBool(Object, TEXT("safetyRecorded"), Sample.bHasSafety, OutError))
				return false;
			if (!ReadBool(Object, TEXT("healthRecorded"), Sample.bHasHealth, OutError))
				return false;
			if (!ReadBool(Object, TEXT("wasteRecorded"), Sample.bHasWaste, OutError))
				return false;
			if (!ReadBool(Object, TEXT("migrationRecorded"), Sample.bHasMigration, OutError))
				return false;
			for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
				if (!ReadInt64(Object, FCityReportHistory::Name(static_cast<ECityReportMetric>(I)),
				               Sample.Values[I], OutError))
					return false;
			if (!ReadBool(Object, TEXT("policiesRecorded"), Sample.bHasPolicies, OutError))
				return false;
			if (!ReadBool(Object, TEXT("loansRecorded"), Sample.bHasLoans, OutError))
				return false;
			if (!ReadBool(Object, TEXT("sentimentRecorded"), Sample.bHasSentiment, OutError))
				return false;
			Document.Reports.Months.Add(Sample);
		}
		if (!Document.Reports.Validate(Document.Economy.LastBilledMonthId, OutError) ||
		    !ReadInt64(Root, TEXT("exportsThisMonth"), Document.Population.ExportsThisMonth, OutError) ||
		    !ReadInt64(Root, TEXT("lastMonthExports"), Document.Population.LastMonthExports, OutError) ||
		    Document.Population.ExportsThisMonth < 0 || Document.Population.LastMonthExports < 0)
			return false;
	}
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
		uint64 Random = 0;
		auto& State = Document.Disasters;
		if (!Root->TryGetObjectField(TEXT("disasters"), O) ||
		    !ReadInt64(*O, TEXT("lastStep"), State.LastFixedStepId, OutError) ||
		    !ReadInt64(*O, TEXT("nextCheck"), State.NextCheckStep, OutError) ||
		    !ReadUInt64(*O, TEXT("random"), Random, OutError) || Random > MAX_uint32 ||
		    !ReadInt64(*O, TEXT("total"), State.TotalEvents, OutError) ||
		    !(*O)->TryGetArrayField(TEXT("events"), Events) ||
		    Events->Num() > FCityDisasterSimulation::MaximumEvents)
		{
			OutError = TEXT("Missing or invalid natural disaster state");
			return false;
		}
		State.RandomState = static_cast<uint32>(Random);
		for (const auto& Value : *Events)
		{
			const auto V = Value->AsObject();
			FCityDisasterEvent E;
			E.Dimensions = Dimensions;
			int32 Kind = 0;
			const TArray<TSharedPtr<FJsonValue>>* Affected = nullptr;
			if (!ReadInt64(V, TEXT("id"), E.Id, OutError) || !ReadInt32(V, TEXT("kind"), Kind, OutError) ||
			    Kind < 1 || Kind > 2 || !ReadInt32(V, TEXT("x"), E.Center.X, OutError) ||
			    !ReadInt32(V, TEXT("y"), E.Center.Y, OutError) ||
			    !ReadInt32(V, TEXT("direction"), E.Direction, OutError) ||
			    !ReadInt64(V, TEXT("warning"), E.WarningStep, OutError) ||
			    !ReadInt64(V, TEXT("start"), E.StartStep, OutError) ||
			    !ReadInt64(V, TEXT("end"), E.EndStep, OutError) ||
			    !V->TryGetBoolField(TEXT("test"), E.bTest) ||
			    !V->TryGetArrayField(TEXT("affected"), Affected) ||
			    Affected->Num() > Dimensions.GetCellCount())
			{
				OutError = TEXT("Invalid natural disaster event");
				return false;
			}
			E.Kind = static_cast<ECityNaturalDisaster>(Kind);
			{
				const TArray<TSharedPtr<FJsonValue>>* Plan = nullptr;
				if (!ReadInt32(V, TEXT("profile"), E.Profile, OutError) ||
				    !ReadInt32(V, TEXT("severity"), E.SeverityPercent, OutError) ||
				    !ReadInt32(V, TEXT("initialBuildings"), E.InitialBuildings, OutError) ||
				    !V->TryGetArrayField(TEXT("plan"), Plan) || Plan->Num() > Dimensions.GetCellCount())
					return false;
				{
					const TArray<TSharedPtr<FJsonValue>>* Distances = nullptr;
					if (!ReadInt32(V, TEXT("radiusSquared"), E.RadiusSquared, OutError) ||
					    !V->TryGetArrayField(TEXT("travelDistances"), Distances) ||
					    Distances->Num() > Dimensions.GetCellCount())
						return false;
					for (const auto& Entry : *Distances)
					{
						double Distance = 0;
						if (!Entry->TryGetNumber(Distance) || !FMath::IsFinite(Distance))
							return false;
						E.TravelDistances.Add(Distance);
					}
				}
				for (const auto& Entry : *Plan)
				{
					const auto P = Entry->AsObject();
					uint64 Id = 0;
					FCityCellCoord Cell;
					if (!ReadUInt64(P, TEXT("id"), Id, OutError) ||
					    !ReadInt32(P, TEXT("x"), Cell.X, OutError) ||
					    !ReadInt32(P, TEXT("y"), Cell.Y, OutError))
						return false;
					E.PlannedTargets.Add(Id);
					E.Path.Add(Cell);
				}
			}

			for (const auto& Id : *Affected)
			{
				auto F = MakeShared<FJsonObject>();
				F->SetField(TEXT("id"), Id);
				uint64 Entity = 0;
				if (!ReadUInt64(F, TEXT("id"), Entity, OutError))
					return false;
				E.Affected.Add(Entity);
			}
			State.Events.Add(E);
		}
		if (State.LastFixedStepId != Document.Economy.FixedStepId ||
		    !FCityDisasterSimulation::ValidateState(State, Document.Economy.FixedStepId, OutError))
			return false;
	}
	{
		const TSharedPtr<FJsonObject>* H = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Patients = nullptr;
		if (!Root->TryGetObjectField(TEXT("health"), H))
		{
			OutError = TEXT("Missing health state");
			return false;
		}
		if (!ReadInt64(*H, TEXT("step"), Document.Health.LastStep, OutError) ||
		    !ReadInt64(*H, TEXT("introduced"), Document.Health.IntroductionStep, OutError) ||
		    !ReadInt64(*H, TEXT("checked"), Document.Health.LastCheckStep, OutError) ||
		    !ReadInt64(*H, TEXT("recovered"), Document.Health.TotalRecovered, OutError) ||
		    !(*H)->TryGetArrayField(TEXT("patients"), Patients) || Patients->Num() > 100000)
			return false;
		for (const auto& Entry : *Patients)
		{
			const auto V = Entry->AsObject();
			FCityHealthPatient P;
			int32 Illness = 0, Care = 0;
			if (!ReadUInt64(V, TEXT("citizen"), P.CitizenId, OutError) ||
			    !ReadUInt64(V, TEXT("hospital"), P.HospitalId, OutError) ||
			    !ReadInt32(V, TEXT("illness"), Illness, OutError) || Illness < 0 || Illness > 1 ||
			    !ReadInt32(V, TEXT("care"), Care, OutError) || Care < 0 || Care > (SourceSchema >= FCitySaveDocument::PreAiSchemaVersion ? 6 : 4) ||
			    !ReadInt32(V, TEXT("risk"), P.ExposureRisk, OutError) ||
			    !ReadInt64(V, TEXT("onset"), P.OnsetStep, OutError) ||
			    !ReadInt64(V, TEXT("treatment"), P.TreatmentSteps, OutError) ||
			    !ReadInt64(V, TEXT("treatedAt"), P.LastTreatmentStep, OutError))
				return false;
			P.Illness = static_cast<ECityIllness>(Illness);
			P.Care = static_cast<ECityHealthCare>(Care);
			if (SourceSchema < FCitySaveDocument::PreAiSchemaVersion && P.Illness == ECityIllness::Mild)
			{
				// Validate old references/progress before conversion can clear them.
				if ((P.HospitalId == 0) != (P.Care == ECityHealthCare::None) ||
				    (P.HospitalId && !Document.Placement.Entities.ContainsByPredicate([&](const auto& E) {
					    return E.EntityId == P.HospitalId && E.DefinitionId == TEXT("Hospital"); })))
				{ OutError=TEXT("Invalid preceding-city care reference");return false; }
				FCityHealthSnapshot Check;Check.LastStep=Document.Health.LastStep;
				auto Checked=P;
				if (Checked.Care==ECityHealthCare::HomeRecovery) Checked.HospitalId=0;
				else if (Checked.Care==ECityHealthCare::Hospital) Checked.Care=ECityHealthCare::ReturningHome;
				Check.Patients.Add(Checked);
				if (!FCityHealthSimulation::ValidateState(Check,OutError)) return false;
				// Old remote care never visited a clinic: retain the illness and require a real visit.
				if (P.Care == ECityHealthCare::HomeRecovery) { P.Care=ECityHealthCare::None;P.HospitalId=0;P.TreatmentSteps=0; }
				// An old admitted mild patient already received care; return home with that course.
				else if (P.Care == ECityHealthCare::Hospital) P.Care=ECityHealthCare::ReturningHome;
			}
			Document.Health.Patients.Add(P);
		}

		if (!FCityHealthSimulation::ValidateState(Document.Health, OutError))
			return false;
		if (Document.Health.LastStep > Document.Economy.FixedStepId)
		{
			OutError = TEXT("Health clock exceeds economy clock");
			return false;
		}
		for (const auto& P : Document.Health.Patients)
		{
			if (!Document.Population.Citizens.ContainsByPredicate(
			        [&](const auto& C) { return C.Id == P.CitizenId; }))
			{
				OutError = TEXT("Unknown health citizen");
				return false;
			}
			if (P.HospitalId && !Document.Placement.Entities.ContainsByPredicate([&](const auto& E) {
				    return E.EntityId == P.HospitalId && E.DefinitionId == TEXT("Hospital");
			    }))
			{
				OutError = TEXT("Unknown health hospital");
				return false;
			}
		}
	}
	{
		const TSharedPtr<FJsonObject>* W = nullptr;
		if (!Root->TryGetObjectField(TEXT("waste"), W) ||
		    !FCityWastePersistence::Read(*W, Document.Waste, Document.Economy.FixedStepId, OutError,
		                                 Dimensions))
			return false;
	}
	if (!Document.Economy.Policies.Validate(Document.Economy.LastBilledMonthId, OutError))
		return false;
	for (const auto& C : Document.Population.Citizens)
		if (C.LastPolicyGrant < 0 || C.LastPolicyGrant > 7 || C.LastPolicyGrantMonth < 0 ||
		    C.LastPolicyGrantMonth > Document.Economy.Policies.LastSettledMonth ||
		    (C.LastPolicyGrantMonth == 0 && C.LastPolicyGrant))
		{
			OutError = TEXT("Invalid citizen policy grant");
			return false;
		}
	if (!Document.Economy.Loan.Validate(Document.Economy.LastBilledMonthId, OutError))
		return false;
	if ((!FCityHousingMigration::Validate(Document.Population, OutError) ||
	     Document.Population.LastMigrationMonth > Document.Economy.LastBilledMonthId))
	{
		if (OutError.IsEmpty())
			OutError = TEXT("Future housing migration clock");
		return false;
	}

	if (!Document.ValidateDimensions(OutError))
		return false;
	OutDocument = MoveTemp(Document);
	if (OutFailure)
		*OutFailure = ECitySaveReadFailure::None;
	return true;
}

bool FCityPersistence::WriteAtomic(const FString& SavePath, const FString& Json, FString& OutError,
                                   const bool bSimulateInterruptionBeforeReplace)
{
	IFileManager& Files = IFileManager::Get();
	if (!Files.MakeDirectory(*FPaths::GetPath(SavePath), true))
	{
		OutError = TEXT("Could not create the local save directory");
		return false;
	}
	const FString TemporaryPath = SavePath + TEXT(".tmp");
	const FString BackupPath = SavePath + TEXT(".bak");
	if (!FFileHelper::SaveStringToFile(Json, *TemporaryPath,
	                                   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = TEXT("Could not write the temporary city save");
		return false;
	}
	if (bSimulateInterruptionBeforeReplace)
	{
		OutError = TEXT("Simulated interruption before atomic replacement");
		return false;
	}
	if (Files.FileExists(*SavePath) && Files.Copy(*BackupPath, *SavePath, true, true) != COPY_OK)
	{
		OutError = TEXT("Could not retain the last-known-good save backup");
		return false;
	}
	if (!Files.Move(*SavePath, *TemporaryPath, true, true, false, true))
	{
		OutError = TEXT("Could not atomically replace the city save");
		return false;
	}
	return true;
}

bool FCityPersistence::ReadFile(const FString& SavePath, FString& OutJson, FString& OutError)
{
	if (!FFileHelper::LoadFileToString(OutJson, *SavePath))
	{
		OutError = TEXT("No readable local city save was found");
		return false;
	}
	return true;
}

FString FCityPersistence::GetDefaultSavePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("CitySaveV1.json"));
}
