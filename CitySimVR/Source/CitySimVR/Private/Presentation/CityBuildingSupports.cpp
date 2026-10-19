#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityPrivateEntrances.h"
#include "Interaction/CityBoardPicking.h"
#include "Misc/Paths.h"
#include "Simulation/CityTerrain.h"

TConstArrayView<FCityBuildingSupportRegion> CityBuildingSupports::Find(const FString& MeshPackage)
{
	// Damage retains the intact structural footprint. Construction and the
	// animated wind-body mesh resolve through the definition's intended mesh.
    const FString MeshName = FPaths::GetBaseFilename(MeshPackage);
    const auto* Ground = CityResidentialEntrances::Find(MeshName);
    if (!Ground)
        if (const auto* Commercial = CityCommercialEntrances::Find(MeshName))
            if (MeshName == FPaths::GetBaseFilename(Commercial->TargetMesh)) Ground = Commercial;
    if(!Ground)
        if(const auto* Industrial=CityIndustrialEntrances::Find(MeshName))
            if(MeshName==FPaths::GetBaseFilename(Industrial->TargetMesh)) Ground=Industrial;
    if(!Ground)
        if(const auto* Civic=CityCivicEntrances::Find(MeshName))
            if(MeshName==FPaths::GetBaseFilename(Civic->TargetMesh)) Ground=Civic;
    if(!Ground)
        if(const auto* Utility=CityUtilityEntrances::Find(MeshName))
            if(MeshName==FPaths::GetBaseFilename(Utility->TargetMesh)) Ground=Utility;
    // Unmodified originals retain their original measurements. Only the
    // commercial derivative has the rear setback and structural entrance floor.
    FName Name(Ground ? Ground->SupportMesh : *MeshName.Replace(TEXT("_Damaged"), TEXT("")));
    if(Name==TEXT("SM_Building_Residential_Construction_A"))
    {
        // The measured15m structural work deck is retained; no full16m
        // parcel slab or old3m ground walk is part of this support region.
        static const FCityBuildingSupportRegion Regions[]={{-750,-750,750,750,0}};
        return MakeArrayView(Regions);
    }
#include "CityApprovedCivicSupportData.inl"
#include "CityCivicSupportData.inl"
#include "CityUtilitySupportData.inl"
#include "CityIndustrialSupportData.inl"
#include "CityCommercialSupportData.inl"
#include "CityBuildingSupportData.inl"
	return {};
}

void CityBuildingSupports::MakeTransforms(const TConstArrayView<FCityBuildingSupportRegion> Regions,
                                          const FTransform& BuildingPose, const FCityTerrainHeightfield& Terrain,
                                          TArray<FTransform>& OutTransforms)
{
	OutTransforms.Reset();
	constexpr double Cell = CityBoardPicking::LogicalCellSizeCm;
	for (const auto& R : Regions)
	{
		const FTransform SupportPose=FTransform(FRotator(0,R.YawDegrees,0))*BuildingPose;
		// Slightly inset vertical faces avoid coincident wall/support edges.
		const FVector Low(R.MinX + .5, R.MinY + .5, R.BottomZ);
		const FVector High(R.MaxX - .5, R.MaxY - .5, R.BottomZ);
		FBox Bounds(ForceInit);
		double Minimum = MAX_dbl;
		for (const FVector Corner : {Low, High, FVector(Low.X, High.Y, R.BottomZ), FVector(High.X, Low.Y, R.BottomZ)})
		{
			const FVector P = SupportPose.TransformPosition(Corner);
			Bounds += P;
			Minimum = FMath::Min(Minimum, Terrain.SampleHeightCm(P.X / Cell, P.Y / Cell));
		}
		// Include the fine terrain lattice, not only the footprint corners. The
		// surrounding sample ring is conservative for partially crossed triangles.
		for (int32 Y = FMath::FloorToInt(Bounds.Min.Y / Cell * Terrain.SamplesPerCell);
		     Y <= FMath::CeilToInt(Bounds.Max.Y / Cell * Terrain.SamplesPerCell); ++Y)
			for (int32 X = FMath::FloorToInt(Bounds.Min.X / Cell * Terrain.SamplesPerCell);
			     X <= FMath::CeilToInt(Bounds.Max.X / Cell * Terrain.SamplesPerCell); ++X)
				Minimum = FMath::Min(Minimum, static_cast<double>(Terrain.GetSampleHeightCm(X, Y)));
		const double Top = Bounds.Max.Z + 1.0; // One cm into the structural floor hides seams.
		const double Bottom = FMath::Min(Top - 40.0, Minimum - 20.0);
		FVector Center = SupportPose.TransformPosition((Low + High) * .5);
		Center.Z = (Bottom + Top) * .5;
		OutTransforms.Emplace(SupportPose.GetRotation(), Center,
		                      FVector((High.X - Low.X) / 100.0, (High.Y - Low.Y) / 100.0, (Top - Bottom) / 100.0));
	}
}
