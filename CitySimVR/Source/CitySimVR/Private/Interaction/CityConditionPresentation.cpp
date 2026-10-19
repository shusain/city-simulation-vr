#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "GameFramework/PlayerController.h"

void ACityVRToolController::SetConditionIndicatorsVisible(const bool bVisible)
{
	bConditionIndicatorsVisible = bVisible;
	UpdateConditionIndicators();
}

void ACityVRToolController::UpdateConditionIndicators()
{
	const auto* Sim = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	ConditionIndicators.Reset();
	const bool bToolVisible = !bDesktopUi;
	const bool bShow = bConditionIndicatorsVisible && Board && Board->HasCurrentCityModel() && HeadsetCamera && Sim &&
	                   bToolVisible && !bPaletteOpen && !bManipulationSession && LifecycleInputGate.CanAcceptInput() &&
	                   (Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::None ||
	                    Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Happiness);
	int32 AffectedLots = 0;
	if (bShow)
		ConditionIndicators = FCityConditionIndicators::Build(
		    Sim->GetPlacementModel(), Sim->GetFireSnapshot(), Sim->GetCrimeSnapshot(),
		    Sim->GetEconomyTuning().FireRepairSteps, &Sim->GetHealthSnapshot(), &Sim->GetPopulationSnapshot(),
		    InspectedEntityId, &Sim->GetWasteSnapshot(), &Sim->GetServiceSnapshot(),
		    FCityConditionIndicators::MaximumBadges, FCityConditionIndicators::MaximumClusterDiameterCells,
		    &AffectedLots);
	if (ConditionSummaryText)
	{
		int32 Shown = 0;
		for (const auto& R : ConditionIndicators)
			Shown += R.EntityIds.Num();
		ConditionSummaryText->SetVisibility(bShow && AffectedLots > 0);
		ConditionSummaryText->SetText(FText::FromString(
		    FString::Printf(TEXT("%d affected | %d shown | Settings > Next issue"), AffectedLots, Shown)));
	}
	TArray<FBox2D> Occupied;
	TArray<FTransform> Leaders;
	for (int32 Index = 0; Index < ConditionRoots.Num(); ++Index)
	{
		TArray<FVector> Roofs;
		FVector Roof = FVector::ZeroVector;
		double HighestRoof = -DBL_MAX;
		if (ConditionIndicators.IsValidIndex(Index))
			for (const auto Id : ConditionIndicators[Index].EntityIds)
			{
				FVector MemberRoof;
				if (!Board->GetBuildingMarkerAnchor(Id, MemberRoof))
					continue;
				Roofs.Add(MemberRoof);
				Roof += MemberRoof;
				HighestRoof = FMath::Max(HighestRoof, MemberRoof.Z);
			}
		const bool bActive = !Roofs.IsEmpty();
		auto* Root = ConditionRoots[Index].Get();
		if (Root->IsVisible() != bActive)
			Root->SetVisibility(bActive, true);
		if (!bActive)
			continue;
		Roof /= Roofs.Num();
		Roof.Z = HighestRoof;
		const auto& Row = ConditionIndicators[Index];
		const double Scale =
		    FMath::Clamp(FVector::Distance(Roof, HeadsetCamera->GetComponentLocation()) / 110.0, .25, 6.0);
		FVector Location = Roof + FVector::UpVector * (FCityConditionIndicators::HalfHeight + 1.5) * Scale;
		// Separate nearby badges in the viewer's projection. Leaders retain lot
		// association; the work is bounded by the fixed pool, not city population.
		FBox2D Bounds;
		for (int32 Attempt = 0; Attempt < FCityConditionIndicators::MaximumBadges; ++Attempt)
		{
			const FVector View = HeadsetCamera->GetComponentTransform().InverseTransformPosition(Location);
			const double Depth = FMath::Max(1.0, View.X);
			const FVector2D Center(View.Y / Depth, View.Z / Depth),
			    Half((FCityConditionIndicators::HalfWidth + .5) * Scale / Depth,
			         (FCityConditionIndicators::HalfHeight + .5) * Scale / Depth);
			Bounds = FBox2D(Center - Half, Center + Half);
			if (!Occupied.ContainsByPredicate([&](const auto& Other) { return Bounds.Intersect(Other); }))
				break;
			Location += HeadsetCamera->GetUpVector() * (2 * FCityConditionIndicators::HalfHeight + 1.5) * Scale;
		}
		Occupied.Add(Bounds);
		FRotator Facing = (HeadsetCamera->GetComponentLocation() - Location).Rotation();
		Facing.Roll = 0;
		Root->SetWorldLocationAndRotation(Location, Facing);
		Root->SetWorldScale3D(FVector(Scale));
		ConditionIcons[Index]->SetMaterial(0, ConditionIconMaterials[Row.bCrime       ? 2
		                                                             : Row.bRepairing ? 1
		                                                                              : 0]);
		if (const auto* Material = ConditionSymbols.Find(Row.Symbol());
		    Material && Row.Symbol() != TEXT("Police") && Row.Symbol() != TEXT("Repair"))
			ConditionIcons[Index]->SetMaterial(0, *Material);
		ConditionIcons[Index]->SetVisibility(true);
		ConditionCrimeIcons[Index]->SetVisibility(false);
		const FString Title = FString::Printf(
		    TEXT("%d lot%s%s"), Row.EntityIds.Num(), Row.EntityIds.Num() == 1 ? TEXT("") : TEXT("s"),
		    Row.CauseCount() > 1 ? *FString::Printf(TEXT(" +%d"), Row.CauseCount() - 1) : TEXT(""));
		const FString Detail;
		ConditionDetails[Index]->SetVisibility(false);
		if (ConditionTitles[Index]->Text.ToString() != Title)
			ConditionTitles[Index]->SetText(FText::FromString(Title));
		if (ConditionDetails[Index]->Text.ToString() != Detail)
			ConditionDetails[Index]->SetText(FText::FromString(Detail));
		ConditionTitles[Index]->SetTextRenderColor(Row.bRepairing ? FColor::Cyan
		                                           : Row.bNeedsRepair || Row.SearchingResidents
		                                               ? FColor(255, 205, 85)
		                                               : FColor(155, 210, 255));
		ConditionDetails[Index]->SetTextRenderColor(FColor::White);
		ConditionRails[Index]->SetVisibility(Row.bRepairing);
		ConditionFills[Index]->SetVisibility(Row.bRepairing && Row.RepairPercent > 0);
		const double Fraction = Row.RepairPercent / 100.0;
		ConditionFills[Index]->SetRelativeLocation(FVector(.45, 2.75 * (1 - Fraction), -2.35));
		ConditionFills[Index]->SetRelativeScale3D(FVector(.002, .055 * FMath::Max(.001, Fraction), .0012));
		const FVector End =
		    Root->GetComponentTransform().TransformPosition(FVector(0, 0, -FCityConditionIndicators::HalfHeight));
		for (const FVector& MemberRoof : Roofs)
			if (Leaders.Num() < FCityConditionIndicators::MaximumLeaders)
				Leaders.Emplace((End - MemberRoof).Rotation(), (MemberRoof + End) * .5,
				                FVector(FVector::Distance(MemberRoof, End) / 100.0, .001 * Scale, .001 * Scale));
	}
	if (ConditionLeaderInstances->GetInstanceCount() != Leaders.Num())
	{
		ConditionLeaderInstances->ClearInstances();
		if (!Leaders.IsEmpty())
			ConditionLeaderInstances->AddInstances(Leaders, false, true, false);
	}
	else if (!Leaders.IsEmpty())
		ConditionLeaderInstances->BatchUpdateInstancesTransforms(0, Leaders, true, true, true);
	ConditionLeaderInstances->SetVisibility(!Leaders.IsEmpty());
}

