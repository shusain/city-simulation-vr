#include "Presentation/CityIndustrialDriveway.h"
#include "Simulation/CityTerrain.h"
#include "Algo/Reverse.h"

void FCityIndustrialDriveway::Build(const FVector& Bay,const FVector& PublicEdge,const FVector& Outward,
    const FCityTerrainHeightfield& Terrain,TFunctionRef<double(const FVector&)> PublicHeight,
    const double WidthCm,const double GroundReserveCm,const double TerrainLiftCm,const bool bLevelStart)
{
    *this=FCityIndustrialDriveway();
    const FVector F=Outward.GetSafeNormal2D(),R=FVector::CrossProduct(FVector::UpVector,F);
    const double Length=FVector::DotProduct(PublicEdge-Bay,F),Shift=FVector::DotProduct(PublicEdge-Bay,R),Half=WidthCm*.5;
    if(Length<1. || WidthCm<1.) return;
    const double Run=FMath::Max(1.,Length-GroundReserveCm);
    auto Along=[&](const FVector& P){return FVector::DotProduct(P-Bay,F);};
    // Keep the raised ramp opposite the bay. Turn toward a partial road
    // frontage only after reaching the terrain forecourt, clear of stairs.
    auto Across=[&](const FVector& P){return FVector::DotProduct(P-Bay,R)-Shift*FMath::Clamp((Along(P)-Run)/FMath::Max(.001,Length-Run),0.,1.);};
    auto Height=[&](const FVector& P){
        const double Y=FMath::Clamp(Along(P),0.,Length),X=Across(P);
        const FVector End=PublicEdge+R*X;
        const double G=GroundHeight(Terrain,P),Public=PublicHeight(End);
        if(Y>=Length-.00001) return Public;
        if(Y<=.00001) return bLevelStart?Bay.Z:G+TerrainLiftCm;
        // Leave a shared terrain strip at the front of the plot for a
        // personnel path to cross a delivery lane without crossing stairs.
        const double GroundLift=FMath::Lerp(TerrainLiftCm,Public-GroundHeight(Terrain,End),FMath::Clamp((Y-Run)/(Length-Run),0.,1.));
        // The public road may use a4cm lift. Once on the ground strip, follow
        // its actual6→4cm transition instead of retaining a6cm minimum.
        if(Y>=Run) return G+GroundLift;
        return bLevelStart?FMath::Max(G+GroundLift,FMath::Lerp(Bay.Z,G+TerrainLiftCm,FMath::Min(1.,Y/Run))):G+GroundLift;
    };
    auto Face=[&](TArray<FVector> P,FVector Normal){
        for(int32 I=P.Num()-1;I>=0;--I)
            if(P.Num()>1 && P[I].Equals(P[(I+1)%P.Num()],.00001)) P.RemoveAt(I);
        if(P.Num()<3) return;
        while(P.Num()>3 && FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).SizeSquared()<.00001) P.RemoveAt(1);
        if(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).SizeSquared()<.00001) return;
        if(Normal.IsNearlyZero()) {Normal=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal();if(Normal.Z<0) Normal=-Normal;}
        if(FVector::DotProduct(FVector::CrossProduct(P[1]-P[0],P[2]-P[0]),Normal)>0) Algo::Reverse(P);
        const int32 Base=Vertices.Num();
        for(const auto& V:P){Vertices.Add(V);Normals.Add(Normal);UVs.Add(FVector2D(V.X,V.Y)/100.);}
        for(int32 I=1;I+1<P.Num();++I)
            if(FVector::CrossProduct(P[I]-P[0],P[I+1]-P[0]).SizeSquared()>.00001) Triangles.Append({Base,Base+I,Base+I+1});
    };
    auto Clip=[](TArray<FVector> P,TFunctionRef<double(const FVector&)> Distance){
        TArray<FVector> Result;
        for(int32 I=0;I<P.Num();++I){const FVector V=P[I],W=P[(I+1)%P.Num()];const double A=Distance(V),B=Distance(W);
            if(A>=-.000001) Result.Add(V);
            if((A>0 && B<0)||(A<0 && B>0)) Result.Add(FMath::Lerp(V,W,A/(A-B)));
        }return Result;
    };
    const FBox Bounds(TArray<FVector>{Bay-R*Half,Bay+R*Half,Bay+F*Run-R*Half,Bay+F*Run+R*Half,PublicEdge-R*Half,PublicEdge+R*Half});
    for(int32 Y=FMath::FloorToInt(Bounds.Min.Y/400.);Y<=FMath::FloorToInt(Bounds.Max.Y/400.);++Y)
        for(int32 X=FMath::FloorToInt(Bounds.Min.X/400.);X<=FMath::FloorToInt(Bounds.Max.X/400.);++X)
            FCityTerrainSurface::Facets(Terrain,X,Y,[&](const FVector& A,const FVector& B,const FVector& C){
                TArray<FVector> P{A,B,C};
                P=Clip(P,[&](const FVector& V){return Along(V);});P=Clip(P,[&](const FVector& V){return Length-Along(V);});
                // Split at the grade/terrain transition so no triangle bridges
                // over the shared front strip or cuts through a terrain crest.
                for(const bool Front:{false,true}) {
                    auto Part=Clip(P,[&](const FVector& V){return Front?Along(V)-Run:Run-Along(V);});
                    Part=Clip(Part,[&](const FVector& V){return Across(V)+Half;});Part=Clip(Part,[&](const FVector& V){return Half-Across(V);});
                    for(auto& V:Part) V.Z=Height(V);
                    Face(Part,FVector::ZeroVector);
                    for(int32 I=0;I<Part.Num();++I){const FVector V=Part[I],W=Part[(I+1)%Part.Num()];
                        const double XV=Across(V),XW=Across(W),YV=Along(V),YW=Along(W);
                        FVector N=FVector::ZeroVector;
                        const FVector SideNormal=(R-F*(Front?Shift/FMath::Max(.001,Length-Run):0.)).GetSafeNormal();
                        if(FMath::IsNearlyEqual(XV,Half,.01)&&FMath::IsNearlyEqual(XW,Half,.01)) N=SideNormal;
                        if(FMath::IsNearlyEqual(XV,-Half,.01)&&FMath::IsNearlyEqual(XW,-Half,.01)) N=-SideNormal;
                        if(FMath::Abs(YV)<.01 && FMath::Abs(YW)<.01) N=-F;
                        if(N.IsNearlyZero()) continue;
                        FVector LowV=V,LowW=W;LowV.Z=GroundHeight(Terrain,V)-10.;LowW.Z=GroundHeight(Terrain,W)-10.;
                        Face({V,W,LowW,LowV},N);
                    }
                }
            });
}
