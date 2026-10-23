#include "Presentation/CityBoardActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Interaction/CityBoardPicking.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACityBoardActor::UpdatePlacementFootprint(const FCityBuildingDefinition& Definition, const bool bValid)
{
    PlacementFootprintInstances->ClearInstances();
    if (!PlacementFootprintMaterial)
    {
        PlacementFootprintMaterial = UMaterialInstanceDynamic::Create(PlacementFootprintInstances->GetMaterial(0), this);
        PlacementFootprintInstances->SetMaterial(0, PlacementFootprintMaterial);
    }
    PlacementFootprintMaterial->SetVectorParameterValue(TEXT("BarColor"),
        bValid ? FLinearColor(.03f, 1.f, .12f) : FLinearColor(1.f, .04f, .015f));
    const FIntPoint Size = Definition.Footprint.GetRotatedSize(PlacementPreview.QuarterTurnsClockwise);
    const double Cell = CityBoardPicking::LogicalCellSizeCm;
    const double X = PlacementPreview.Anchor.X * Cell, Y = PlacementPreview.Anchor.Y * Cell;
    const FVector Corners[] = {{X,Y,0}, {X+Size.X*Cell,Y,0},
        {X+Size.X*Cell,Y+Size.Y*Cell,0}, {X,Y+Size.Y*Cell,0}};
    const double Lift = DiagnosticOverlay == ECityDiagnosticOverlay::None ? 110. : 310.;
    const auto Height = [&](FVector P) {
        P.Z = (SimulationSubsystem ? SimulationSubsystem->GetTerrain().SampleHeightCm(P.X/Cell,P.Y/Cell) : 0) + Lift;
        return P;
    };
    for (int32 Side=0; Side<4; ++Side)
    {
        const int32 Segments = (Side%2 ? Size.Y : Size.X)*4;
        // The thicker edge identifies the main entrance while the entire reserved plot stays visible.
        const double Width = Side == PlacementPreview.QuarterTurnsClockwise%4 ? 72 : 36;
        for (int32 I=0; I<Segments; ++I)
        {
            const FVector A=Height(FMath::Lerp(Corners[Side],Corners[(Side+1)%4],double(I)/Segments));
            const FVector B=Height(FMath::Lerp(Corners[Side],Corners[(Side+1)%4],double(I+1)/Segments));
            PlacementFootprintInstances->AddInstance(FTransform((B-A).Rotation(),(A+B)*.5,
                FVector((B-A).Size()/100,Width/100,8./100)));
        }
    }
    PlacementFootprintInstances->SetVisibility(true);
}
