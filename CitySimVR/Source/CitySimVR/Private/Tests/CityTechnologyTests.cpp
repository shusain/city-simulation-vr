#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityTechnology.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Interaction/CityPaletteModel.h"
#include "Presentation/CityDesktopIcon.h"
#include "CityTechnologyFixture.h"
#include "Interaction/CityVRToolController.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace
{
struct TechnologyFixture
{
	UCityBuildingCatalog* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	FCityPlacementModel City{100000};
	FCityDevelopmentSimulation Dev;
	FCityEconomyTuning T;
	TMap<FName, uint64> Ids;
	FCityServiceSnapshot Services(const FCityPopulationSnapshot* People = nullptr)
	{
		return FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Dev.GetSnapshot(), nullptr, People);
	}
	uint64 Place(FName Kind, int32 X)
	{
		const auto* D = Catalog ? Catalog->FindDefinition(Kind) : nullptr;
		if (!D)
			return 0;
		auto S = Services();
		auto P = City.BeginPreview(*D, {X, 9});
		const auto R = City.CommitPlacement(P, *D, ECityConstructionFunding::CityTreasury, &S);
		if (R.IsSuccess())
			Ids.Add(Kind, R.EntityId);
		return R.EntityId;
	}
	bool Setup()
	{
		if (!Catalog)
			return false;
		auto Road = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Road, {40, 8});
		if (!City.CommitRoad(Road).IsSuccess() || !Place(TEXT("CoalPower"), 1) ||
		    !Place(TEXT("Service"), 4) || !Place(TEXT("College"), 8))
			return false;
		Dev.InitializeExistingAsOperational(City, Catalog, 0);
		return true;
	}
	bool Complete()
	{
		if (!Setup())
			return false;
		int32 X = 12;
		for (const FName Kind : {FName(TEXT("DigitalExchange")), FName(TEXT("ComputingCenter")), FName(TEXT("ResearchCampus"))})
		{
			if (!Place(Kind, X))
				return false;
			Dev.InitializeExistingAsOperational(City, Catalog, 0);
			X += 4;
		}
		return true;
	}
	FCityPopulationSnapshot Workers()
	{
		FCityPopulationSnapshot P;
		for (const auto& Pair : Ids)
		{
			const auto* D = Catalog->FindDefinition(Pair.Key);
			const int32 Jobs = Pair.Key == TEXT("Service") ? T.UtilityStaff : D->JobsProvided;
			FCityPublicEmployerRecord E;
			E.EntityId = Pair.Value;
			E.JobCapacity = Jobs;
			E.PayrollDue = E.PayrollPaid = Jobs * T.HouseholdIncomePerEmployedResident;
			P.PublicEmployers.Add(E);
			for (int32 I = 0; I < Jobs; ++I)
			{
				FCityCitizenRecord C;
				C.Id = P.Citizens.Num() + 1;
				C.WorkplaceEntityId = Pair.Value;
				C.Education = ECityEducationLevel::Higher;
				P.Citizens.Add(C);
			}
		}
		return P;
	}
};
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyTransitionsTest, "CitySim.Technology.OperatingPrerequisites",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyTransitionsTest::RunTest(const FString& Parameters)
{
	TechnologyFixture F;
	if (!TestTrue(TEXT("Existing catalog foundation places"), F.Setup()))
		return false;
	const auto* D = F.Catalog->FindDefinition(TEXT("ComputingCenter"));
	if (!TestNotNull(TEXT("Computing catalog"), D))
		return false;
	auto P = F.City.BeginPreview(*D, {16, 9});
	const int64 Funds = F.City.GetFunds();
	const auto Revision = F.City.GetRevision();
	TestEqual(TEXT("No snapshot cannot bypass prerequisites"), F.City.CommitPlacement(P, *D).Failure,
	          ECityPlacementFailure::TechnologyPrerequisite);
	auto S = F.Services();
	TestEqual(TEXT("No exchange rejects construction"),
	          F.City.CommitPlacement(P, *D, ECityConstructionFunding::CityTreasury, &S).Failure,
	          ECityPlacementFailure::TechnologyPrerequisite);
	TestEqual(TEXT("Locked command moves no money"), F.City.GetFunds(), Funds);
	TestEqual(TEXT("Locked command changes no revision"), F.City.GetRevision(), Revision);
	if (!TestTrue(TEXT("Exchange places after actual power/college"),
	              F.Place(TEXT("DigitalExchange"), 12) > 0))
		return false;
	F.Dev.Reconcile(F.City, F.Catalog);
	S = F.Services();
	TestFalse(TEXT("Construction alone cannot unlock computing"), D->PrerequisitesMet(&S));
	F.Dev.InitializeExistingAsOperational(F.City, F.Catalog, 0);
	if (!TestTrue(TEXT("Operating exchange unlocks computing"), F.Place(TEXT("ComputingCenter"), 16) > 0))
		return false;
	F.Dev.InitializeExistingAsOperational(F.City, F.Catalog, 0);
	if (!TestTrue(TEXT("Operating computing unlocks research"), F.Place(TEXT("ResearchCampus"), 20) > 0))
		return false;
	F.Dev.InitializeExistingAsOperational(F.City, F.Catalog, 0);
	S = F.Services();
	TestEqual(TEXT("Full physical foundation operating"),
	          S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), 1);
	F.City.DemolishEntity(F.Ids.FindRef(TEXT("DigitalExchange")));
	P = F.City.BeginPreview(*D, {28, 9});
	TestEqual(TEXT("Stale pre-demolition capabilities cannot authorize a new building"),
	          F.City.CommitPlacement(P, *D, ECityConstructionFunding::CityTreasury, &S).Failure,
	          ECityPlacementFailure::StalePreview);
	S = F.Services();
	TestEqual(TEXT("Demolition suspends computing"), S.GetFailure(F.Ids.FindRef(TEXT("ComputingCenter"))),
	          ECityOperationFailure::TechnologyPrerequisite);
	TestEqual(TEXT("Demolition suspends research"), S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), 0);
	TestTrue(TEXT("Exchange can rebuild without permanent lock"), F.Place(TEXT("DigitalExchange"), 12) > 0);
	F.Dev.InitializeExistingAsOperational(F.City, F.Catalog, 0);
	S = F.Services();
	TestEqual(TEXT("Higher rebuilt ID resumes dependency chain"),
	          S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyStaffTest, "CitySim.Technology.StaffAndUtilities",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyStaffTest::RunTest(const FString& Parameters)
{
	TechnologyFixture F;
	if (!TestTrue(TEXT("Complete technology catalog"), F.Complete()))
		return false;
	auto P = F.Workers();
	auto S = F.Services(&P);
	const uint64 Exchange = F.Ids.FindRef(TEXT("DigitalExchange"));
	const uint64 Compute = F.Ids.FindRef(TEXT("ComputingCenter"));
	const uint64 Research = F.Ids.FindRef(TEXT("ResearchCampus"));
	TestEqual(TEXT("Qualified paid chain operates"), S.OperatingFacilities.FindRef(TEXT("ResearchCampus")),
	          1);
	TestEqual(TEXT("Public technology vacancies counted exactly once"), S.Jobs, 28);
	TestEqual(TEXT("Exact authored technology upkeep"), S.CivicUpkeep, int64(380)); // College90 +40+100+150.
	TestTrue(TEXT("Computing consumes substantial power"), S.PowerUsed >= 95);
	TestEqual(TEXT("Actual cooling water accounted"), S.Water.EntityDelivered.FindRef(Compute), 15);
	for (auto& C : P.Citizens)
		if (C.WorkplaceEntityId == Research)
			C.Education = ECityEducationLevel::Secondary;
	S = F.Services(&P);
	TestEqual(TEXT("Secondary cannot operate higher research"), S.GetFailure(Research),
	          ECityOperationFailure::NoStaff);
	TestEqual(TEXT("Vacancies retained to bootstrap qualified hiring"), S.PublicJobs.FindRef(Research), 6);
	for (auto& C : P.Citizens)
		if (C.WorkplaceEntityId == Research)
			C.Education = ECityEducationLevel::Higher;
	for (auto& E : P.PublicEmployers)
		if (E.EntityId == Exchange)
			E.PayrollPaid = 0;
	S = F.Services(&P);
	TestEqual(TEXT("Unpaid exchange cannot operate"), S.GetFailure(Exchange), ECityOperationFailure::NoStaff);
	TestEqual(TEXT("Unpaid upstream removes downstream capability"),
	          S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), 0);
	P = F.Workers();
	for (auto& C : P.Citizens)
		if (C.WorkplaceEntityId == Exchange)
			C.bOnMedicalLeave = true;
	S = F.Services(&P);
	TestEqual(TEXT("Medical leave cannot fake qualified staffing"), S.PublicStaffPercent.FindRef(Exchange),
	          0);
	P = F.Workers();
	int32 Absent = 0;
	for (auto& C : P.Citizens)
		if (C.WorkplaceEntityId == Exchange && Absent++ < 2)
			C.WorkplaceEntityId = 0;
	S = F.Services(&P);
	TestEqual(TEXT("Half staffing meets documented floor"),
	          S.OperatingFacilities.FindRef(TEXT("ResearchCampus")), 1);
	F.City.DemolishEntity(F.Ids.FindRef(TEXT("Service")));
	S = F.Services(&P);
	TestEqual(TEXT("Water outage suspends foundation"), S.OperatingFacilities.FindRef(TEXT("ResearchCampus")),
	          0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyPresentationTest, "CitySim.Technology.SharedNavigation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyPresentationTest::RunTest(const FString& Parameters)
{
	TechnologyFixture F;
	if (!TestTrue(TEXT("Catalog loads"), F.Setup()))
		return false;
	const auto S = F.Services();
	TestTrue(TEXT("Research explains concrete lock"),
	         FCityTechnology::Review(TEXT("ResearchCampus"), S, F.Catalog)
	             .Contains(TEXT("NEED Computing center operating")));
	TestTrue(TEXT("Actual operating status is visible"),
	         FCityTechnology::Review(TEXT("ResearchCampus"), S, F.Catalog)
	             .Contains(TEXT("exchange 0 | computing 0 | research 0")));
	TestEqual(TEXT("Technology returns to Build"),
	          FCityPaletteModel::ParentPage(ECityPalettePage::TechnologyFacilities), ECityPalettePage::Build);
	auto Early = *F.Catalog->FindDefinition(TEXT("PrimarySchool"));
	Early.RequiredOperatingFacilities = {TEXT("College")};
	TestTrue(TEXT("Ordinary facility can use the same data-driven lock"), Early.PrerequisitesMet(&S));
	TestFalse(TEXT("Ordinary facility cannot bypass lock without a service snapshot"),
	          Early.PrerequisitesMet(nullptr));
	FCityPlacementModel Fresh(10000);
	auto Preview = Fresh.BeginPreview(Early, {2, 9});
	TestEqual(TEXT("Ordinary construction uses the authoritative lock"),
	          Fresh.CommitPlacement(Preview, Early).Failure, ECityPlacementFailure::TechnologyPrerequisite);
	TestEqual(TEXT("Blocked ordinary construction preserves cash"), Fresh.GetFunds(), int64(10000));
	Early.RequiredOperatingFacilities.Add(Early.DefinitionId);
	TestFalse(TEXT("Self prerequisite rejected at definition validation"), Early.IsValid());
	TestEqual(TEXT("Technology review returns to City"),
	          FCityPaletteModel::ParentPage(ECityPalettePage::TechnologyReview), ECityPalettePage::City);
	auto HasAction = [](ECityPalettePage Page, ECityPaletteAction Action)
	{ return FCityPaletteModel::Entries(Page).ContainsByPredicate([&](const auto& E) { return E.Action == Action; }); };
	TestTrue(TEXT("City owns progression review"),
	         HasAction(ECityPalettePage::City, ECityPaletteAction::OpenTechnology));
	TestFalse(TEXT("Build does not open progression report"),
	          HasAction(ECityPalettePage::Build, ECityPaletteAction::OpenTechnology));
	TestTrue(TEXT("Build owns construction picker"),
	         HasAction(ECityPalettePage::Build, ECityPaletteAction::OpenTechnologyFacilities));
	TestFalse(TEXT("Construction needs no extra USE TOOL action"),
	          HasAction(ECityPalettePage::TechnologyFacilities, ECityPaletteAction::Close));
	for (auto Action : {ECityPaletteAction::OpenTechnology, ECityPaletteAction::OpenTechnologyFacilities,
	                    ECityPaletteAction::SelectDigitalExchange, ECityPaletteAction::SelectComputingCenter,
	                    ECityPaletteAction::SelectResearchCampus, ECityPaletteAction::ReviewDigitalExchange,
	                    ECityPaletteAction::ReviewComputingCenter, ECityPaletteAction::ReviewResearchCampus})
		TestFalse(TEXT("Every technology action has an icon"),
		          UCityDesktopIcon::ForAction(static_cast<int32>(Action)).IsNone());
	for (bool Hand : {false, true})
	{
		const auto Entries = FCityPaletteModel::SurfaceEntries(ECityPalettePage::TechnologyFacilities, Hand);
		TestTrue(TEXT("Bounded shared action pool"), Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
		TestEqual(TEXT("Normal six-button construction picker"), Entries.Num(),
		          (Hand ? 0 : FCityPaletteModel::BarEntryCount) + 6);
		TestEqual(TEXT("Review retains shared report toolbar"),
		          FCityPaletteModel::SurfaceEntries(ECityPalettePage::TechnologyReview, Hand).Num(),
		          FCityPaletteModel::BarEntryCount + 7);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyLiveTest, "CitySim.Technology.LiveSettlementAndReload",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyLiveTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TechnologyLiveTest"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	if (!TestTrue(TEXT("Live qualified city seed"), FCityTechnologyFixture::Seed(Sim)))
	{
		W->DestroyWorld(false);
		return false;
	}
	int32 X = 12;
	for (const FName Id : {FName(TEXT("DigitalExchange")), FName(TEXT("ComputingCenter")), FName(TEXT("ResearchCampus"))})
	{
		if (!TestTrue(TEXT("Real construction/hiring enables facility"),
		              FCityTechnologyFixture::Place(Sim, Id, X)))
		{
			W->DestroyWorld(false);
			return false;
		}
		X += 4;
	}
	TestTrue(TEXT("Technology jobs pay named public payroll"),
	         Sim->GetEconomySnapshot().LastPublicPayrollPaid > 0);
	const uint64 Research = FCityTechnologyFixture::Find(Sim, TEXT("ResearchCampus"));
	const auto* Employer = Sim->GetPopulationSnapshot().PublicEmployers.FindByPredicate(
	    [Research](const auto& E) { return E.EntityId == Research; });
	TestTrue(TEXT("Research staff actually receive funded wages"), Employer && Employer->PayrollPaid > 0);
	TestTrue(TEXT("Technology upkeep is included in actual bills"),
	         Sim->GetEconomySnapshot().CivicUpkeep >= 380);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/Technology-Technology-Native.json");
	FString Error;
	TestTrue(TEXT("Live technology city saves"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Live technology city reloads"), Sim->LoadCityFromPath(Path, Error, false));
	TestEqual(TEXT("Capabilities reconstructed from operating saved records"),
	          Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("ResearchCampus")), 1);
	auto* UI = W->SpawnActor<ACityVRToolController>();
	UI->Board = W->SpawnActor<ACityBoardActor>();
	UI->Board->BuildingCatalog = const_cast<UCityBuildingCatalog*>(Sim->GetBuildingCatalog());
	UI->Board->DispatchBeginPlay();
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bPaletteOpen = true;
	const FName BeforeReview = UI->Board->GetSelectedFacilityId();
	const auto BeforeTool = UI->Board->GetToolMode();
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenTechnology);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewResearchCampus);
	TestEqual(TEXT("VR review never changes active facility"), UI->Board->GetSelectedFacilityId(), BeforeReview);
	TestEqual(TEXT("VR review never arms a tool"), UI->Board->GetToolMode(), BeforeTool);
	UI->RefreshReports();
	TestTrue(TEXT("World sheet contains actual research requirements"),
	         UI->ReportText->Text.ToString().Contains(TEXT("Computing center operating")));
	const auto Bounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	AddInfo(Bounds.ToString());
	TestTrue(TEXT("Longest technology review fits shared world sheet"),
	         Bounds.Min.Z > 28 && Bounds.Max.Z < 82 && Bounds.Min.Y >= -47 && Bounds.Max.Y <= 47);
	W->DestroyWorld(false);
	return true;
}
#endif
