#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityPolicyFixture
{
  public:
	static void ClearWallets(UCitySimulationSubsystem* Sim)
	{
		auto People = Sim->Population.GetSnapshot();
		for (auto& C : People.Citizens)
			C.AvailableMoney = 0;
		FString Error;
		check(Sim->Population.RestoreState(People, Error));
		Sim->Population.Reconcile(Sim->GetPlacementModel(), Sim->Economy.GetSnapshot(), Sim->EconomyTuning,
		                          &Sim->ServiceSnapshot);
	}
	static bool Seed(UCitySimulationSubsystem* Sim, ACityBoardActor* Board)
	{
		auto Parameters = Sim->Terrain.GetParameters();
		Parameters.GeneratorVersion = 0;
		Parameters.BroadElevationAmplitudeCm = Parameters.HillAmplitudeCm = Parameters.RidgeAmplitudeCm = 0;
		Sim->Terrain.Generate(Parameters);
		Sim->ResetCity(false);
		auto* Original = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!Original)
			return false;
		auto* Catalog = NewObject<UCityBuildingCatalog>();
		Catalog->Definitions = Original->Definitions;
		for (auto& D : Catalog->Definitions)
			if (D.DefinitionId == TEXT("Home") && D.DensityLevels.Num() > 1)
				D.DensityLevels.SetNum(1);
		Sim->RegisterBuildingCatalog(Catalog);
		Board->PrepareCityView();
		auto& City = Sim->GetPlacementModel();
		auto Road = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Road, {32, 8});
		if (!City.CommitRoad(Road).IsSuccess())
			return false;
		const TCHAR* Names[] = {TEXT("Power"), TEXT("Service"),       TEXT("Home"),           TEXT("Home"),
		                        TEXT("Home"),  TEXT("PrimarySchool"), TEXT("SecondarySchool")};
		const int32 Xs[] = {1, 4, 7, 11, 15, 19, 25};
		for (int32 I = 0; I < 7; ++I)
		{
			const auto* Definition = Catalog->FindDefinition(Names[I]);
			if (!Definition)
				return false;
			auto Preview = City.BeginPreview(*Definition, {Xs[I], 9});
			if (!City.CommitPlacement(Preview, *Definition).IsSuccess())
				return false;
		}
		Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
		Sim->EconomyTuning.FixedStepSeconds = .125;
		Sim->EconomyTuning.FixedStepsPerMonth = 4;
		Sim->EconomyTuning.MaxCatchUpSteps = 32;
		Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
		Sim->StartScenario();
		Sim->SetSimulationSpeed(1);
		for (int32 I = 0; I < 4; ++I)
			Sim->Tick(.125f);
		Sim->SetSimulationPaused(true);
		auto People = Sim->Population.GetSnapshot();
		for (auto& C : People.Citizens)
			C.AvailableMoney = 0;
		FString Error;
		if (!Sim->Population.RestoreState(People, Error))
			return false;
		Sim->Population.Reconcile(City, Sim->Economy.GetSnapshot(), Sim->EconomyTuning,
		                          &Sim->ServiceSnapshot);
		++Sim->PresentationGeneration;
		Board->PrepareCityView();
		return !People.Citizens.IsEmpty();
	}
};
class FCityDesktopPoliciesCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopPoliciesCommand(FAutomationTestBase* In) : Test(In)
	{
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (W)
			for (TActorIterator<ACityVRToolController> It(W); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->DesktopHud || !UI->Board)
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Policy HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		auto Click = [&](ECityPaletteAction Action) {
			const auto Data = UI->GetUiSnapshot();
			for (int32 I = 0; I < Data.MenuEntries.Num(); ++I)
				if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
				{
					const FString Name =
					    FString::Printf(TEXT("%s%d"), Data.bNewspaper ? TEXT("NewsAction") : TEXT("Menu"), I);
					auto* B =
					    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(FName(Name)));
					if (B && B->GetIsEnabled())
						B->OnClicked.Broadcast();
					else
						Test->AddError(TEXT("Policy button unavailable"));
					return;
				}
			Test->AddError(TEXT("Policy action missing"));
		};
		auto Capture = [](const TCHAR* Name) {
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/CityPolicies");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / Name, true, false);
		};
		auto Month = [&]() {
			Sim->SetSimulationPaused(false);
			for (int32 I = 0; I < 4; ++I)
				Sim->Tick(.125f);
			Sim->SetSimulationPaused(true);
			UI->RefreshReports();
		};
		FString Error;
		switch (Phase)
		{
		case 0:
			UI->ReturnToCity();
			if (!FCityPolicyFixture::Seed(Sim, UI->Board))
			{
				Test->AddError(TEXT("Policy school/citizen fixture failed"));
				return true;
			}
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 1:
			Click(ECityPaletteAction::OpenPolicies);
			break;
		case 2:
			Click(ECityPaletteAction::ReviewReservePolicy);
			break;
		case 3:
			Test->TestFalse(TEXT("Preview not enabled"),
			                Sim->GetEconomySnapshot().Policies.Programs[0].bEnabled);
			Capture(TEXT("AidTerms.png"));
			break;
		case 4:
			Click(ECityPaletteAction::ConfirmPolicy);
			break;
		case 5:
			Click(ECityPaletteAction::ReviewBursaryPolicy);
			break;
		case 6:
			Capture(TEXT("BursaryTerms.png"));
			break;
		case 7:
			Click(ECityPaletteAction::ConfirmPolicy);
			break;
		case 8:
			Month();
			Test->TestTrue(TEXT("Assistance paid after real settlement"),
			               Sim->GetEconomySnapshot().Policies.Programs[0].GrantsPaid > 0);
			Test->TestTrue(TEXT("Real operating learners receive bursaries"),
			               Sim->GetEconomySnapshot().Policies.Programs[1].GrantsPaid > 0);
			{
				int64 Paid = 0;
				for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
					Paid += C.LastPolicyGrant;
				Test->TestEqual(TEXT("Recipient wallets reconcile to policy ledger"), Paid,
				                Sim->GetEconomySnapshot().Policies.Programs[0].GrantsPaid +
				                    Sim->GetEconomySnapshot().Policies.Programs[1].GrantsPaid);
			}
			{
				const FString Save = FPaths::ProjectSavedDir() / TEXT("Automation/Technology-Policy-City.json");
				Test->TestTrue(TEXT("Live policy city saves"), Sim->SaveCityToPath(Save, Error));
				Test->TestTrue(TEXT("Live policy city reloads"), Sim->LoadCityFromPath(Save, Error, false));
			}
			UI->RefreshReports();
			break;
		case 9:
			Capture(TEXT("Funded.png"));
			break;
		case 10:
			FCityPolicyFixture::ClearWallets(Sim);
			Sim->GetPlacementModel().TrySpendFunds(Sim->GetPlacementModel().GetFunds());
			Month();
			Test->TestTrue(TEXT("Funding failure visible"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("WARNING: unfunded")));
			Test->TestEqual(TEXT("No unfunded benefit created"), Sim->GetEconomySnapshot().Policies.Paid(),
			                int64(0));
			break;
		case 11:
			Capture(TEXT("Unfunded.png"));
			break;
		case 12:
			Click(ECityPaletteAction::Back);
			break;
		case 13:
			Click(ECityPaletteAction::OpenQuarterlyNews);
			break;
		case 14:
			Test->TestTrue(
			    TEXT("Policy fiscal issue makes quarter article"),
			    UI->SharedNewspaper.Articles.Last().Headline.ToString().Contains(TEXT("falls short")));
			if (auto* Paper =
			        Cast<UCityDesktopNewspaper>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Newspaper"))))
				if (auto* Scroll = Cast<UScrollBox>(Paper->GetParent()->GetParent()))
					Scroll->ScrollToEnd();
			break;
		case 15:
			Capture(TEXT("PolicyNews.png"));
			break;
		case 16:
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_DESKTOP_POLICIES_COMPLETE Captures=5 SyntheticButtons=1 "
			            "PhysicalInputPending=1"));
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + .8;
		return false;
	}

  private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0;
	double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopPoliciesTest, "CitySim.Desktop.FundedPolicies",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopPoliciesTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Technology")))
	{
		AddError(TEXT("Policy smoke requires isolated Technology desktop profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopPoliciesCommand(this));
	return true;
}
#endif
