#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "Presentation/CityWindRotorMotion.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Commands/CityPlacementModel.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWindRotorTest, "CitySim.Presentation.WindRotorRampsAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWindRotorTest::RunTest(const FString& Parameters)
{
	FCityWindRotorMotion Fine, Coarse;
	for (int32 I = 0; I < 100; ++I)
		Fine.Advance(.1, 2);
	Coarse.Advance(10, 2);
	TestTrue(TEXT("Frame-independent ramp speed"), FMath::IsNearlyEqual(Fine.Speed, Coarse.Speed, 1.e-8));
	TestTrue(TEXT("Frame-independent angle integration"),
	         FMath::IsNearlyEqual(Fine.Angle, Coarse.Angle, 1.e-8));
	TestTrue(TEXT("Acceleration approaches target without overshoot"), Fine.Speed > 170 && Fine.Speed < 180);
	const double Fast = Fine.Speed;
	Fine.Advance(.1, 0);
	TestTrue(TEXT("Calm wind coasts instead of stopping instantly"), Fine.Speed > 0 && Fine.Speed < Fast);
	Fine.Advance(30, 0);
	TestTrue(TEXT("Calm settles to rest"), Fine.Speed < .01);
	const double Before = Fine.Angle;
	Fine.Advance(-1, 2);
	Fine.Advance(std::numeric_limits<double>::quiet_NaN(), 2);
	TestEqual(TEXT("Invalid delta cannot corrupt presentation"), Fine.Angle, Before);
	TestTrue(TEXT("Axle remains fixed during rotation"),
	         Coarse.Transform(FVector(0,290,83.500003814697)).TransformPosition(FVector::ZeroVector).Equals(FVector(0,290,83.500003814697)));
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WindRotorTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	const auto* Definition = Catalog->FindDefinition(TEXT("WindPower"));
	auto Preview = City.BeginPreview(*Definition, {8, 9});
	const uint64 Id = City.CommitPlacement(Preview, *Definition).EntityId;
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = Catalog;
	Board->DispatchBeginPlay();
	// Direct model edits in this fixture bypass command-driven generation changes.
	++Sim->PresentationGeneration;
	Board->PrepareCityView();
	TestNotNull(TEXT("Split turbine body imported"), Board->WindBodyMesh.Get());
	TestNotNull(TEXT("Axle rotor imported"), Board->WindRotorInstances->GetStaticMesh().Get());
	TestNotNull(TEXT("Independent yaw nacelle imported"),Board->WindNacelleInstances->GetStaticMesh().Get());
	Sim->SetSimulationPaused(false);
	Board->UpdateWindRotors(.1f);
	TestEqual(TEXT("One pooled rotor per completed intact turbine"),
	          Board->WindRotorInstances->GetInstanceCount(), 1);
	TestEqual(TEXT("One pooled yaw nacelle per intact turbine"),Board->WindNacelleInstances->GetInstanceCount(),1);
	const auto Presentation=Board->ResolvePresentationDefinition(Id,*Definition);
	for(int32 Turn=0;Turn<4;++Turn)
	{
		const auto Pose=Board->MakeBuildingTransform({8,9},Presentation,Turn);
		TestTrue(TEXT("Door and mast keep placed orientation independent of weather"),
		    Pose.GetRotation().Equals(FRotator(0,((Turn+2)%4)*90.,0).Quaternion(),.0001));
	}
	FCityWindRotorMotion Quarter;
	Quarter.Angle=90.;
	const auto QuarterPose=Quarter.Transform(FVector(0,290,83.500003814697));
	TestTrue(TEXT("Reflected source positive spin uses native +Y axial rotation"),
	    QuarterPose.TransformVector(FVector(1,0,0)).Equals(FVector(0,0,-1),.0001));
	FTransform NacellePose,RotorPose;
	Board->WindNacelleInstances->GetInstanceTransform(0,NacellePose);
	Board->WindRotorInstances->GetInstanceTransform(0,RotorPose);
	TestTrue(TEXT("Rotor centre stays on the measured nacelle-local axle"),
	    NacellePose.InverseTransformPosition(RotorPose.GetLocation()).Equals(FVector(0,290,83.500003814697),.001));
	TestTrue(TEXT("Completed mesh replaces welded blades"),
	         Board->ResolvePresentationDefinition(Id, *Definition).PresentationMesh.Get() ==
	             Board->WindBodyMesh.Get());
	FCityDesktopModelSource Inspector;
	TestTrue(TEXT("Wind inspector resolves current entity"),Board->GetDesktopModelSource(Id,Inspector));
	TestTrue(TEXT("Inspector retains the complete approved turbine rather than the mast alone"),Inspector.Mesh && Inspector.Mesh->GetName()==TEXT("SM_wind_turbine_v1"));
	const double Angle = Board->WindRotorMotion.FindChecked(Id).Angle;
	Sim->SetSimulationPaused(true);
	Board->UpdateWindRotors(5);
	TestEqual(TEXT("Pause freezes angle"), Board->WindRotorMotion.FindChecked(Id).Angle, Angle);
	Sim->SetSimulationPaused(false);
	Sim->SetSimulationSpeed(5);
	Board->WindRotorMotion.FindChecked(Id) = FCityWindRotorMotion();
	Board->UpdateWindRotors(.1f);
	TestTrue(TEXT("Game speed does not multiply visual RPM"),
	         FMath::IsNearlyEqual(Board->WindRotorMotion.FindChecked(Id).Angle, Angle, 1.e-6));
	auto Fire = Sim->Fire.GetSnapshot();
	Fire.DamagedBuildings.Add(Id, {100, -1, 2});
	FString Error;
	TestTrue(TEXT("Fire fixture restore"), Sim->Fire.RestoreState(Fire, 2, Error));
	++Sim->PresentationGeneration;
	Board->UpdateWindRotors(.1f);
	TestEqual(TEXT("Damage removes animated rotor"), Board->WindRotorInstances->GetInstanceCount(), 0);
	TestEqual(TEXT("Damage removes separate nacelle rather than overlaying the whole damaged body"),Board->WindNacelleInstances->GetInstanceCount(),0);
	TestEqual(TEXT("Damage removes motion state"), Board->WindRotorMotion.Num(), 0);
	Fire.DamagedBuildings.Reset();
	TestTrue(TEXT("Fire fixture restore"), Sim->Fire.RestoreState(Fire, 2, Error));
	++Sim->PresentationGeneration;
	Board->UpdateWindRotors(.1f);
	TestEqual(TEXT("Repair restores exactly one rotor"), Board->WindRotorInstances->GetInstanceCount(), 1);
	TestEqual(TEXT("Repair restores exactly one nacelle"),Board->WindNacelleInstances->GetInstanceCount(),1);
	World->DestroyWorld(false);
	return true;
}
#endif
