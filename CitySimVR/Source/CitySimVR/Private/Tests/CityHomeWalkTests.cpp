#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityHomeWalk.h"
#include "Simulation/CityTerrain.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHomeWalkTest, "CitySim.Terrain.CompactHomeWalk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHomeWalkTest::RunTest(const FString&)
{
	auto* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot.SM_CompactHome_GroundPilot"));
	if (!TestNotNull(TEXT("native pathless home exists"), Mesh)) return false;
	TestTrue(TEXT("removed walk no longer extends below body pivot"), Mesh->GetBoundingBox().Min.Z >= -.1);
	TestEqual(TEXT("four approved home material slots remain"), Mesh->GetStaticMaterials().Num(), 4);
	TestNotNull(TEXT("concrete is an explicit cooked material"), LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CitySim/Art/HomeGroundPilot/M_Concrete.M_Concrete")));
	FCityTerrainHeightfield Terrain; FCityTerrainGenerationParameters Parameters; Terrain.Generate(Parameters);
	auto& Heights = const_cast<TArray<int32>&>(Terrain.GetQuantizedHeightsCm());
	for (int32 Fixture = 0; Fixture < 4; ++Fixture)
	{
		for (int32 Y = 0; Y < Terrain.GetSampleHeight(); ++Y)
			for (int32 X = 0; X < Terrain.GetSampleWidth(); ++X)
				Heights[Y * Terrain.GetSampleWidth() + X] = FMath::RoundToInt(70 * FMath::Sin(X * 1.3 + Fixture) + 65 * FMath::Cos(Y * 1.7 - Fixture));
		const auto Before = Heights;
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			const FVector Door(16400, 13200, 180);
			const FVector End = Door + FRotator(0, Turn * 90., 0).RotateVector(FVector(0,-470,0));
			FCityHomeWalk Walk;
			auto PublicHeight = [&](const FVector& P) { return FCityHomeWalk::GroundHeight(Terrain, P) + 12.; };
			Walk.Build(Door, End, Terrain, PublicHeight);
			TestTrue(TEXT("nonplanar rotated fixture produces a paved walk and steps"), !Walk.Vertices.IsEmpty() && Walk.StepCount > 0);
			const FVector Direction = (End - Door).GetSafeNormal2D();
			int32 Ends = 0, GroundFaces = 0;
			for (int32 I = 0; I < Walk.Vertices.Num(); ++I)
			{
				const auto P = Walk.Vertices[I];
				if (Walk.Normals[I].Z < .5) continue; // buried edges are deliberate.
				TestTrue(TEXT("upper vertices clear the actual triangulated terrain"), P.Z >= FCityHomeWalk::GroundHeight(Terrain, P) - .01);
				const double Along = FVector::DotProduct(P - Door, Direction);
				if (FMath::Abs(Along - 470) < .01) { ++Ends; TestTrue(TEXT("both sidewalk edges meet their own surface elevation"), FMath::Abs(P.Z - PublicHeight(P)) < .01); }
				if (FMath::Abs(P.Z - FCityHomeWalk::GroundHeight(Terrain,P) - 6) < .01) ++GroundFaces;
			}
			TestTrue(TEXT("path reaches public edge across width"), Ends >= 2);
			TestTrue(TEXT("walk contains terrain-following surface beyond stairs"), GroundFaces > 0);
			for (int32 I = 0; I < Walk.Triangles.Num(); I += 3)
			{
				const int32 A = Walk.Triangles[I], B = Walk.Triangles[I+1], C = Walk.Triangles[I+2];
				TestTrue(TEXT("all faces point toward the viewer from their exterior"), FVector::DotProduct(FVector::CrossProduct(Walk.Vertices[B]-Walk.Vertices[A],Walk.Vertices[C]-Walk.Vertices[A]), Walk.Normals[A]) < 0);
				if (Walk.Normals[A].Z > .5) {
					const FVector Center = (Walk.Vertices[A]+Walk.Vertices[B]+Walk.Vertices[C])/3.;
					TestTrue(TEXT("terrain cannot protrude through the interior of a tread or paving triangle"), Center.Z >= FCityHomeWalk::GroundHeight(Terrain,Center)-.01);
				}
			}
		}
		TestTrue(TEXT("private paving never grades or changes terrain"), Before == Heights);
	}
    // A flat home no longer descends to ground then climbs a61cm road shelf.
    for(auto& H:Heights) H=0;
    for(const int32 Count:{2,4,5,9}) for(int32 Turn=0;Turn<4;++Turn) {
        const FVector Door(16400,13200,6+Count*18);
        const FVector Forward=FRotator(0,Turn*90.,0).RotateVector(FVector(0,-1,0));
        FCityHomeWalk Walk; Walk.Build(Door,Door+Forward*700,Terrain,[](const FVector&){return 12.;});
        TestEqual(TEXT("Actual flat riser count"),Walk.ActualStepCount,Count);
        TestEqual(TEXT("Both rails appear only above four actual steps"),!Walk.RailVertices.IsEmpty(),Count>4);
        for(int32 I=0;I<Walk.Vertices.Num();++I) {
            const double Along=FVector::DotProduct(Walk.Vertices[I]-Door,Forward);
            if(Along>600 && Walk.Normals[I].Z>.5) TestTrue(TEXT("Final approach has only the small curb transition"),Walk.Vertices[I].Z<=12.01);
        }
        for(int32 I=0;I<Walk.RailTriangles.Num();I+=3) {
            const int32 A=Walk.RailTriangles[I],B=Walk.RailTriangles[I+1],C=Walk.RailTriangles[I+2];
            TestTrue(TEXT("Rotated railing faces have outward winding"),FVector::DotProduct(FVector::CrossProduct(Walk.RailVertices[B]-Walk.RailVertices[A],Walk.RailVertices[C]-Walk.RailVertices[A]),Walk.RailNormals[A])<0);
        }
        if(Count>4) {
            const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
            bool Left=false,RightSide=false;
            for(const auto& V:Walk.RailVertices) {const double Across=FVector::DotProduct(V-Door,Right);Left|=Across<-55;RightSide|=Across>55;TestTrue(TEXT("Rails leave the pedestrian opening clear"),FMath::Abs(Across)>=57.9);}
            TestTrue(TEXT("Long flights have rails on both sides"),Left && RightSide);
        }
    }
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFrontageWalkTest,"CitySim.Terrain.ResidentialFrontageWalk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFrontageWalkTest::RunTest(const FString&)
{
    FCityTerrainHeightfield Terrain;FCityTerrainGenerationParameters Parameters;Terrain.Generate(Parameters);
    auto& Heights=const_cast<TArray<int32>&>(Terrain.GetQuantizedHeightsCm());
    for(int32 Fixture=0;Fixture<2;++Fixture)
    {
        for(int32 Y=0;Y<Terrain.GetSampleHeight();++Y)
            for(int32 X=0;X<Terrain.GetSampleWidth();++X)
                Heights[Y*Terrain.GetSampleWidth()+X]=Fixture?FMath::RoundToInt(70*FMath::Sin(X*1.3)+65*FMath::Cos(Y*1.7)):0;
        const auto Before=Heights;
        for(int32 Turn=0;Turn<4;++Turn) for(const double Sign:{-1.,1.})
            for(const double Width:{80.,100.,130.}) for(const double PublicLift:{4.,12.})
        {
            const FVector Door(16400,13200,230);
            const FVector F=FRotator(0,Turn*90.,0).RotateVector(FVector(0,-1,0));
            const FVector R=FVector::CrossProduct(FVector::UpVector,F)*Sign;
            const double Depth=Width+(Width==130.?22.:2.);
            const FVector Public=Door+R*(800.-Width*.5)+F*Depth;
            auto PublicHeight=[&](const FVector& P){return FCityHomeWalk::GroundHeight(Terrain,P)+PublicLift;};
            FCityHomeWalk Walk;Walk.BuildFrontage(Door,Public,F,Terrain,PublicHeight,Width,300,10,60);
            TestTrue(TEXT("Frontage flight and corner apron are present"),!Walk.Vertices.IsEmpty() && Walk.ActualStepCount>4 && !Walk.RailVertices.IsEmpty());
            int32 Contacts=0,Thresholds=0;
            for(int32 I=0;I<Walk.Vertices.Num();++I)
            {
                const FVector P=Walk.Vertices[I];if(Walk.Normals[I].Z<.5) continue;
                TestTrue(TEXT("Frontage upper vertices clear the actual terrain"),P.Z>=FCityHomeWalk::GroundHeight(Terrain,P)-.01);
                const double X=FVector::DotProduct(P-Door,R),Y=FVector::DotProduct(P-Door,F);
                TestTrue(TEXT("Stairs remain inside the reserved frontage"),X>=-150.01 && X<=800.01 && Y>=-.01 && Y<=Depth+.01);
                if(FMath::Abs(Y-Depth)<.01) {++Contacts;TestTrue(TEXT("Public apron touches actual sidewalk across its width"),FMath::Abs(P.Z-PublicHeight(P))<.01);}
                if(FMath::Abs(Y)<.01 && FMath::Abs(X)<=150.01 && FMath::Abs(P.Z-Door.Z)<.01) ++Thresholds;
            }
            TestTrue(TEXT("Door landing and full-width public join exist"),Thresholds>=2 && Contacts>=2);
            for(int32 I=0;I<Walk.Triangles.Num();I+=3)
            {
                const int32 A=Walk.Triangles[I],B=Walk.Triangles[I+1],C=Walk.Triangles[I+2];
                const double Winding=FVector::DotProduct(FVector::CrossProduct(Walk.Vertices[B]-Walk.Vertices[A],Walk.Vertices[C]-Walk.Vertices[A]),Walk.Normals[A]);
                TestTrue(FString::Printf(TEXT("Frontage winding fixture%d turn%d side%.0f width%.0f public%.0f triangle%d dot%.6f"),Fixture,Turn,Sign,Width,PublicLift,I/3,Winding),Winding<0);
                if(Walk.Normals[A].Z>.5) {
                    const FVector P=(Walk.Vertices[A]+Walk.Vertices[B]+Walk.Vertices[C])/3.;
                    TestTrue(TEXT("Terrain does not cut the stair/apron interior"),P.Z>=FCityHomeWalk::GroundHeight(Terrain,P)-.01);
                }
            }
        }
        TestTrue(TEXT("Frontage treatment preserves terrain"),Before==Heights);
    }
    return true;
}
#endif
