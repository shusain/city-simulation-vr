#include "Presentation/CityBoardActor.h"
#include "Simulation/CityTechnology.h"
#include "Presentation/CityServiceGuidance.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

namespace
{
	FString FirstLine(const FString& Text)
	{
		int32 End;
		return Text.FindChar(TEXT('\n'), End) ? Text.Left(End) : Text;
	}
} // namespace

FString ACityBoardActor::DescribeDesktopToolContext() const
{
	if (!SimulationSubsystem || !PlacementModel)
		return FString();
	const auto& S = SimulationSubsystem->GetServiceSnapshot();
	const auto& T = SimulationSubsystem->GetEconomyTuning();
	FString Cost;
	const auto* Definition = GetSelectedDefinition();
	if (Definition && PlacementPreview.bActive && PlacementGhost && PlacementGhost->IsVisible())
	{
		const auto Validation = PlacementModel->ValidatePlacement(PlacementPreview, *Definition,
		                                                          ECityConstructionFunding::CityTreasury, &S);
		Cost = FString::Printf(TEXT("$%lld | %s\n"), Definition->Cost,
		                       bConstructionPriceBlocked ? TEXT("INSUFFICIENT FUNDS")
		                                                 : *FCityPlacementModel::FailureToString(Validation.Failure));
	}
	if (ToolMode == ECityToolMode::Technology && Definition)
		return Cost + Definition->DescribePrerequisites(S, BuildingCatalog);
	if (ToolMode == ECityToolMode::Repair)
	{
		const uint64 Id = HeatMapPoint.IsSet() ? PlacementModel->GetGrid().GetOccupant(HeatMapPoint.GetValue()) : 0;
		const int64 Price = SimulationSubsystem->GetRepairCost(Id);
		return Price > 0
		           ? FString::Printf(TEXT("Repair $%lld | %s"), Price,
		                             PlacementModel->GetFunds() >= Price ? TEXT("Click damaged building to repair")
		                                                                 : TEXT("INSUFFICIENT FUNDS"))
		           : TEXT("Point at a damaged building for cost; click to repair");
	}
	if (WaterPlacementSurface && WaterPlacementSurface->IsVisible())
	{
		int32 Short = 0;
		for (const auto& Demand : S.Water.EntityDemand)
			Short += S.Water.EntityDelivered.FindRef(Demand.Key) < Demand.Value;
		FString Site = S.Water.RulesVersion ? TEXT("Groundwater: red60% / amber100% / cyan140%")
		                                    : TEXT("Groundwater yield100% (legacy)");
		if (HeatMapPoint.IsSet())
		{
			const auto Cell = HeatMapPoint.GetValue();
			const int32 Index = Cell.Y * GetDimensions().GetWidth() + Cell.X;
			if (GetDimensions().IsValidCell(Cell) &&
			    WaterPlacementOverlay.GroundwaterYieldPercent.IsValidIndex(Index) &&
			    WaterPlacementOverlay.GroundwaterYieldPercent[Index] >= 0)
				Site += FString::Printf(TEXT(" | site %d%%"), WaterPlacementOverlay.GroundwaterYieldPercent[Index]);
		}
		FString Text = Cost + Site +
		               FString::Printf(TEXT("\nBlue = pressure, not spare supply | Water %d/%d | %d short"),
		                               S.ServiceUsed, S.ServiceDemand, Short);
		if (HeatMapPoint.IsSet())
			if (const auto* Road = WaterPlacementOverlay.Roads.FindByPredicate(
			        [&](const auto& R) { return R.Cell == HeatMapPoint.GetValue(); }))
				return Text + FString::Printf(TEXT("\nPipe %d%% | Spare source output %d"), Road->PressurePercent,
				                              Road->RemainingSourceOutput);
		return Text + TEXT("\nInspect shortages for the source and remedy");
	}
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::Education)
	{
		const auto* Focus = PlacementModel->GetEntities().Find(DisplayedServiceCoverageEntityId);
		const bool Proposed = ToolMode == ECityToolMode::School && Definition && PlacementPreview.bActive;
		const bool School = Proposed || (Focus && FCityBuildingDefinition::IsSchoolFacility(Focus->DefinitionId));
		if (School)
		{
			FString Text = Cost + FString::Printf(TEXT("Reach %d road steps | Cyan = potential reach"),
			                                      FMath::Clamp(T.SchoolRoadRangeHops, 1, 64));
			if (Proposed)
				Text +=
				    FString::Printf(TEXT("\nTeaches %s | Places need staff and utilities"),
				                    Definition->DefinitionId == TEXT("College")           ? TEXT("Secondary to Higher")
				                    : Definition->DefinitionId == TEXT("PrimarySchool")   ? TEXT("None to Basic")
				                    : Definition->DefinitionId == TEXT("SecondarySchool") ? TEXT("Basic to Secondary")
				                                                                          : TEXT("None to Secondary"));
			else
				Text += FString::Printf(TEXT("\nSchool places used %d/%d | Inspect homes for missing tier"),
				                        S.SchoolUsedCapacity.FindRef(DisplayedServiceCoverageEntityId),
				                        S.SchoolCapacities.FindRef(DisplayedServiceCoverageEntityId));
			return Text + TEXT("\nStudy places: green full / yellow some / red none");
		}
		return FString::Printf(
		    TEXT("Study places %d/%d | Capacity %d\nBright = more access | Inspect homes for tier and places"),
		    S.EducationServed, S.EducationDemand, S.EducationCapacity);
	}
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::Service)
		return FString::Printf(
		    TEXT(
		        "Blue = pipe pressure | Buildings: green full / yellow some / red none\nWater delivered %d/%d | Source output %d\nInspect shortages for pressure, capacity and remedies"),
		    S.ServiceUsed, S.ServiceDemand, S.ServiceCapacity);
	if (HeatMapResult.Overlay != ECityDiagnosticOverlay::None)
	{
		FString Text = HeatMapResult.Description + TEXT("\n") + FirstLine(HeatMapResult.Legend);
		if (HeatMapPoint.IsSet())
			Text += TEXT("\n") + FirstLine(HeatMapResult.DescribeCell(HeatMapPoint.GetValue()));
		return Text;
	}
	if (!Cost.IsEmpty())
	{
		const FString Range = FCityServiceGuidance::Range(*Definition, T);
		return Cost + FirstLine(Range);
	}
	if (ToolMode == ECityToolMode::Road)
	{
		TArray<FString> Lines;
		RoadConstructionStatus.ParseIntoArrayLines(Lines);
		Lines.SetNum(FMath::Min(2, Lines.Num()));
		FString Text = FString::Join(Lines, TEXT("\n"));
		if (bConstructionPriceBlocked && !Text.Contains(TEXT("INSUFFICIENT FUNDS")))
			Text = TEXT("INSUFFICIENT FUNDS\n") + Text;
		return Text;
	}
	return FString();
}