int32 ACityVRToolController::FindConditionIndicator(const FVector& Origin, const FVector& Direction,
                                                    double& OutDistance) const
{
	int32 Result = INDEX_NONE;
	OutDistance = 1000;
	const auto* Sim = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	FCityCellCoord Cell;
	FVector TerrainHit;
	if (Board && Sim &&
	    CityBoardPicking::PickCellFromWorldRay(Board->GetActorTransform(), Sim->GetTerrain(), Origin, Direction, Cell,
	                                           &TerrainHit))
		OutDistance = FMath::Min(OutDistance, FVector::Distance(Origin, TerrainHit));
	for (int32 Index = 0; Index < ConditionIndicators.Num(); ++Index)
	{
		double Distance = 0;
		if (ConditionRoots[Index]->IsVisible() &&
		    FCityConditionIndicators::Hit(ConditionRoots[Index]->GetComponentTransform(), Origin, Direction,
		                                  Distance) &&
		    Distance < OutDistance)
		{
			Result = Index;
			OutDistance = Distance;
		}
	}
	return Result;
}

uint64 ACityVRToolController::PickConditionMember(const int32 Index, const FVector& Origin,
                                                  const FVector& Direction) const
{
	if (!Board || !ConditionIndicators.IsValidIndex(Index) || Direction.IsNearlyZero() || Direction.ContainsNaN() ||
	    Origin.ContainsNaN())
		return FCityGrid::EmptyEntityId;
	const auto& Row = ConditionIndicators[Index];
	uint64 Closest = Row.EntityId;
	double Best = DBL_MAX;
	const FVector Ray = Direction.GetSafeNormal();
	for (const auto Id : Row.EntityIds)
	{
		FVector Roof;
		if (!Board->GetBuildingMarkerAnchor(Id, Roof))
			continue;
		const FVector Offset = Roof - Origin;
		const double Along = FVector::DotProduct(Offset, Ray);
		if (Along <= 0)
			continue;
		const double Score = FMath::Max(0.0, Offset.SizeSquared() - Along * Along) / FMath::Max(1.0, Along * Along);
		if (Score < Best)
		{
			Best = Score;
			Closest = Id;
		}
	}
	return Closest;
}

