#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

bool ACityVRToolController::IsInspectorSurfaceHit(const FVector& Origin, const FVector& Direction) const
{
	if (!Board || Board->GetToolMode() != ECityToolMode::Inspect || bPaletteOpen || bManipulationSession ||
	    !LifecycleInputGate.CanAcceptInput() || !InspectorStatusRoot->IsVisible() || InspectedEntityId == 0)
		return false;
	const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	if (!Sim || InspectionGeneration != Sim->GetPresentationGeneration())
		return false;
	const FVector Normal = InspectorStatusRoot->GetForwardVector();
	const double Denominator = FVector::DotProduct(Direction, Normal);
	if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
		return false;
	const double Distance =
	    FVector::DotProduct(InspectorStatusRoot->GetComponentLocation() - Origin, Normal) / Denominator;
	if (Distance < 0 || Distance > 1000.0)
		return false;
	const FVector Local =
	    InspectorStatusRoot->GetComponentTransform().InverseTransformPosition(Origin + Direction * Distance);
	if (FMath::Abs(Local.Y) > InspectorHalfWidth || FMath::Abs(Local.Z) > InspectorHalfHeight)
		return false;
	uint64 Entity = 0;
	FVector Hit;
	if (Board->PickInspectionBuilding(Origin, Direction, Entity, Hit) &&
	    FVector::Distance(Origin, Hit) + 0.05 < Distance)
		return false;
	return true;
}

bool ACityVRToolController::HandleInspectorUpgradeRay(const FVector& Origin, const FVector& Direction)
{
	if (!IsInspectorSurfaceHit(Origin, Direction))
		return false;
	if (!bInspectorUpgradeAvailable)
		return true;
	const FVector Normal = InspectorStatusRoot->GetForwardVector();
	const double Distance = FVector::DotProduct(InspectorStatusRoot->GetComponentLocation() - Origin, Normal) /
	                        FVector::DotProduct(Direction, Normal);
	const FVector Local =
	    InspectorStatusRoot->GetComponentTransform().InverseTransformPosition(Origin + Direction * Distance);
	if (FMath::Abs(Local.Y) > InspectorHalfWidth - 2.0 ||
	    FMath::Abs(Local.Z - InspectorUpgradeButton->GetRelativeLocation().Z) > 2.2)
		return true;
	ActivateInspectedUpgrade();
	return true;
}

void ACityVRToolController::ActivateInspectedUpgrade()
{
	if (!bInspectorUpgradeAvailable)
		return;
	auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	const auto* State = Sim->GetDevelopmentSnapshot().Find(InspectedEntityId);
	if (!State)
	{
		ClearInspection();
		return;
	}
	if (StationUpgradePreviewEntity == InspectedEntityId)
	{
		if (Sim->PurchaseStationUpgrade(InspectedEntityId, StationUpgradePreviewLevel))
			StationUpgradePreviewEntity = 0;
		InspectionGeneration = Sim->GetPresentationGeneration();
	}
	else
	{
		StationUpgradePreviewEntity = InspectedEntityId;
		StationUpgradePreviewLevel = State->DensityLevel;
	}
	UpdateInspectionTooltip();
	return;
}
