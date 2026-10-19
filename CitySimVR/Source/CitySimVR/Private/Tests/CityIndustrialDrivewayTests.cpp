#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityIndustrialDriveway.h"
#include "Simulation/CityTerrain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialDrivewayTest,"CitySim.Terrain.IndustrialDriveways",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityIndustrialDrivewayTest::RunTest(const FString&)
{
    FCityTerrainHeightfield Terrain;FCityTerrainGenerationParameters Parameters;Terrain.Generate(Parameters);
    auto& Heights=const_cast<TArray<int32>&>(Terrain.GetQuantizedHeightsCm());
    for(int32 Y=0;Y<Terrain.GetSampleHeight();++Y)
        for(int32 X=0;X<Terrain.GetSampleWidth();++X)
            Heights[Y*Terrain.GetSampleWidth()+X]=FMath::RoundToInt(70*FMath::Sin(X*1.3)+65*FMath::Cos(Y*1.7));
    const auto Before=Heights;
    for(int32 Turn=0;Turn<4;++Turn) for(const double Width:{260.,565.}) for(const double Shift:{0.,400.})
    {
        const FVector Bay(16400,13200,180),F=FRotator(0,Turn*90.,0).RotateVector(FVector(0,-1,0));
        const FVector R=FVector::CrossProduct(FVector::UpVector,F),End=Bay+F*400+R*Shift;
        const auto Public=[&](const FVector& P){return FCityHomeWalk::GroundHeight(Terrain,P)+4.;};
        FCityIndustrialDriveway Drive;Drive.Build(Bay,End,F,Terrain,Public,Width);
        TestTrue(TEXT("Vehicle paving exists without stair/rail geometry"),!Drive.Vertices.IsEmpty() && Drive.StepCount==0 && Drive.RailVertices.IsEmpty());
        int32 Ends=0,Starts=0;double Min=MAX_dbl,Max=-MAX_dbl;
        for(int32 I=0;I<Drive.Vertices.Num();++I) {
            const FVector P=Drive.Vertices[I];if(Drive.Normals[I].Z<=.001) continue;
            TestTrue(TEXT("Upper paving clears terrain at every clipped vertex"),P.Z>=FCityHomeWalk::GroundHeight(Terrain,P)-.01);
            const double Along=FVector::DotProduct(P-Bay,F);
            if(Along<225.-.01)
                TestTrue(TEXT("Raised delivery ramp stays out of the personnel corridor before turning on terrain"),FMath::Abs(FVector::DotProduct(P-Bay,R))<=Width*.5+.01);
            if(FMath::Abs(Along-400)<.01) {
                ++Ends;TestTrue(TEXT("Every public endpoint touches its actual crossfall"),FMath::Abs(P.Z-Public(P))<.01);
                const double Across=FVector::DotProduct(P-End,R);Min=FMath::Min(Min,Across);Max=FMath::Max(Max,Across);
            }
            if(FMath::Abs(Along)<.01) {++Starts;TestTrue(TEXT("Vehicle pad meets the authored bay or below-dock datum"),FMath::Abs(P.Z-Bay.Z)<.01);}
        }
        TestTrue(TEXT("The complete loading width reaches the public surface"),Ends>=2 && Max-Min>=Width-.1 && Starts>=2);
        for(int32 I=0;I<Drive.Triangles.Num();I+=3) {
            const int32 A=Drive.Triangles[I],B=Drive.Triangles[I+1],C=Drive.Triangles[I+2];
            const FVector Cross=FVector::CrossProduct(Drive.Vertices[B]-Drive.Vertices[A],Drive.Vertices[C]-Drive.Vertices[A]);
            TestTrue(TEXT("All exterior triangles are nondegenerate and outward wound"),FVector::DotProduct(Cross,Drive.Normals[A])<-.000001);
            if(Drive.Normals[A].Z>.001) {
                const FVector P=(Drive.Vertices[A]+Drive.Vertices[B]+Drive.Vertices[C])/3.;
                TestTrue(TEXT("Terrain cannot protrude through a vehicle paving triangle"),P.Z>=FCityHomeWalk::GroundHeight(Terrain,P)-.01);
            }
        }
        // Crosswalks start on terrain across their width, not on the level
        // freight sill. A bad inherited centre elevation must be ignored.
        FCityIndustrialDriveway Strip;
        Strip.Build(Bay-FVector(0,0,1000),End,F,Terrain,[&](const FVector& P){return FCityHomeWalk::GroundHeight(Terrain,P)+6.5;},130.,2000.,6.5,false);
        for(int32 I=0;I<Strip.Vertices.Num();++I)
            if(Strip.Normals[I].Z>.001)
                TestTrue(TEXT("Every ground crossing vertex follows its own terrain datum"),FMath::Abs(Strip.Vertices[I].Z-FCityHomeWalk::GroundHeight(Terrain,Strip.Vertices[I])-6.5)<.01);
    }
    TestTrue(TEXT("Driveways never grade the terrain"),Before==Heights);
    return true;
}
#endif
