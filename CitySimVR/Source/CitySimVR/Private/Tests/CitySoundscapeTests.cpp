#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Presentation/CitySoundscapePolicy.h"
#include "Presentation/CitySoundscapeComponent.h"
#include "Presentation/CityAudioSettings.h"
#include "Presentation/CityBoardActor.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "MetasoundSource.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeDoppler.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySoundscapePolicyTest, "CitySim.Audio.EligibilityAndTransitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySoundscapePolicyTest::RunTest(const FString& Parameters)
{
	FCityTrafficVehicleState Vehicle;
	for (auto Type :
	     {ECityTripVehicleType::PoliceCar, ECityTripVehicleType::FireEngine, ECityTripVehicleType::Ambulance})
	{
		Vehicle.VehicleType = Type;
		Vehicle.bEmergencyResponding = true;
		Vehicle.bReturning = false;
		Vehicle.bCompleted = false;
		TestTrue(TEXT("Active outbound crews have sirens"), CitySoundscape::IsResponding(Vehicle));
		Vehicle.bCompleted = true;
		TestFalse(TEXT("Arrival stops sirens"), CitySoundscape::IsResponding(Vehicle));
		Vehicle.bCompleted = false;
		Vehicle.bReturning = true;
		TestFalse(TEXT("Returning crews stay quiet"), CitySoundscape::IsResponding(Vehicle));
		Vehicle.bReturning = false;
		Vehicle.bEmergencyResponding = false;
		TestFalse(TEXT("Parked crews stay quiet"), CitySoundscape::IsResponding(Vehicle));
	}
	Vehicle.bEmergencyResponding = true;
	Vehicle.VehicleType = ECityTripVehicleType::GarbageTruck;
	TestFalse(TEXT("Ordinary/service traffic never gets an emergency siren"), CitySoundscape::IsResponding(Vehicle));
	CitySoundscape::FTransitionGate Gate;
	TestFalse(TEXT("Load baselines current state"), Gate.Observe(1, false, 12));
	TestTrue(TEXT("New actionable edge is delivered"), Gate.Observe(1, false, 13));
	TestFalse(TEXT("Repeated high-speed frames do not duplicate"), Gate.Observe(1, false, 13));
	TestFalse(TEXT("Paused changes are consumed"), Gate.Observe(1, true, 14));
	TestFalse(TEXT("Resume does not replay"), Gate.Observe(1, false, 14));
	TestFalse(TEXT("New city's older counters cannot produce stale events"), Gate.Observe(2, false, 1));
	TestTrue(TEXT("New city future edge is still audible"), Gate.Observe(2, false, 2));
	TestEqual(TEXT("Envelope reaches center"), CitySoundscape::AreaWeight(0, 10), 1.f);
	TestEqual(TEXT("Envelope is silent beyond its boundary"), CitySoundscape::AreaWeight(11, 10), 0.f);
	TestEqual(TEXT("Invalid input is silent"), CitySoundscape::AreaWeight(std::numeric_limits<double>::quiet_NaN(), 10),
	          0.f);
	TestTrue(TEXT("Near boundary weights change smoothly"),
	         FMath::Abs(CitySoundscape::AreaWeight(9.9, 10) - CitySoundscape::AreaWeight(10, 10)) < .001);
	TestTrue(TEXT("Zoom reveals local detail"), CitySoundscape::LocalDetail(4) > CitySoundscape::LocalDetail(40));
	const float One = CitySoundscape::Approach(0, 1, 1);
	const float Split = CitySoundscape::Approach(CitySoundscape::Approach(0, 1, .4), 1, .6);
	TestTrue(TEXT("Mix smoothing follows wall time rather than frame/tick count"),
	         FMath::IsNearlyEqual(One, Split, .00001f));
	TestEqual(TEXT("Police variation replaces its primary cadence"),
	          CitySoundscape::AccentPrimaryGain(ECityAudioCue::PoliceLoop, ECityAudioCue::PoliceAlternate), 0.f);
	TestEqual(TEXT("Fire horn halves its primary gain"),
	          CitySoundscape::AccentPrimaryGain(ECityAudioCue::FireLoop, ECityAudioCue::FireHorn), .5f);
	TestEqual(TEXT("An unrelated accent cannot duck another response family"),
	          CitySoundscape::AccentPrimaryGain(ECityAudioCue::AmbulanceLoop, ECityAudioCue::FireHorn), 1.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySoundscapeBankTest, "CitySim.Audio.NativeBankAndExplicitLoops",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySoundscapeBankTest::RunTest(const FString& Parameters)
{
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({TEXT("/Game")}, true);
	for (const TCHAR* Retired : {
	         TEXT("/Game/CitySim/Audio/Soundscape/Ambience/City/SW_Car_Pass"),
	         TEXT("/Game/CitySim/Audio/Soundscape/Ambience/City/SW_Truck_Pass"),
	         TEXT("/Game/CitySim/Audio/Soundscape/Graphs/MS_Car_Pass"),
	         TEXT("/Game/CitySim/Audio/Soundscape/Graphs/MS_Truck_Pass")})
	{
		TArray<FName> Referencers;
		Registry.GetReferencers(FName(Retired), Referencers);
		TestEqual(FString::Printf(TEXT("Retired audio has no project referencers: %s"), Retired), Referencers.Num(), 0);
	}
	int32 Loops = 0;
	for (const auto& Entry : CityAudioAssets)
	{
		auto* Wave = LoadObject<USoundWave>(nullptr, Entry.WavePackage);
		const auto Path = FString::Printf(TEXT("%s/%s.%s"), CityAudioGraphFolder, Entry.GraphName, Entry.GraphName);
		if (FString(Entry.GraphName).StartsWith(TEXT("SC_")))
		{
			auto* Cue = LoadObject<USoundCue>(nullptr, *Path);
			if (!TestNotNull(TEXT("Native traffic cue is cook-referenced"), Cue) || !TestNotNull(Entry.Id, Wave))
				continue;
			auto* Doppler = Cast<USoundNodeDoppler>(Cue->FirstNode);
			if (!TestNotNull(TEXT("Unreal handles actual motion pitch"), Doppler))
				continue;
			TestTrue(TEXT("Native Doppler is smoothed"), Doppler->bUseSmoothing);
			TestEqual(TEXT("Traffic cue contains exactly a Doppler and looping Wave Player"), Cue->AllNodes.Num(), 2);
			auto* Player =
			    Doppler->ChildNodes.Num() == 1 ? Cast<USoundNodeWavePlayer>(Doppler->ChildNodes[0]) : nullptr;
			if (TestNotNull(TEXT("Traffic Wave Player"), Player))
			{
				TestTrue(TEXT("Steady traffic loops explicitly"), Player->bLooping);
				TestEqual(TEXT("Exact steady source survives serialization"), Player->GetSoundWave(), Wave);
			}
			TestTrue(TEXT("Approved source duration survives import"), FMath::IsNearlyEqual(Wave->Duration, Entry.Seconds, .001f));
			TestEqual(TEXT("Native source volume stays unchanged"), Wave->Volume, 1.f);
			++Loops;
			continue;
		}
		auto* Graph = LoadObject<UMetaSoundSource>(nullptr, *Path);
		if (!TestNotNull(Entry.Id, Wave) || !TestNotNull(TEXT("Graph has a cookable native source"), Graph))
			continue;
		TestEqual(TEXT("Mono source retained"), Wave->NumChannels, 1);
		TestTrue(TEXT("Duration retained"), FMath::IsNearlyEqual(Wave->Duration, Entry.Seconds, .001f));
		TestEqual(TEXT("Source gain is not normalized twice"), Wave->Volume, 1.f);
		TestEqual(TEXT("Source pitch is unchanged"), Wave->Pitch, 1.f);
		const auto& Document = Graph->GetConstDocument();
		const auto* Class = Document.Dependencies.FindByPredicate(
		    [](const auto& Item) { return Item.Metadata.GetClassName().Name == TEXT("Wave Player"); });
		if (!TestNotNull(TEXT("Graph contains the engine Wave Player"), Class))
			continue;
		const auto* Node = Document.RootGraph.GetConstDefaultGraph().Nodes.FindByPredicate(
		    [&](const auto& Item) { return Item.ClassID == Class->ID; });
		if (!TestNotNull(TEXT("Wave Player is instantiated"), Node))
			continue;
		auto Literal = [&](FName Pin) -> const FMetasoundFrontendLiteral*
		{
			const auto* Input =
			    Node->Interface.Inputs.FindByPredicate([&](const auto& Item) { return Item.Name == Pin; });
			if (!Input)
				return nullptr;
			const auto* Value =
			    Node->InputLiterals.FindByPredicate([&](const auto& Item) { return Item.VertexID == Input->VertexID; });
			return Value ? &Value->Value : nullptr;
		};
		bool Loop = !Entry.bLoop;
		const auto* LoopValue = Literal(TEXT("Loop"));
		TestTrue(TEXT("MetaSound explicitly declares loop eligibility"),
		         LoopValue && LoopValue->TryGet(Loop) && Loop == Entry.bLoop);
		float Duration = 0;
		const auto* DurationValue = Literal(TEXT("Loop Duration"));
		TestTrue(TEXT("Whole-file loop duration is explicit"),
		         DurationValue && DurationValue->TryGet(Duration) && Duration == -1.f);
		UObject* Reference = nullptr;
		const auto* WaveValue = Literal(TEXT("Wave Asset"));
		TestTrue(FString::Printf(TEXT("%s graph directly references the exact imported wave for cook inclusion"), Entry.Id),
		         WaveValue && WaveValue->TryGet(Reference) && Reference == Wave);
		Loops += Entry.bLoop;
	}
	TestEqual(TEXT("Bank has21 preserved plus five recorded traffic loops"), Loops, 26);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySoundscapeRoutingTest, "CitySim.Audio.BoundedPoolAndCookReferences",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySoundscapeRoutingTest::RunTest(const FString& Parameters)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Audio = Board->FindComponentByClass<UCitySoundscapeComponent>();
	if (TestNotNull(TEXT("Board owns one soundscape component"), Audio))
	{
		TestTrue(TEXT("All53 graphs are hard referenced by the runtime component"), Audio->IsReady());
		TestFalse(TEXT("No independent component tick"), Audio->PrimaryComponentTick.bCanEverTick);
		Audio->EnsurePool();
		const int32 Count = Audio->Players.Num();
		TestEqual(TEXT("Pool is bounded at32 components, including two engines and transitions"), Count, 32);
		Audio->EnsurePool();
		TestEqual(TEXT("Repeated initialization never duplicates voices"), Audio->Players.Num(), Count);
		FCityAudioVoice Siren;
		Siren.GridPosition = FVector(16, 20, 0);
		for (const auto Cue : {ECityAudioCue::PoliceIntro, ECityAudioCue::PoliceLoop, ECityAudioCue::PoliceOutro,
		                       ECityAudioCue::FireLoop, ECityAudioCue::AmbulanceLoop, ECityAudioCue::PoliceAlternate,
		                       ECityAudioCue::FireHorn})
		{
			Siren.Cue = Cue;
			float Near = 0;
			for (double Scale : {.003, .006, .012})
			{
				Audio->BoardTransform = FTransform(FRotator(0, 37, 0), FVector(110, -90, 40), FVector(Scale));
				Audio->Listener = Audio->BoardTransform.TransformPosition(FVector(12800, 16000, 3300));
				const float Close = Audio->VoiceDistanceGain(Siren);
				if (Near > 0)
					TestTrue(TEXT("Siren mix follows city zoom independently of board scale/rotation"),
					         FMath::IsNearlyEqual(Close, Near, .0001f));
				Near = Close;
				Audio->Listener = Audio->BoardTransform.TransformPosition(FVector(12800, 16000, 38500));
				TestTrue(TEXT("Overview siren phases and horns are at least30dB below nearby response"),
				         Audio->VoiceDistanceGain(Siren) < Close * .032f);
				Audio->Listener = Audio->BoardTransform.TransformPosition(FVector(44800, 16000, 3300));
				TestTrue(TEXT("Panning away also reduces response audio"),
				         Audio->VoiceDistanceGain(Siren) < Close * .032f);
			}
		}
		Siren.Cue = ECityAudioCue::TownLoop;
		TestEqual(TEXT("City-wide warning gain is separate from vehicle attenuation"), Audio->VoiceDistanceGain(Siren),
		          1.f);
		TestEqual(TEXT("Invalid listener distance is silent"),
		          CitySoundscape::VehicleSirenGain(std::numeric_limits<double>::quiet_NaN()), 0.f);
		TestTrue(TEXT("Closest normal desktop zoom retains the recognizable nearby siren mix"),
		         CitySoundscape::VehicleSirenGain(64 * .15) > .9f);
		for (const auto& Player : Audio->Players)
		{
			TestFalse(TEXT("Voices cannot auto activate on load"), Player->bAutoActivate);
			TestFalse(TEXT("Pool components cannot auto destroy under the allocator"), Player->bAutoDestroy);
			TestFalse(TEXT("Distance uses city focus and scale, avoiding world-unit attenuation drift"),
			          Player->AttenuationOverrides.bAttenuate);
		}
		TestTrue(TEXT("Lifecycle cancellation fits the100ms contract"), CitySoundscape::InterruptSeconds <= .1f);
		// Exercise the real pooled routing against an isolated city and native audio device.
		World->bAllowAudioPlayback = true;
		FAudioDeviceManager::Get()->SetAudioDevice(*World, GEngine->GetMainAudioDevice().GetDeviceID());
		auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
		Simulation->PlacementModel->SetTerrain(nullptr);
		auto Road = Simulation->PlacementModel->BeginRoadPreview(FCityPlacementModel::EntranceCell);
		Simulation->PlacementModel->UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
		TestTrue(TEXT("Isolated audio road fixture commits"), Simulation->PlacementModel->CommitRoad(Road).IsSuccess());
		FCityRoadNetworkBuilder::Build(*Simulation->PlacementModel, FCityRoadDefinition::Local(),
		                               Simulation->RoadNetworkSnapshot);
		Board->SimulationSubsystem = Simulation;
		Board->PlacementModel = Simulation->PlacementModel.Get();
		Board->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(110, -90, 40), FVector(.006)));
		Audio->Listener = Board->GetActorTransform().TransformPosition(FVector(5200, 6800, 7700));
		Simulation->SetEntryMenuSuspended(false);
		Simulation->SetSimulationPaused(false);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FCityTrafficVehicleState Car;
			Car.Id = 101 + Index;
			TestTrue(TEXT("Audio fixture route builds"),
			         FCityTrafficSimulation::BuildRoute(Simulation->RoadNetworkSnapshot,
			                                            FCityCellCoord(2 + Index * 5, 8), FCityCellCoord(20, 8), Car));
			TestTrue(TEXT("Audio fixture vehicle adds"),
			         Simulation->Traffic.AddVehicle(Car, Simulation->RoadNetworkSnapshot));
		}
		Audio->Update(*Board, *Simulation, .1f, false);
		TestTrue(TEXT("First nearest queued car has a native idle emitter"),
		         Audio->FindVoice({ECityAudioChannel::Traffic, 101}) != INDEX_NONE);
		TestTrue(TEXT("Second nearest queued car has a native idle emitter"),
		         Audio->FindVoice({ECityAudioChannel::Traffic, 102}) != INDEX_NONE);
		TestEqual(TEXT("Third nearby vehicle cannot exceed the engine cap"),
		          Audio->FindVoice({ECityAudioChannel::Traffic, 103}), INDEX_NONE);
		Simulation->PreviousTraffic = Simulation->GetTrafficSnapshot();
		Simulation->Traffic.AdvanceOneStep(Simulation->RoadNetworkSnapshot, 1);
		Audio->Update(*Board, *Simulation, .1f, false);
		// The closest vehicle changes first; the shared spare serializes simultaneous crossfades.
		const int32 Moving = Audio->FindVoice({ECityAudioChannel::Traffic, 102});
		if (TestTrue(TEXT("Moving vehicle retains the same emitter identity"), Moving != INDEX_NONE))
		{
			TestEqual(TEXT("Actual authoritative motion selects the driving loop"), Audio->Voices[Moving].Cue,
			          ECityAudioCue::CarDriving);
			const FVector Position = Audio->Voices[Moving].GridPosition;
			const FVector Expected = Board->GetActorTransform().TransformPosition(
			    FVector(Position.X * 800, Position.Y * 800, Position.Z + 100));
			// Placement is checked after normal routing's Desktop listener-relative transform.
			const FVector FocusWorld = Board->GetActorTransform().TransformPosition(Audio->Focus * 800);
			TestTrue(TEXT("Native engine component follows its exact interpolated vehicle position"),
			         Audio->Players[Moving]->GetComponentLocation().Equals(
			             Audio->Listener + (Expected - FocusWorld) * .05, .001));
		}
		// Exercise engine and siren coexistence through the real allocator, not only policy helpers.
		auto ResetAudioFixture = [&]()
		{
			Audio->StopAll();
			// This synchronous test does not advance the mixer through a retiring fade.
			for (const auto& Player : Audio->Players)
				Player->Stop();
			Simulation->Traffic.Reset();
		};
		for (const auto Type : {ECityTripVehicleType::PoliceCar, ECityTripVehicleType::FireEngine,
		                        ECityTripVehicleType::Ambulance})
		{
			ResetAudioFixture();
			FCityTrafficVehicleState Responder;
			Responder.Id = 200 + static_cast<uint64>(Type);
			Responder.VehicleType = Type;
			Responder.bEmergencyResponding = true;
			TestTrue(TEXT("Responder audio route builds"),
			         FCityTrafficSimulation::BuildRoute(Simulation->RoadNetworkSnapshot,
			                                            FCityCellCoord(2, 8), FCityCellCoord(20, 8), Responder));
			TestTrue(TEXT("Responder audio fixture adds"),
			         Simulation->Traffic.AddVehicle(Responder, Simulation->RoadNetworkSnapshot));
			Simulation->PreviousTraffic = Simulation->GetTrafficSnapshot();
			Audio->Update(*Board, *Simulation, .1f, false);
			TestTrue(TEXT("Responding vehicle has an independent base engine"),
			         Audio->FindVoice({ECityAudioChannel::Traffic, Responder.Id}) != INDEX_NONE);
			TestTrue(TEXT("Responding vehicle keeps its siren alongside its engine"),
			         Audio->FindVoice({ECityAudioChannel::Vehicle, Responder.Id}) != INDEX_NONE);
			ResetAudioFixture();
			Responder.bReturning = true;
			TestTrue(TEXT("Returning responder fixture adds"),
			         Simulation->Traffic.AddVehicle(Responder, Simulation->RoadNetworkSnapshot));
			Simulation->PreviousTraffic = Simulation->GetTrafficSnapshot();
			Audio->Update(*Board, *Simulation, .1f, false);
			TestTrue(TEXT("Returning crew retains its engine"),
			         Audio->FindVoice({ECityAudioChannel::Traffic, Responder.Id}) != INDEX_NONE);
			TestEqual(TEXT("Returning crew has no responding siren"),
			          Audio->FindVoice({ECityAudioChannel::Vehicle, Responder.Id}), INDEX_NONE);
		}
		// A new fire counter cannot generate IncidentNotice; all other counters are unchanged.
		Audio->NextNotification = 0;
		auto Fires = Simulation->GetFireSnapshot();
		++Fires.TotalIncidents;
		FString Error;
		TestTrue(TEXT("Isolated fire history is valid"), Simulation->Fire.RestoreState(Fires, 1, Error));
		Audio->Update(*Board, *Simulation, .1f, false);
		TestFalse(TEXT("Fire incident onset never plays a notification chime"),
		          Audio->Voices.ContainsByPredicate([&](const auto& Voice)
		                                            { return Voice.Cue == ECityAudioCue::IncidentNotice; }));
		// Legacy simple/corner Park has radius0 and uses economy tuning; audio must use that policy too.
		Simulation->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!TestNotNull(TEXT("Isolated ambience fixture loads the native catalog"), Simulation->BuildingCatalog.Get()))
		{
			World->DestroyWorld(false);
			return false;
		}
		int32 ParkIndex = 0;
		for (const FName Id : {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark")), FName(TEXT("CityPark")),
		                       FName(TEXT("BasketballPark")), FName(TEXT("SkatePark")), FName(TEXT("BaseballPark")),
		                       FName(TEXT("SoccerPark"))})
		{
			const auto* Authored = Simulation->GetBuildingCatalog()->FindDefinition(Id);
			if (!TestNotNull(TEXT("Every native park definition exists"), Authored))
				continue;
			auto Definition = *Authored;
			Definition.Cost = 0;
			Definition.Footprint = FCityCellFootprint(1, 1);
			auto Preview = Simulation->PlacementModel->BeginPreview(Definition, FCityCellCoord(30 + ParkIndex * 2, 30));
			TestTrue(TEXT("Isolated park audio fixture commits"),
			         Simulation->PlacementModel->CommitPlacement(Preview, Definition).IsSuccess());
			++ParkIndex;
		}
		Audio->bWasBlocked = true;
		Audio->RebuildAreas(*Simulation);
		TestEqual(TEXT("All seven definitions, including simple/corner Park, enter the shared ambience areas"),
		          Audio->Areas.FilterByPredicate([](const auto& Area) { return Area.bPark; }).Num(), 7);
		TestTrue(TEXT("Legacy simple/corner park is covered despite zero authored radius"),
		         Audio->Areas.ContainsByPredicate([](const auto& Area)
		                                          { return Area.bPark && Area.Definition == TEXT("Park"); }));
		Audio->bWasBlocked = false;
		Simulation->SetSimulationPaused(true);
		Audio->Update(*Board, *Simulation, .1f, false);
		TestTrue(TEXT("Pause retires every engine, including crossfade components"),
		         Audio->Voices
		             .FilterByPredicate([](const auto& Voice)
		                                { return Voice.Key.Channel == ECityAudioChannel::Traffic && !Voice.bRetiring; })
		             .IsEmpty());
		Simulation->SetSimulationPaused(false);
		Audio->StopAll();
		for (const auto& Player : Audio->Players)
			Player->Stop();
		const float PriorFX = GetDefault<UCityAudioSettings>()->GetAlarmVolume();
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(.75f);
		FCityAudioVoice Warning;
		Warning.Key = {ECityAudioChannel::Town, 82001};
		Warning.Cue = ECityAudioCue::TownLoop;
		Warning.Gain = 1;
		Warning.bSpatial = false;
		Warning.EndSeconds = Audio->WallSeconds + 30;
		TestTrue(TEXT("Native warning fixture starts"), Audio->StartVoice(Warning, 0));
		TestEqual(TEXT("Playing city-wide warning ducks music"), Audio->GetMusicDuckGain(), .35f);
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(0);
		TestEqual(TEXT("Muted FX never ducks audible music"), Audio->GetMusicDuckGain(), 1.f);
		GetMutableDefault<UCityAudioSettings>()->SetAlarmVolume(PriorFX);
		for (const auto& Player : Audio->Players)
			Player->Stop();
		TestEqual(TEXT("Warning cancellation releases music duck"), Audio->GetMusicDuckGain(), 1.f);
	}
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficAudioPolicyTest, "CitySim.Audio.TrafficStatesAndEarthquakeWindow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTrafficAudioPolicyTest::RunTest(const FString& Parameters)
{
	FCityTrafficVehicleState Vehicle;
	for (const auto Type :
	     {ECityTripVehicleType::Car, ECityTripVehicleType::FreightTruck, ECityTripVehicleType::GarbageTruck,
	      ECityTripVehicleType::PoliceCar, ECityTripVehicleType::FireEngine, ECityTripVehicleType::Ambulance})
	{
		Vehicle.VehicleType = Type;
		Vehicle.bCompleted = false;
		Vehicle.SpeedUnitsPerStep = 0;
		TestTrue(TEXT("Every active motor vehicle can emit an engine"), CitySoundscape::HasEngineAudio(Vehicle));
		const auto Idle = CitySoundscape::TrafficLoop(Vehicle);
		const bool CarFamily = Type == ECityTripVehicleType::Car || Type == ECityTripVehicleType::PoliceCar;
		TestEqual(TEXT("Every vehicle maps to its approved engine family"), Idle,
		          CarFamily ? ECityAudioCue::CarIdle : ECityAudioCue::TruckIdle);
		TestEqual(TEXT("Accepted car level is retained; truck idle is75% of the previous0.6 gain"),
		          CitySoundscape::EngineGain(Idle), CarFamily ? .6f : .45f);
		Vehicle.SpeedUnitsPerStep = 1;
		const auto Driving = CitySoundscape::TrafficLoop(Vehicle);
		TestEqual(TEXT("Driving uses the same accepted family gain"),
		          CitySoundscape::EngineGain(Driving), CarFamily ? .6f : .45f);
		TestTrue(TEXT("Moving and queued traffic have distinct stems"), Idle != Driving);
		Vehicle.SpeedUnitsPerStep = 7000;
		TestEqual(TEXT("Faster traffic keeps a steady driving stem without a baked pass envelope"),
		          CitySoundscape::TrafficLoop(Vehicle), Driving);
		Vehicle.SpeedUnitsPerStep = 0;
		TestEqual(TEXT("Stopping returns to its matching idle"), CitySoundscape::TrafficLoop(Vehicle), Idle);
		Vehicle.bCompleted = true;
		TestFalse(TEXT("Completed traffic loses its emitter"), CitySoundscape::HasEngineAudio(Vehicle));
	}
	Vehicle.bCompleted = false;
	Vehicle.VehicleType = ECityTripVehicleType::Walk;
	TestFalse(TEXT("Pedestrians never emit engines"), CitySoundscape::HasEngineAudio(Vehicle));
	Vehicle.VehicleType = ECityTripVehicleType::Car;
	Vehicle.SpeedUnitsPerStep = 10;
	Vehicle.Id = 100;
	TestEqual(TEXT("Even car uses approved A"), CitySoundscape::TrafficLoop(Vehicle), ECityAudioCue::CarDriving);
	Vehicle.Id = 101;
	TestEqual(TEXT("Odd car uses approved B"), CitySoundscape::TrafficLoop(Vehicle), ECityAudioCue::CarDrivingAlternate);
	TestTrue(TEXT("Alternate retains traffic spatial treatment"), CitySoundscape::IsTrafficLoop(ECityAudioCue::CarDrivingAlternate));
	FCityDisasterEvent Event;
	Event.WarningStep = 100;
	Event.StartStep = 101;
	for (int64 Length : {int64(11), int64(30), int64(110)})
	{
		Event.EndStep = Event.WarningStep + Length;
		Event.Kind = ECityNaturalDisaster::Earthquake;
		const int64 Cutoff = CitySoundscape::TownAlarmEndStep(Event);
		TestTrue(TEXT("Earthquake audio window halves to within one simulation step"),
		         FMath::Abs((Cutoff - Event.WarningStep) * 2 - Length) <= 1);
		TestTrue(TEXT("Later earthquake still exists after its alarm stops"), Cutoff < Event.EndStep);
		TestEqual(TEXT("Alarm policy leaves authoritative duration intact"), Event.EndStep, Event.WarningStep + Length);
		Event.Kind = ECityNaturalDisaster::Tornado;
		TestEqual(TEXT("Tornado warning lifetime is preserved"), CitySoundscape::TownAlarmEndStep(Event),
		          Event.EndStep);
	}
	return true;
}
#endif