void ACityVRToolController::FocusNextCondition()
{
	if (!Board || bEntrySceneActive || !LifecycleInputGate.CanAcceptInput() || Board->HasActiveToolAction() ||
	    bManipulationSession)
		return;
	auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	if (!Sim)
		return;
	if (FocusedConditionSession != Sim->GetCitySessionGeneration())
	{
		LastFocusedCondition = 0;
		FocusedConditionSession = Sim->GetCitySessionGeneration();
	}
	const auto Rows = FCityConditionIndicators::Build(
	    Sim->GetPlacementModel(), Sim->GetFireSnapshot(), Sim->GetCrimeSnapshot(),
	    Sim->GetEconomyTuning().FireRepairSteps, &Sim->GetHealthSnapshot(), &Sim->GetPopulationSnapshot(), 0,
	    &Sim->GetWasteSnapshot(), &Sim->GetServiceSnapshot(), MAX_int32, 0);
	if (Rows.IsEmpty())
		return;
	const int32 Previous =
	    Rows.IndexOfByPredicate([&](const auto& R) { return R.EntityId == LastFocusedCondition; });
	const uint64 Id = Rows[(Previous + 1) % Rows.Num()].EntityId;
	SetPaletteOpen(false);
	SelectInspect();
	InspectBuilding(static_cast<int64>(Id));
	LastFocusedCondition = Id;
	if (bDesktopUi)
		if (auto* Player = GetWorld()->GetFirstPlayerController())
			if (auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn()))
			{
				FVector Roof;
				if (Board->GetBuildingMarkerAnchor(Id, Roof))
					Pawn->FocusCondition(Roof);
			}
}
