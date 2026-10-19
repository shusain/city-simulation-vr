#include "Presentation/CityUtilityFoundations.h"
#include "Presentation/CityUtilityEntrances.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CityTerrain.h"

TConstArrayView<FCityUtilityFoundationGroup> CityUtilityFoundations::All()
{
    static const FCityUtilityFoundationGroup Groups[]={
#include "CityApprovedUtilityFoundationData.inl"
    };
    return MakeArrayView(Groups);
}

TConstArrayView<FCityUtilityFoundationGroup> CityUtilityFoundations::Find(const FString& MeshName)
{
    const auto* Entry=CityUtilityEntrances::Find(MeshName);
    if(!Entry) return {};
    const auto Groups=All();
    int32 First=INDEX_NONE,Count=0;
    for(int32 I=0;I<Groups.Num();++I)
        if(FString(Entry->SupportMesh)==Groups[I].AssetKey)
        {
            if(First==INDEX_NONE) First=I;
            ++Count;
        }
    return First==INDEX_NONE?TConstArrayView<FCityUtilityFoundationGroup>():Groups.Slice(First,Count);
}

FTransform CityUtilityFoundations::MakeTransform(const FCityUtilityFoundationGroup& Group,
    const FTransform& BuildingPose,const FCityTerrainHeightfield& Terrain)
{
    constexpr double Cell=CityBoardPicking::LogicalCellSizeCm;
    FBox Bounds(ForceInit);
    double Minimum=MAX_dbl;
    for(const double X:{Group.MinCm.X,Group.MaxCm.X})
        for(const double Y:{Group.MinCm.Y,Group.MaxCm.Y})
        {
            const FVector P=BuildingPose.TransformPosition(FVector(X,Y,Group.TopCm));
            Bounds+=P;
            Minimum=FMath::Min(Minimum,Terrain.SampleHeightCm(P.X/Cell,P.Y/Cell));
        }
    // A low point within the group bounds gives every disconnected column
    // adequate buried depth. It does not add concrete between their contours.
    for(int32 Y=FMath::FloorToInt(Bounds.Min.Y/Cell*Terrain.SamplesPerCell);
        Y<=FMath::CeilToInt(Bounds.Max.Y/Cell*Terrain.SamplesPerCell);++Y)
        for(int32 X=FMath::FloorToInt(Bounds.Min.X/Cell*Terrain.SamplesPerCell);
            X<=FMath::CeilToInt(Bounds.Max.X/Cell*Terrain.SamplesPerCell);++X)
            Minimum=FMath::Min(Minimum,static_cast<double>(Terrain.GetSampleHeightCm(X,Y)));
    const double Top=Bounds.Max.Z+1.;
    const double Bottom=FMath::Min(Top-40.,Minimum-20.);
    FVector Location=BuildingPose.GetLocation();
    Location.Z=Top;
    return FTransform(BuildingPose.GetRotation(),Location,FVector(1,1,(Top-Bottom)/100.));
}
