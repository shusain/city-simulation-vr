#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCivicBudgetTest, "CitySim.Progression.CivicBudgetRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCivicBudgetTest::RunTest(const FString& Parameters)
{
	// Provisional diagnostic target: affordable operational safety/secondary education
	// within three years, followed by a funded road-outage recovery. No cash grants.
	FString Report =
	    TEXT("seed,route,month,residents,cash,income,expenses,net,payroll,power_gap,water_gap,schools,secondary\n");
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	for (int32 Seed : {7429, 1337, 2026})
		for (int32 ExpansionMonth : {6, 12})
		{
			UWorld* World =
			    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
			auto* Catalog = LoadObject<UCityBuildingCatalog>(
			    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
			if (!TestNotNull(TEXT("Authored civic catalog"), Catalog))
			{
				World->DestroyWorld(false);
				continue;
			}
			Sim->RegisterBuildingCatalog(Catalog);
			FCityIdentity Identity;
			Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
			Identity.Name = TEXT("Civic budget regression");
			Identity.DisasterFrequency = ECityDisasterFrequency::Off;
			FCityTerrainGenerationParameters Terrain;
			Terrain.Seed = Seed;
			const FString Directory =
			    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CivicBudget-Civic"), Identity.Id);
			FString Error;
			if (!TestTrue(TEXT("Fresh Normal civic city"), Sim->CreateCity(Identity, Terrain, Directory, Error)))
			{
				AddError(Error);
				World->DestroyWorld(false);
				continue;
			}
			auto Place = [&](FName Name, FCityCellCoord Anchor) -> uint64
			{
				const auto* Def = Catalog->FindDefinition(Name);
				if (!Def)
					return 0;
				auto& City = Sim->GetPlacementModel();
				auto Preview = City.BeginRoadFacingPreview(*Def, Anchor);
				if (!City.ValidatePlacement(Preview, *Def).IsSuccess())
					return 0;
				return City.CommitPlacement(Preview, *Def).EntityId;
			};
			auto Road = [&](FCityCellCoord Start, FCityCellCoord End)
			{
				auto& City = Sim->GetPlacementModel();
				auto Preview = City.BeginRoadPreview(Start);
				Preview.End = End;
				return City.CommitRoad(Preview).IsSuccess();
			};
			TestTrue(TEXT("Starter road"), Road({0, 8}, {10, 8}));
			TestTrue(TEXT("Purchased starter power"), Place(TEXT("Power"), {0, 9}) > 0);
			TestTrue(TEXT("Purchased starter water"), Place(TEXT("Service"), {2, 9}) > 0);
			auto Zone = [&](FCityCellCoord Anchor, FIntPoint Size, ECityZoneType Type)
			{ return Sim->GetPlacementModel().SetZoneArea(Anchor, Size, Type); };
			TestTrue(TEXT("Starter homes"), Zone({4, 9}, {6, 2}, ECityZoneType::Residential));
			TestTrue(TEXT("Starter shops"), Zone({0, 6}, {4, 2}, ECityZoneType::Commercial));
			TestTrue(TEXT("Starter industry"), Zone({4, 6}, {6, 2}, ECityZoneType::Industrial));
			const auto Tuning = Sim->GetEconomyTuning();
			Sim->SetSimulationPaused(false);
			uint64 School = 0, Police = 0, Fire = 0, Primary = 0;
			TSet<uint64> PrimaryStudents;
			int32 SchoolMonth = 0, PoliceMonth = 0, FireMonth = 0;
			int64 MinimumCash = 10000;
			const FString LearningPath = Directory / TEXT("learning.json");
			bool bExpanded = false;
			int32 NextPurchase = 0;
			const FName Purchases[] = {TEXT("Power"), TEXT("Service"),         TEXT("Police"),
			                           TEXT("Fire"),  TEXT("SecondarySchool"), TEXT("PrimarySchool")};
			auto AdvanceMonth = [&]()
			{
				for (int32 Step = 0; Step < Tuning.FixedStepsPerMonth; ++Step)
					Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
			};
			for (int32 Month = 1; Month <= 36; ++Month)
			{
				if (Month == ExpansionMonth)
				{
					// A shared explicit player policy; tax effects on demand remain enabled.
					TestTrue(TEXT("Growth policy applies"), Sim->SetTaxRates(2000, 1500, 1500));
					bExpanded = Road({10, 8}, {10, 14}) && Road({0, 14}, {20, 14}) && Road({10, 8}, {40, 8});
					TestTrue(TEXT("Terrain-valid expansion roads"), bExpanded);
					// A mixed-density city needs actual low-density land, rather than
					// relying on every starter lot upgrading. Buy the supporting utilities.
					for (FName Name : {FName(TEXT("Power")), FName(TEXT("Service"))})
					{
						uint64 Utility = 0;
						for (int32 X = 18; X <= 38 && !Utility; ++X)
							Utility = Place(Name, {X, 9});
						TestTrue(TEXT("Earned treasury buys mixed-density district utilities"), Utility > 0);
					}
					TestTrue(TEXT("Mixed-density residential land"),
					         Zone({12, 9}, {28, 2}, ECityZoneType::Residential));
					TestTrue(TEXT("Mixed-density commercial land"), Zone({10, 6}, {14, 2}, ECityZoneType::Commercial));
					TestTrue(TEXT("Mixed-density industrial land"), Zone({24, 6}, {16, 2}, ECityZoneType::Industrial));
				}
				if (bExpanded && NextPurchase < UE_ARRAY_COUNT(Purchases))
				{
					const auto* Def = Catalog->FindDefinition(Purchases[NextPurchase]);
					if (TestNotNull(TEXT("Civic purchase definition"), Def) &&
					    Sim->GetPlacementModel().GetFunds() >= Def->Cost + 1000)
					{
						uint64 Id = 0;
						// Search real roadside terrain; do not flatten hills or force occupancy.
						if (NextPurchase == 5)
						{
							for (int32 X = 19; X <= 20 && !Id; ++X)
								Id = Place(Purchases[NextPurchase], {X, 12});
						}
						else
							for (int32 Y = 15; Y <= 16 && !Id; ++Y)
								for (int32 X = 1; X <= 18 && !Id; ++X)
									Id = Place(Purchases[NextPurchase], {X, Y});
						if (Id)
						{
							if (NextPurchase == 2)
								Police = Id;
							if (NextPurchase == 3)
								Fire = Id;
							if (NextPurchase == 4)
								School = Id;
							if (NextPurchase == 5)
								Primary = Id;
							AddInfo(FString::Printf(TEXT("Seed %d route %d buys %s month %d cash %lld"), Seed,
							                        ExpansionMonth, *Purchases[NextPurchase].ToString(), Month,
							                        Sim->GetPlacementModel().GetFunds()));
							++NextPurchase;
						}
					}
				}
				// Grow on the opposite side of the new road, retaining civic lots above it.
				if (bExpanded && NextPurchase == UE_ARRAY_COUNT(Purchases))
					for (int32 X = 1; X < 19; X += 2)
						Zone({X, 12}, {2, 2},
						     X < 11   ? ECityZoneType::Residential
						     : X < 15 ? ECityZoneType::Commercial
						              : ECityZoneType::Industrial);
				AdvanceMonth();
				const auto Budget = Sim->GetProjectedBudget();
				const auto& Services = Sim->GetServiceSnapshot();
				const auto& Needs = Sim->GetNeedsSnapshot();
				MinimumCash = FMath::Min(MinimumCash, Sim->GetPlacementModel().GetFunds());
				if (Primary && Services.SchoolUsedCapacity.FindRef(Primary) > 0)
					for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
						if (C.Education == ECityEducationLevel::None && C.EducationMonths > 0)
							PrimaryStudents.Add(C.Id);
				if (!SchoolMonth && Services.OperationalSchools > 0)
					SchoolMonth = Month;
				if (!PoliceMonth && Police && Sim->GetCrimeSnapshot().OperationalStations > 0)
					PoliceMonth = Month;
				if (!FireMonth && Sim->GetFireSnapshot().OperationalStations > 0)
					FireMonth = Month;
				if (SchoolMonth > 0 && Month == SchoolMonth + 2)
					TestTrue(TEXT("Funded city saves during an active course"),
					         Sim->SaveCityToPath(LearningPath, Error));
				Report += FString::Printf(
				    TEXT("%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%d,%d,%d,%d\n"), Seed, ExpansionMonth, Month,
				    Sim->GetPopulationSnapshot().Citizens.Num(), Sim->GetPlacementModel().GetFunds(), Budget.Income,
				    Budget.Expenses, Budget.Net(), Budget.PublicPayroll, Needs.PowerShortage,
				    Needs.UnservedServiceDemand, Services.OperationalSchools, Services.SecondaryResidents);
			}
			TestTrue(TEXT("Civic purchases affordable within three years"), School > 0 && Police > 0 && Fire > 0);
			TestTrue(TEXT("Operational safety and school within three years"),
			         SchoolMonth > 0 && PoliceMonth > 0 && FireMonth > 0);
			TestTrue(TEXT("Civic route avoids exhausted treasury"), MinimumCash > 0);
			TestTrue(TEXT("Primary school teaches actual uneducated arrivals"),
			         Primary > 0 && PrimaryStudents.Num() > 0);
			int32 PrimaryGraduates = 0;
			for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
				if (PrimaryStudents.Contains(C.Id) && C.Education >= ECityEducationLevel::Basic)
					++PrimaryGraduates;
			TestTrue(TEXT("Real Primary students complete the default course"), PrimaryGraduates > 0);
			AddInfo(FString::Printf(TEXT("Primary seed %d route %d: students %d graduates %d"), Seed, ExpansionMonth,
			                        PrimaryStudents.Num(), PrimaryGraduates));
			TestTrue(TEXT("Secondary qualifications emerge without fixture grants"),
			         Sim->GetServiceSnapshot().SecondaryResidents > 0);
			const int64 YearThreeFunds = Sim->GetPlacementModel().GetFunds();
			const int32 YearThreeResidents = Sim->GetPopulationSnapshot().Citizens.Num(),
			            YearThreeSecondary = Sim->GetServiceSnapshot().SecondaryResidents;
			const FString StablePath = Directory / TEXT("stable.json");
			TestTrue(TEXT("Profitable civic city saves"), Sim->SaveCityToPath(StablePath, Error));
			auto AdvanceDays = [&](int32 Days, int32 Speed)
			{
				Sim->SetSimulationSpeed(Speed);
				Sim->SetSimulationPaused(false);
				for (int32 Remaining = Days; Remaining > 0;)
				{
					const int32 Count = FMath::Min(Remaining, Speed);
					Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds * Count / Speed + 0.00001));
					Remaining -= Count;
				}
			};
			auto CheckReports = [&](const FCityReportHistory& Expected)
			{
				const auto& Actual = Sim->GetReportHistory();
				TestTrue(TEXT("Settled history validates"),
				         Actual.Validate(Sim->GetEconomySnapshot().LastBilledMonthId, Error));
				auto Compare = [&](const TArray<FCityReportSample>& A, const TArray<FCityReportSample>& B)
				{
					if (!TestEqual(TEXT("Same observed report samples"), A.Num(), B.Num()))
						return;
					for (int32 I = 0; I < A.Num(); ++I)
					{
						TestTrue(TEXT("Report periods and safety availability survive replay"),
						         A[I].MonthId == B[I].MonthId && A[I].ObservedMonths == B[I].ObservedMonths &&
						             A[I].bHasSafety == B[I].bHasSafety);
						for (int32 Metric = 0; Metric < static_cast<int32>(ECityReportMetric::Count); ++Metric)
							TestEqual(TEXT("Every settled report metric agrees across speeds"), A[I].Values[Metric],
							          B[I].Values[Metric]);
					}
				};
				Compare(Actual.Months, Expected.Months);
				Compare(Actual.Annual(), Expected.Annual());
			};
			// Education: actual graduates enter authored skilled roles and receive funded payroll.
			int32 Skilled = 0;
			int64 PrivatePaid = 0, PrivateDue = 0, PublicPaid = 0;
			const auto& Employed = Sim->GetPopulationSnapshot();
			for (const auto& Business : Employed.Businesses)
			{
				int32 FilledSkilled = 0;
				int64 Due = 0;
				for (const auto& Citizen : Employed.Citizens)
					if (Citizen.WorkplaceEntityId == Business.EntityId)
					{
						if (Citizen.bSkilledJob)
						{
							++FilledSkilled;
							++Skilled;
							TestTrue(TEXT("Actual skilled hire is Secondary-qualified"),
							         Citizen.Education >= ECityEducationLevel::Secondary);
						}
						Due += Tuning.HouseholdIncomePerEmployedResident * (Citizen.bSkilledJob ? 2 : 1);
					}
				TestTrue(TEXT("Real skilled hiring respects authored capacity"),
				         FilledSkilled <= Business.SkilledJobCapacity);
				TestEqual(TEXT("Real private payroll owes doubled skilled salary"), Business.PayrollDue, Due);
				TestTrue(TEXT("Actual private wages are funded and cash-limited"),
				         Business.PayrollPaid >= 0 && Business.PayrollPaid <= Business.PayrollDue);
				PrivatePaid += Business.PayrollPaid;
				PrivateDue += Business.PayrollDue;
			}
			for (const auto& Employer : Employed.PublicEmployers)
				PublicPaid += Employer.PayrollPaid;
			TestTrue(TEXT("Real school graduates fill skilled jobs and receive wages"), Skilled > 0 && PrivatePaid > 0);
			TestEqual(TEXT("Treasury payroll reconciles to public ledgers"), PublicPaid,
			          Sim->GetEconomySnapshot().LastPublicPayrollPaid);
			const auto& Settled = Sim->GetReportHistory().Months.Last();
			TestEqual(TEXT("Reports reconcile actual paid public/private salaries"),
			          Settled.Get(ECityReportMetric::WagesPaid), PublicPaid + PrivatePaid);
			AddInfo(FString::Printf(
			    TEXT("Skilled wage seed %d route %d: skilled %d, private due/paid %lld/%lld, public paid %lld"), Seed,
			    ExpansionMonth, Skilled, PrivateDue, PrivatePaid, PublicPaid));
			// CivicBudget: taxes can intentionally exhaust treasury; recovery must use revenue.
			TestTrue(TEXT("Deliberate zero-tax stress applies"), Sim->SetTaxRates(0, 0, 0));
			int32 StressMonths = 0;
			for (; StressMonths < 96 && !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall; ++StressMonths)
				AdvanceDays(Tuning.FixedStepsPerMonth, 1);
			TestTrue(TEXT("Zero revenue eventually creates actual unpaid bills"),
			         Sim->GetEconomySnapshot().bGrowthBlockedByShortfall);
			TestTrue(TEXT("Treasury pressure reduces actual public pay"),
			         Sim->GetEconomySnapshot().LastPublicPayrollPaid < Sim->GetEconomySnapshot().LastPublicPayrollDue);
			const FString StressPath = Directory / TEXT("payroll.json");
			TestTrue(TEXT("Actual payroll shortfall saves"), Sim->SaveCityToPath(StressPath, Error));
			int64 RestoredCash = 0;
			FCityReportHistory RestoredReports;
			int32 RecoveryMonths = 0;
			for (int32 Speed : {1, 3, 5})
			{
				TestTrue(TEXT("Payroll shortfall reloads"), Sim->LoadCityFromPath(StressPath, Error, false));
				TestTrue(TEXT("Player recovery tax policy applies"), Sim->SetTaxRates(2500, 2500, 2500));
				TestTrue(TEXT("Tax edit cannot backdate bill payment"),
				         Sim->GetEconomySnapshot().bGrowthBlockedByShortfall);
				int32 PaidMonth = 0;
				for (int32 Month = 1; Month <= 6; ++Month)
				{
					AdvanceDays(Tuning.FixedStepsPerMonth, Speed);
					if (!PaidMonth && !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall)
						PaidMonth = Month;
				}
				TestTrue(TEXT("Revenue restores fully paid bills within six months"), PaidMonth > 0);
				TestEqual(TEXT("Recovered public payroll is fully funded"),
				          Sim->GetEconomySnapshot().LastPublicPayrollPaid,
				          Sim->GetEconomySnapshot().LastPublicPayrollDue);
				TestTrue(TEXT("Paid safety departments recover capacity"),
				         Sim->GetCrimeSnapshot().OperationalStations > 0 &&
				             Sim->GetFireSnapshot().OperationalStations > 0);
				if (Speed == 1)
				{
					RestoredCash = Sim->GetPlacementModel().GetFunds();
					RestoredReports = Sim->GetReportHistory();
					RecoveryMonths = PaidMonth;
				}
				else
				{
					TestEqual(TEXT("Tax recovery treasury agrees across speeds"), Sim->GetPlacementModel().GetFunds(),
					          RestoredCash);
					TestEqual(TEXT("Tax recovery deadline agrees across speeds"), PaidMonth, RecoveryMonths);
					CheckReports(RestoredReports);
				}
			}
			AddInfo(FString::Printf(
			    TEXT(
			        "Payroll seed %d route %d: zero-tax shortfall after %d months; paid recovery month %d, six-month cash %lld"),
			    Seed, ExpansionMonth, StressMonths, RecoveryMonths, RestoredCash));
			// FacilityUpgrades: real utility expansion confirmation, pending-save and fast completion.
			TestTrue(TEXT("Stable city reloads for utility checks"), Sim->LoadCityFromPath(StablePath, Error, false));
			uint64 StarterPower = 0, StarterWater = 0;
			for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
				if (Pair.Value.Anchor.Y == 9)
				{
					if (Pair.Value.DefinitionId == TEXT("Power"))
						StarterPower = Pair.Key;
					if (Pair.Value.DefinitionId == TEXT("Service"))
						StarterWater = Pair.Key;
				}
			const int64 UpgradeFunds = Sim->GetPlacementModel().GetFunds();
			TestTrue(TEXT("Real power upgrade confirmation succeeds"),
			         Sim->PurchaseStationUpgrade(StarterPower, ECityDensityLevel::Low));
			TestTrue(TEXT("Real water upgrade confirmation succeeds"),
			         Sim->PurchaseStationUpgrade(StarterWater, ECityDensityLevel::Low));
			TestTrue(TEXT("Both expansions charge treasury"), Sim->GetPlacementModel().GetFunds() < UpgradeFunds);
			TestTrue(TEXT("Pending upgrades retain starter stages"),
			         Sim->GetDevelopmentSnapshot().Find(StarterPower)->DensityLevel == ECityDensityLevel::Low &&
			             Sim->GetDevelopmentSnapshot().Find(StarterWater)->DensityLevel == ECityDensityLevel::Low);
			const FString UpgradePath = Directory / TEXT("utilities.json");
			TestTrue(TEXT("Both pending expansions save"), Sim->SaveCityToPath(UpgradePath, Error));
			int64 ExpandedCash = 0;
			int32 ExpandedPower = 0, ExpandedWater = 0;
			FCityReportHistory ExpandedReports;
			for (int32 Speed : {1, 3, 5})
			{
				TestTrue(TEXT("Pending facilities reload"), Sim->LoadCityFromPath(UpgradePath, Error, false));
				AdvanceDays(2 * Tuning.FixedStepsPerMonth, Speed);
				TestTrue(TEXT("Both saved expansions finish at Medium"),
				         Sim->GetDevelopmentSnapshot().Find(StarterPower)->DensityLevel == ECityDensityLevel::Medium &&
				             Sim->GetDevelopmentSnapshot().Find(StarterWater)->DensityLevel ==
				                 ECityDensityLevel::Medium);
				if (Speed == 1)
				{
					ExpandedCash = Sim->GetPlacementModel().GetFunds();
					ExpandedPower = Sim->GetServiceSnapshot().PowerCapacity;
					ExpandedWater = Sim->GetServiceSnapshot().ServiceCapacity;
					ExpandedReports = Sim->GetReportHistory();
				}
				else
				{
					TestEqual(TEXT("Expanded treasury matches fast resume"), Sim->GetPlacementModel().GetFunds(),
					          ExpandedCash);
					TestEqual(TEXT("Expanded power matches fast resume"), Sim->GetServiceSnapshot().PowerCapacity,
					          ExpandedPower);
					TestEqual(TEXT("Expanded water matches fast resume"), Sim->GetServiceSnapshot().ServiceCapacity,
					          ExpandedWater);
					CheckReports(ExpandedReports);
				}
			}
			// NaturalDisasters/FacilityUpgrades: real upgraded utilities survive event save/resume and paid repair.
			if (Seed == 7429 && ExpansionMonth == 12)
			{
				const FString ExpandedPath = Directory / TEXT("expanded.json");
				TestTrue(TEXT("Completed utility city saves for disaster checks"),
				         Sim->SaveCityToPath(ExpandedPath, Error));
				for (const auto Kind : {ECityNaturalDisaster::Tornado, ECityNaturalDisaster::Earthquake})
				{
					TestTrue(TEXT("Upgraded city reloads for natural damage"),
					         Sim->LoadCityFromPath(ExpandedPath, Error, false));
					TestTrue(TEXT("Real city disaster test starts"), Sim->StartDisasterTest(Kind));
					AdvanceDays(1, 5);
					TestEqual(TEXT("Disaster onset automatically selects 1x"), Sim->GetSimulationSpeed(), 1);
					const auto Event = Sim->GetDisasterSnapshot().Events.Last();
					TArray<uint64> Utilities;
					for (uint64 Id : Event.PlannedTargets)
					{
						const auto& Entity = Sim->GetPlacementModel().GetEntities().FindChecked(Id);
						if (Entity.DefinitionId == TEXT("Power") || Entity.DefinitionId == TEXT("Service"))
							Utilities.Add(Id);
					}
					TestTrue(TEXT("This seeded disaster plan reaches a real utility"), !Utilities.IsEmpty());
					const FString EventPath = Directory / TEXT("disaster.json");
					TestTrue(TEXT("Active upgraded-utility event saves"), Sim->SaveCityToPath(EventPath, Error));
					int64 RepairedCash = 0;
					FCityReportHistory RepairedReports;
					for (int32 Speed : {1, 3, 5})
					{
						TestTrue(TEXT("Active natural event resumes"), Sim->LoadCityFromPath(EventPath, Error, false));
						AdvanceDays(static_cast<int32>(Event.EndStep - Sim->GetEconomySnapshot().FixedStepId), Speed);
						TestFalse(TEXT("Saved event ends at its deadline"),
						          Sim->GetDisasterSnapshot().Current() != nullptr);
						for (uint64 Id : Utilities)
						{
							TestTrue(TEXT("Actual natural event disables planned utility"),
							         Sim->GetFireSnapshot().IsDisabled(Id));
							TestEqual(TEXT("Actual natural cause is retained"),
							          Sim->GetFireSnapshot().DamagedBuildings.FindRef(Id).Cause,
							          static_cast<int32>(Kind));
						}
						const int32 DisabledWater = Sim->GetServiceSnapshot().OperationalServiceBuildings;
						TArray<uint64> Damaged;
						Sim->GetFireSnapshot().DamagedBuildings.GetKeys(Damaged);
						Damaged.Sort();
						const int64 BeforeRepair = Sim->GetPlacementModel().GetFunds();
						int64 RepairBill = 0, Deadline = Sim->GetEconomySnapshot().FixedStepId;
						for (uint64 Id : Damaged)
						{
							// The event can end while an unrelated ordinary fire is still
							// burning. Its repair must remain blocked without charging.
							if (Sim->GetFireSnapshot().IsBurning(Id))
							{
								const int64 BurningFunds = Sim->GetPlacementModel().GetFunds();
								TestFalse(TEXT("An active fire blocks structural repair"), Sim->RepairBuilding(Id));
								TestEqual(TEXT("Blocked burning repair does not charge treasury"),
								          Sim->GetPlacementModel().GetFunds(), BurningFunds);
								continue;
							}
							RepairBill += Sim->GetRepairCost(Id);
							TestTrue(TEXT("Real city funds natural-damage repair"), Sim->RepairBuilding(Id));
							Deadline = FMath::Max(
							    Deadline, Sim->GetFireSnapshot().DamagedBuildings.FindRef(Id).RepairCompleteStep);
						}
						TestEqual(TEXT("Repairs debit exact quoted costs once"),
						          BeforeRepair - Sim->GetPlacementModel().GetFunds(), RepairBill);
						AdvanceDays(static_cast<int32>(Deadline - Sim->GetEconomySnapshot().FixedStepId) +
						                Tuning.FixedStepsPerMonth,
						            Speed);
						for (uint64 Id : Utilities)
							TestFalse(TEXT("Paid repair restores affected utility"),
							          Sim->GetFireSnapshot().IsDisabled(Id));
						TestTrue(TEXT("Repair restores lost water providers"),
						         Sim->GetServiceSnapshot().OperationalServiceBuildings > DisabledWater);
						TestTrue(TEXT("Natural damage keeps completed utility upgrades"),
						         Sim->GetDevelopmentSnapshot().Find(StarterPower)->DensityLevel ==
						                 ECityDensityLevel::Medium &&
						             Sim->GetDevelopmentSnapshot().Find(StarterWater)->DensityLevel ==
						                 ECityDensityLevel::Medium);
						if (Speed == 1)
						{
							RepairedCash = Sim->GetPlacementModel().GetFunds();
							RepairedReports = Sim->GetReportHistory();
						}
						else
						{
							TestEqual(TEXT("Disaster repair treasury matches all speeds"),
							          Sim->GetPlacementModel().GetFunds(), RepairedCash);
							CheckReports(RepairedReports);
						}
					}
					AddInfo(FString::Printf(
					    TEXT(
					        "Real %s: severity %d%%, planned utilities %d, repaired treasury %lld; saved upgrades/report replay retained"),
					    FCityDisasterSimulation::Name(Kind), Event.SeverityPercent, Utilities.Num(), RepairedCash));
				}
			}
			// College/FacilityUpgrades: an affordable college course must unlock actual qualified utility staff.
			if (Seed == 7429 && ExpansionMonth == 12)
			{
				TestTrue(TEXT("Stable city reloads for higher education"),
				         Sim->LoadCityFromPath(StablePath, Error, false));
				const uint64 College = Place(TEXT("College"), {14, 15});
				if (TestTrue(TEXT("Normal treasury purchases college on real terrain"), College > 0))
				{
					AdvanceDays(3 * Tuning.FixedStepsPerMonth, 1);
					TestTrue(TEXT("College is operational with real eligible students"),
					         Sim->GetServiceSnapshot().SchoolUsedCapacity.FindRef(College) > 0);
					TestEqual(
					    TEXT("Partial default course has not granted Higher"),
					    Sim->GetPopulationSnapshot()
					        .Citizens
					        .FilterByPredicate([](const auto& C) { return C.Education == ECityEducationLevel::Higher; })
					        .Num(),
					    0);
					const uint64 Nuclear = Place(TEXT("NuclearPower"), {17, 15});
					if (TestTrue(TEXT("Normal earned treasury purchases nuclear option"), Nuclear > 0))
					{
						AdvanceDays(Tuning.FixedStepsPerMonth, 1);
						TestEqual(TEXT("Secondary staff cannot fill nuclear Higher-qualified roles"),
						          Sim->GetPopulationSnapshot().Workers(Nuclear), 0);
						TestEqual(TEXT("Nuclear catalog keeps Higher requirement"),
						          Sim->GetServiceSnapshot().PublicMinimumEducation.FindRef(Nuclear), 3);
						const FString HigherPath = Directory / TEXT("higher.json");
						TestTrue(TEXT("Partial college course and unstaffed nuclear city save"),
						         Sim->SaveCityToPath(HigherPath, Error));
						int64 HigherCash = 0;
						int32 HigherResidents = 0;
						FCityReportHistory HigherReports;
						for (int32 Speed : {1, 3, 5})
						{
							TestTrue(TEXT("Partial college and nuclear staffing resume"),
							         Sim->LoadCityFromPath(HigherPath, Error, false));
							AdvanceDays(6 * Tuning.FixedStepsPerMonth, Speed);
							// Ordinary fires remain enabled. A changed growth/arrival
							// sequence can burn this facility during the real course.
							// Restore it with earned funds before testing qualifications.
							if (Sim->GetFireSnapshot().IsDisabled(Nuclear))
							{
								for (int32 Day = 0;
								     Day < 2 * Tuning.FixedStepsPerMonth && Sim->GetFireSnapshot().IsBurning(Nuclear);
								     ++Day)
									AdvanceDays(1, Speed);
								TestFalse(TEXT("Nuclear fire finishes before repair"),
								          Sim->GetFireSnapshot().IsBurning(Nuclear));
								const int64 Cost = Sim->GetRepairCost(Nuclear);
								const int64 BeforeRepair = Sim->GetPlacementModel().GetFunds();
								TestTrue(TEXT("Earned college city pays to repair damaged nuclear facility"),
								         Sim->RepairBuilding(Nuclear));
								TestEqual(TEXT("Nuclear repair charges its exact quote"),
								          BeforeRepair - Sim->GetPlacementModel().GetFunds(), Cost);
								const int64 Deadline =
								    Sim->GetFireSnapshot().DamagedBuildings.FindRef(Nuclear).RepairCompleteStep;
								AdvanceDays(static_cast<int32>(FMath::Max<int64>(
								                0, Deadline - Sim->GetEconomySnapshot().FixedStepId)) +
								                1,
								            Speed);
								TestFalse(TEXT("Paid nuclear repair restores the facility"),
								          Sim->GetFireSnapshot().IsDisabled(Nuclear));
							}
							const int32 Graduates =
							    Sim->GetPopulationSnapshot()
							        .Citizens
							        .FilterByPredicate([](const auto& C)
							                           { return C.Education == ECityEducationLevel::Higher; })
							        .Num();
							TestTrue(TEXT("Real default college produces Higher graduates"), Graduates > 0);
							AddInfo(FString::Printf(
							    TEXT("Advanced staffing: graduates=%d nuclear=%s power=%d/%d water=%d/%d"), Graduates,
							    *FCityServiceEvaluator::FailureToString(Sim->GetServiceSnapshot().GetFailure(Nuclear)),
							    Sim->GetServiceSnapshot().PowerDemand, Sim->GetServiceSnapshot().PowerCapacity,
							    Sim->GetServiceSnapshot().ServiceDemand, Sim->GetServiceSnapshot().ServiceCapacity));
							TestTrue(TEXT("College graduates fill nuclear roles"),
							         Sim->GetPopulationSnapshot().Workers(Nuclear) > 0);
							for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
								if (C.WorkplaceEntityId == Nuclear)
									TestEqual(TEXT("Each nuclear hire is actually Higher-qualified"), C.Education,
									          ECityEducationLevel::Higher);
							TestTrue(TEXT("Affordable advanced city has positive cash and paid bills"),
							         Sim->GetPlacementModel().GetFunds() > 0 &&
							             !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall);
							if (Speed == 1)
							{
								HigherCash = Sim->GetPlacementModel().GetFunds();
								HigherResidents = Graduates;
								HigherReports = Sim->GetReportHistory();
							}
							else
							{
								TestEqual(TEXT("Higher progression cash agrees across speeds"),
								          Sim->GetPlacementModel().GetFunds(), HigherCash);
								TestEqual(TEXT("Higher graduation agrees across speeds"), Graduates, HigherResidents);
								CheckReports(HigherReports);
							}
						}
						AddInfo(FString::Printf(
						    TEXT(
						        "Real higher education: college %llu nuclear %llu, Higher residents %d, nuclear workers %d, treasury %lld"),
						    College, Nuclear, HigherResidents, Sim->GetPopulationSnapshot().Workers(Nuclear),
						    HigherCash));
					}
				}
			}
			// NaturalDisasters: a real road-routed police arrival, saved custody and upgraded cells.
			if (Seed == 7429 && ExpansionMonth == 12)
			{
				TestTrue(TEXT("Active-course city reloads for arson response"),
				         Sim->LoadCityFromPath(LearningPath, Error, false));
				Sim->SetSimulationSpeed(1);
				Sim->SetSimulationPaused(false);
				const auto StationAnchor = Sim->GetPlacementModel().GetEntities().FindChecked(Police).Anchor;
				TArray<uint64> Targets;
				for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
					if (Sim->GetDevelopmentSnapshot().IsOperational(Pair.Key) &&
					    (Pair.Value.DefinitionId == TEXT("Home") || Pair.Value.DefinitionId == TEXT("Shop") ||
					     Pair.Value.DefinitionId == TEXT("Factory")))
						Targets.Add(Pair.Key);
				Targets.Sort(
				    [&](uint64 A, uint64 B)
				    {
					    const auto& Entities = Sim->GetPlacementModel().GetEntities();
					    const auto CA = Entities.FindChecked(A).Anchor, CB = Entities.FindChecked(B).Anchor;
					    const int32 DA = FMath::Abs(CA.X - StationAnchor.X) + FMath::Abs(CA.Y - StationAnchor.Y),
					                DB = FMath::Abs(CB.X - StationAnchor.X) + FMath::Abs(CB.Y - StationAnchor.Y);
					    return DA == DB ? A < B : DA < DB;
				    });
				uint64 Offender = 0;
				int64 ArrestIncident = 0;
				for (int32 Attempt = 0; Attempt < Targets.Num() && !Offender; ++Attempt)
				{
					for (int32 Wait = 0; Wait < 30 && Sim->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
					                                      [&](const auto& T)
					                                      { return T.IsEmergency() && T.StationEntityId == Police; });
					     ++Wait)
						AdvanceDays(1, 1);
					if (!Sim->StartSafetyTest(Targets[Attempt], true, true))
						continue;
					const int64 Id = Sim->GetCrimeSnapshot().RecentIncidents.Last().Id;
					const auto Cell = Sim->GetPlacementModel().GetEntities().FindChecked(Targets[Attempt]).Anchor;
					AddInfo(
					    FString::Printf(TEXT("Arson attempt %d target %llu at %d,%d police %llu staff %d failure %d"),
					                    Attempt, Targets[Attempt], Cell.X, Cell.Y, Police,
					                    Sim->GetServiceSnapshot().PublicStaffPercent.FindRef(Police),
					                    static_cast<int32>(Sim->GetServiceSnapshot().GetFailure(Police))));
					for (int32 Day = 0; Day < 12 && !Offender; ++Day)
					{
						AdvanceDays(1, 1);
						const auto* Incident = Sim->GetCrimeSnapshot().RecentIncidents.FindByPredicate(
						    [&](const auto& I) { return I.Id == Id; });
						if (Incident && Incident->ArsonOutcome == ECityArsonOutcome::Detained)
						{
							Offender = Incident->OffenderCitizenId;
							ArrestIncident = Id;
						}
					}
				}
				if (TestTrue(TEXT("Real police traffic catches a test arsonist"), Offender > 0))
				{
					auto FindOffender = [&]()
					{
						return Sim->GetPopulationSnapshot().Citizens.FindByPredicate([&](const auto& C)
						                                                             { return C.Id == Offender; });
					};
					const auto Captive = *FindOffender();
					TestTrue(TEXT("Actual custody suspends work and travel"),
					         Captive.IsDetained() && !Captive.WorkplaceEntityId &&
					             Captive.TripPurpose == ECityTripPurpose::None);
					TestEqual(TEXT("Actual detention starts with four spaces"),
					          FCityCrimeSimulation::JailCapacity(Sim->GetDevelopmentSnapshot(), Police), 4);
					TestTrue(TEXT("Police expansion is affordable during custody"),
					         Sim->PurchaseStationUpgrade(Police, ECityDensityLevel::Low));
					TestEqual(TEXT("Pending expansion retains four spaces"),
					          FCityCrimeSimulation::JailCapacity(Sim->GetDevelopmentSnapshot(), Police), 4);
					const FString CustodyPath = Directory / TEXT("custody.json");
					TestTrue(TEXT("Actual prisoner and pending upgrade save"), Sim->SaveCityToPath(CustodyPath, Error));
					int64 ReleasedCash = 0;
					FCityReportHistory ReleasedReports;
					for (int32 Speed : {1, 3, 5})
					{
						TestTrue(TEXT("Actual custody and station upgrade resume"),
						         Sim->LoadCityFromPath(CustodyPath, Error, false));
						AdvanceDays(1, Speed);
						TestEqual(TEXT("Completed police expansion provides eight cells"),
						          FCityCrimeSimulation::JailCapacity(Sim->GetDevelopmentSnapshot(), Police), 8);
						const int32 Days =
						    static_cast<int32>(Captive.JailReleaseStep - Sim->GetEconomySnapshot().FixedStepId - 1);
						AdvanceDays(Days, Speed);
						const auto* C = FindOffender();
						if (TestNotNull(TEXT("Prisoner identity survives the full term"), C))
						{
							TestTrue(TEXT("No early release or reemployment"),
							         C->IsDetained() && !C->WorkplaceEntityId &&
							             C->TripPurpose == ECityTripPurpose::None);
							TestTrue(TEXT("Custody grants no learning"),
							         C->Education == Captive.Education &&
							             C->EducationMonths == Captive.EducationMonths);
							TestEqual(TEXT("Custody pauses unemployment timer"), C->UnemployedSinceStep, int64(-1));
						}
						TestTrue(TEXT("People reports contain actual month-end custody"),
						         Sim->GetReportHistory().Months.ContainsByPredicate(
						             [](const auto& M) { return M.Get(ECityReportMetric::Jailed) > 0; }));
						AdvanceDays(1, Speed);
						C = FindOffender();
						if (TestNotNull(TEXT("Released citizen remains in the city"), C))
						{
							TestFalse(TEXT("Actual deadline releases citizen"), C->IsDetained());
							TestEqual(TEXT("Actual release grants six months reform"), C->ReformedUntilStep,
							          Captive.JailReleaseStep + 6 * Tuning.FixedStepsPerMonth);
						}
						const auto* Incident = Sim->GetCrimeSnapshot().RecentIncidents.FindByPredicate(
						    [&](const auto& I) { return I.Id == ArrestIncident; });
						if (Incident)
							TestEqual(TEXT("Actual incident records scheduled release"), Incident->ArsonOutcome,
							          ECityArsonOutcome::Released);
						if (Speed == 1)
						{
							ReleasedCash = Sim->GetPlacementModel().GetFunds();
							ReleasedReports = Sim->GetReportHistory();
						}
						else
						{
							TestEqual(TEXT("Custody release treasury matches all speeds"),
							          Sim->GetPlacementModel().GetFunds(), ReleasedCash);
							CheckReports(ReleasedReports);
						}
					}
					TestTrue(TEXT("Custody reloads for station-loss branch"),
					         Sim->LoadCityFromPath(CustodyPath, Error, false));
					TestTrue(TEXT("Station demolition uses actual command"),
					         Sim->GetPlacementModel().DemolishEntity(Police).IsSuccess());
					AdvanceDays(1, 1);
					if (const auto* C = FindOffender())
						TestTrue(TEXT("Demolished station releases early without reform"),
						         !C->IsDetained() && C->ReformedUntilStep == Captive.ReformedUntilStep);
					AddInfo(FString::Printf(
					    TEXT(
					        "Real custody: offender %llu incident %lld, release day %lld, upgraded cells8, released cash %lld; 1x/3x/5x and early station loss pass"),
					    Offender, ArrestIncident, Captive.JailReleaseStep, ReleasedCash));
				}
			}
			// CivicBudget/Roads: extend onto untouched generated terrain, with local water supply.
			TestTrue(TEXT("Stable city reloads for terrain expansion"),
			         Sim->LoadCityFromPath(StablePath, Error, false));
			auto& FrontierCity = Sim->GetPlacementModel();
			auto Frontier = FrontierCity.BeginRoadPreview({20, 14});
			Frontier.End = {45, 14};
			Frontier.Profile = FrontierCity.ResolveRoadDrawingProfile(Frontier.Start, Frontier.End, false);
			if (TestTrue(TEXT("Generated-terrain frontier road is valid and affordable"),
			             FrontierCity.CommitRoad(Frontier).IsSuccess()))
			{
				uint64 Tower = 0;
				int32 TowerX = 0;
				for (int32 X = 25; X <= 44 && !Tower; ++X)
				{
					Tower = Place(TEXT("WaterTower"), {X, 15});
					if (Tower)
						TowerX = X;
				}
				TestTrue(TEXT("Untouched terrain supports a purchased local water tower"), Tower > 0);
				int32 ValidHomes = 0, ValidIndustry = 0;
				for (int32 X = 25; X <= 43; ++X)
				{
					const auto* Def = Catalog->FindDefinition(X < 35 ? TEXT("Home") : TEXT("Factory"));
					auto Preview = FrontierCity.BeginRoadFacingPreview(*Def, {X, 15});
					if (FrontierCity.ValidatePlacement(Preview, *Def).IsSuccess() &&
					    Zone({X, 15}, {2, 2}, X < 35 ? ECityZoneType::Residential : ECityZoneType::Industrial))
					{
						if (X < 35)
							++ValidHomes;
						else
							++ValidIndustry;
						++X;
					}
				}
				TestTrue(TEXT("Far-terrain layout has valid home and job lots"), ValidHomes > 0 && ValidIndustry > 0);
				const int32 InitialCount = FrontierCity.GetEntities().Num();
				AdvanceDays(Tuning.FixedStepsPerMonth, 1);
				if (ExpansionMonth == 12)
				{
					TestTrue(TEXT("Other ready families grow while frontier commercial lots are absent"),
					         FrontierCity.GetEntities().Num() > InitialCount);
					TestTrue(TEXT("Player can supply additional frontier commercial lots"),
					         Zone({35, 15}, {4, 2}, ECityZoneType::Commercial));
				}
				AdvanceDays(11 * Tuning.FixedStepsPerMonth, 1);
				int32 FrontierBuildings = 0;
				for (const auto& Pair : FrontierCity.GetEntities())
					if (Pair.Value.Anchor.X >= 25 && Pair.Value.DefinitionId != TEXT("WaterTower"))
						++FrontierBuildings;
				TestTrue(TEXT("Real growth reaches newly serviced far-terrain lots"),
				         FrontierBuildings > 0 && FrontierCity.GetEntities().Num() > InitialCount);
				TestTrue(TEXT("Terrain expansion retains a funded city"),
				         FrontierCity.GetFunds() > 0 && !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall);
				AddInfo(FString::Printf(
				    TEXT(
				        "Terrain seed %d route %d: tower X%d height %.0fcm, valid home/job parcels %d/%d, frontier buildings %d, treasury %lld"),
				    Seed, ExpansionMonth, TowerX, Sim->GetTerrain().GetCellCenterHeightCm({TowerX, 15}), ValidHomes,
				    ValidIndustry, FrontierBuildings, FrontierCity.GetFunds()));
			}
			// Parks/CivicBudget: each park option is affordable and operational on a real city route.
			if (Seed == 7429 && ExpansionMonth == 12)
			{
				for (FName Name : {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark")), FName(TEXT("CityPark")),
				                   FName(TEXT("BasketballPark")), FName(TEXT("SkatePark")), FName(TEXT("BaseballPark")),
				                   FName(TEXT("SoccerPark"))})
				{
					TestTrue(TEXT("Stable city reloads for park option"),
					         Sim->LoadCityFromPath(StablePath, Error, false));
					auto& ParkCity = Sim->GetPlacementModel();
					auto Access = ParkCity.BeginRoadPreview({20, 14});
					Access.End = {45, 14};
					Access.Profile = ParkCity.ResolveRoadDrawingProfile(Access.Start, Access.End, false);
					TestTrue(TEXT("Park access uses real terrain roads"), ParkCity.CommitRoad(Access).IsSuccess());
					TestTrue(TEXT("Park district purchases local water supply"),
					         Place(TEXT("WaterTower"), {25, 15}) > 0);
					uint64 Park = 0;
					for (int32 X = 14; X <= 40 && !Park; ++X)
						Park = Place(Name, {X, 15});
					if (TestTrue(TEXT("Normal treasury can place this park on actual terrain"), Park > 0))
					{
						Sim->Tick(0.f);
						const FString ParkPath = Directory / TEXT("park.json");
						TestTrue(TEXT("Pending park construction saves"), Sim->SaveCityToPath(ParkPath, Error));
						TestTrue(TEXT("Pending park construction resumes"),
						         Sim->LoadCityFromPath(ParkPath, Error, false));
						AdvanceDays(4 * Tuning.FixedStepsPerMonth, 1);
						TestTrue(TEXT("Purchased park finishes and operates"),
						         Sim->GetServiceSnapshot().ParkProviders.ContainsByPredicate(
						             [&](const auto& P) { return P.EntityId == Park; }));
						TestTrue(TEXT("Park option retains affordable paid city operations"),
						         Sim->GetPlacementModel().GetFunds() > 0 &&
						             !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall);
						if (Name != TEXT("Park"))
						{
							TestTrue(TEXT("Larger park completes the guided expansion goal"),
							         Sim->GetGuidedProgress().CompletedIds.Contains(TEXT("park_expansion")));
							TestTrue(TEXT("Normal park route records at least fourteen milestones"),
							         Sim->GetGuidedProgress().CompletedIds.Num() >= 14);
							if (!Sim->GetGuidedProgress().CompletedIds.Contains(TEXT("fire")))
							{
								TestEqual(TEXT("Remaining milestone is the unmet fire population gate"),
								          Sim->GetGuidedProgress().CurrentId, FName(TEXT("fire")));
								const auto Objective = Sim->GetGuidedObjective();
								TestTrue(TEXT("Ready station still explains the unfinished population goal"),
								         Objective.Title.Contains(TEXT("150")) &&
								             Objective.Progress.Contains(TEXT("/ 150")) && Objective.Fraction < 1.0f);
							}
						}
						AddInfo(FString::Printf(
						    TEXT("Real park %s: treasury %lld, monthly park upkeep %lld, completed objectives %d/%d"),
						    *Name.ToString(), Sim->GetPlacementModel().GetFunds(), Sim->GetServiceSnapshot().ParkUpkeep,
						    Sim->GetGuidedProgress().CompletedIds.Num(),
						    FCityFirstPlayableGuide::MilestoneIds().Num()));
					}
				}
			}
			TestTrue(TEXT("Partial-course city resumes for outage testing"),
			         Sim->LoadCityFromPath(LearningPath, Error, false));
			TestTrue(TEXT("Affordable route has students before outage"),
			         Sim->GetServiceSnapshot().EducationServed > 0);
			const auto BeforeOutage = Sim->GetPopulationSnapshot();
			TestTrue(TEXT("Branch road can be disconnected"), Sim->GetPlacementModel().RemoveRoad({10, 11}));
			AdvanceMonth();
			TestEqual(TEXT("Disconnected school serves no students"), Sim->GetServiceSnapshot().EducationServed, 0);
			for (const auto& Expected : BeforeOutage.Citizens)
			{
				const auto* Actual = Sim->GetPopulationSnapshot().Citizens.FindByPredicate(
				    [&](const auto& C) { return C.Id == Expected.Id; });
				if (TestNotNull(TEXT("Outage preserves each citizen"), Actual))
					TestTrue(TEXT("Disconnected month retains education progress"),
					         Actual->Education == Expected.Education &&
					             Actual->EducationMonths == Expected.EducationMonths);
			}
			const FString Path = Directory / TEXT("outage.json");
			TestTrue(TEXT("Real-budget outage saves"), Sim->SaveCityToPath(Path, Error));
			const int64 RecoveryStep = Sim->GetEconomySnapshot().FixedStepId + 2 * Tuning.FixedStepsPerMonth;
			int64 RecoveryFunds = 0;
			FCityPopulationSnapshot Recovered;
			FCityReportHistory RecoveryReports;
			for (int32 Speed : {1, 3, 5})
			{
				TestTrue(TEXT("Real-budget outage resumes"), Sim->LoadCityFromPath(Path, Error, false));
				Sim->SetSimulationSpeed(Speed);
				Sim->SetSimulationPaused(false);
				const int64 RepairFunds = Sim->GetPlacementModel().GetFunds();
				TestTrue(TEXT("Treasury can restore branch road"), Road({10, 11}, {10, 11}));
				TestTrue(TEXT("Recovery uses existing treasury"), Sim->GetPlacementModel().GetFunds() < RepairFunds);
				for (int32 Remaining = 2 * Tuning.FixedStepsPerMonth; Remaining > 0;)
				{
					const int32 Count = FMath::Min(Remaining, Speed);
					Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds * Count / Speed + 0.00001));
					Remaining -= Count;
				}
				TestEqual(TEXT("Recovery reaches the same fixed day"), Sim->GetEconomySnapshot().FixedStepId,
				          RecoveryStep);
				TestTrue(TEXT("Funded road repair restores school access"),
				         Sim->GetServiceSnapshot().EducationServed > 0);
				TestTrue(TEXT("Recovered school is operational"), Sim->GetServiceSnapshot().OperationalSchools > 0);
				bool bLearningResumed = false;
				for (const auto& Expected : BeforeOutage.Citizens)
				{
					const auto* Actual = Sim->GetPopulationSnapshot().Citizens.FindByPredicate(
					    [&](const auto& C) { return C.Id == Expected.Id; });
					if (Actual &&
					    (Actual->Education > Expected.Education || Actual->EducationMonths > Expected.EducationMonths))
						bLearningResumed = true;
				}
				TestTrue(TEXT("Paid recovery advances real partial learning"), bLearningResumed);
				if (Speed == 1)
				{
					RecoveryFunds = Sim->GetPlacementModel().GetFunds();
					Recovered = Sim->GetPopulationSnapshot();
					RecoveryReports = Sim->GetReportHistory();
				}
				else
				{
					CheckReports(RecoveryReports);
					TestEqual(TEXT("Funded recovery cash agrees across speeds"), Sim->GetPlacementModel().GetFunds(),
					          RecoveryFunds);
					TestEqual(TEXT("Funded recovery population agrees across speeds"),
					          Sim->GetPopulationSnapshot().Citizens.Num(), Recovered.Citizens.Num());
					for (const auto& Expected : Recovered.Citizens)
					{
						const auto* Actual = Sim->GetPopulationSnapshot().Citizens.FindByPredicate(
						    [&](const auto& C) { return C.Id == Expected.Id; });
						if (TestNotNull(TEXT("Stable citizen resumes"), Actual))
							TestTrue(TEXT("Recovered education and wages agree across speeds"),
							         Actual->Education == Expected.Education &&
							             Actual->EducationMonths == Expected.EducationMonths &&
							             Actual->AvailableMoney == Expected.AvailableMoney);
					}
				}
			}
			AddInfo(FString::Printf(
			    TEXT(
			        "Civic seed %d route %d: police %d fire %d school %d, min cash %lld, year-three cash %lld, residents %d, secondary %d, recovered cash %lld"),
			    Seed, ExpansionMonth, PoliceMonth, FireMonth, SchoolMonth, MinimumCash, YearThreeFunds,
			    YearThreeResidents, YearThreeSecondary, RecoveryFunds));
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*")), true, false);
			for (const auto& File : Files)
				IFileManager::Get().Delete(*(Directory / File), false, true, true);
			IFileManager::Get().DeleteDirectory(*Directory, false, false);
			World->DestroyWorld(false);
		}
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Diagnostics/CivicBudget-civic-budget.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("Civic monthly evidence writes"),
	         FFileHelper::SaveStringToFile(Report, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}
#endif
