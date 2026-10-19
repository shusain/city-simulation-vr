#include "Interaction/CityVRToolController.h"

#include "Components/TextRenderComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

float ACityVRToolController::GetHudRayDistance(const FVector& Origin, const FVector& Direction,
                                               const float MaxDistance) const
{
	float Nearest = MaxDistance;
	auto Consider = [&](const UStaticMeshComponent* Panel)
	{
		if (!Panel || !Panel->IsVisible() || Panel->bHiddenInGame || IsHidden())
			return;
		// These HUD surfaces use the standard 100cm cube, with local X as
		// depth. Intersect its front face analytically: HUD intentionally has no
		// world collision and must still win when terrain is closer to the hand.
		const FTransform Transform = Panel->GetComponentTransform();
		const FVector LocalOrigin = Transform.InverseTransformPosition(Origin);
		const FVector LocalDirection = Transform.InverseTransformVector(Direction);
		if (FMath::Abs(LocalDirection.X) < SMALL_NUMBER)
			return;
		const double Face = LocalDirection.X < 0 ? 50.0 : -50.0;
		const double Distance = (Face - LocalOrigin.X) / LocalDirection.X;
		const FVector Hit = LocalOrigin + LocalDirection * Distance;
		if (Distance >= 0 && Distance < Nearest && FMath::Abs(Hit.Y) <= 50 && FMath::Abs(Hit.Z) <= 50)
			Nearest = Distance;
	};
	for (const auto* Panel : {PaletteBackground.Get(), CityTotalsBackground.Get(), NeedsBackground.Get(),
	                          InspectorBackground.Get(), ObjectiveBackground.Get(), ObjectiveToggle.Get(),
	                          NeedsToggleBackground.Get(), HeatMapLegendBackground.Get(), RoadStatusBackground.Get()})
		Consider(Panel);
	for (const auto& Panel : PaletteButtons)
		Consider(Panel);
	for (const auto& Panel : TimeControlButtons)
		Consider(Panel);
	for (const auto& Panel : ObjectiveFooterButtons)
		Consider(Panel);
	return Nearest;
}

void ACityVRToolController::InitializeHudOverlay()
{
	auto Overlay = [this](UMaterialInterface* Source) -> UMaterialInterface*
	{
		if (!Source)
			return nullptr;
		const FString Name = Source->GetMaterial()->GetName();
		const int32 Target = Name == TEXT("M_CityTextUnlit") ? 0
		                     : Name == TEXT("M_CityUiBar")   ? 1
		                     : Name == TEXT("M_CityUiIcon")  ? 2
		                     : Name == TEXT("M_CityHeatMap") ? 3
		                                                     : INDEX_NONE;
		if (Target == INDEX_NONE)
			return Source;
		UMaterialInterface* Master = HudOverlayMasters.IsValidIndex(Target) ? HudOverlayMasters[Target].Get() : nullptr;
		if (!ensureMsgf(Master, TEXT("Missing HUD overlay material for %s"), *Name))
			return Source;
		UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(Master, this);
		Result->CopyMaterialUniformParameters(Source);
		return Result;
	};
	// Keep later hover, report and utilization updates on the same HUD variants.
	for (TObjectPtr<UMaterialInterface>* Material :
	     {&DemandRailMaterial, &ResidentialDemandMaterial, &CommercialDemandMaterial, &IndustrialDemandMaterial})
		*Material = Overlay(Material->Get());
	for (TObjectPtr<UMaterialInterface>& Material : PaletteIconMaterials)
		Material = Overlay(Material.Get());
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	GetComponents(Primitives);
	int32 Count = 0;
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		// The entry sky and world-attached condition markers keep ordinary depth.
		bool bWorldMarker = Primitive == EntrySky || Primitive == ConditionLeaderInstances;
		for (const auto& Root : ConditionRoots)
			bWorldMarker |= Primitive->IsAttachedTo(Root);
		if (bWorldMarker)
			continue;
		// Procedural previews have material overrides before their sections exist.
		const UMeshComponent* Mesh = Cast<UMeshComponent>(Primitive);
		const int32 Slots = FMath::Max(Primitive->GetNumMaterials(), Mesh ? Mesh->GetNumOverrideMaterials() : 0);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
			Primitive->SetMaterial(Slot, Overlay(Primitive->GetMaterial(Slot)));
		const FString Name = Primitive->GetName();
		// Explicit layers replace depth writes between HUD pieces. Main backplates
		// precede inner cards, controls/bars, then labels and transparent icons.
		int32 Priority = Name.Contains(TEXT("Background")) ? 100 : 120;
		if (Name.StartsWith(TEXT("CityInfoBackground")) || Name == TEXT("GrowthNeedsBackground"))
			Priority = 105;
		if (Name.Contains(TEXT("Rail")) || Name.Contains(TEXT("Zero")))
			Priority = 110;
		if (Primitive->IsA<UTextRenderComponent>() || Name.Contains(TEXT("Icon")))
			Priority = 130;
		if (Primitive == ConstructionLeader)
			Priority = 90;
		Primitive->SetTranslucentSortPriority(Priority);
		++Count;
	}
	ObjectiveIconMaterial = Cast<UMaterialInstanceDynamic>(ObjectiveToggleIcon->GetMaterial(0));
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_HUD_OVERLAY_READY Primitives=%d"), Count);
}
