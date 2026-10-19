#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityBoardActor.h"
#include "Presentation/CitySoundscapeAssets.h"
#include "Presentation/CitySoundscapeComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRuntimeAssetOrganizationTest, "CitySim.Assets.RuntimeOrganization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityRuntimeAssetOrganizationTest::RunTest(const FString& Parameters)
{
    const auto* Board = GetDefault<ACityBoardActor>();
    if (!TestNotNull(TEXT("Actual board defaults retain the cooked soundscape"), Board->Soundscape.Get())) return false;
    TestTrue(TEXT("All native soundscape sources are ready"), Board->Soundscape->IsReady());
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CityAudioAssets); ++Index)
    {
        const auto& Entry = CityAudioAssets[Index];
        auto* Wave = LoadObject<USoundWave>(nullptr, Entry.WavePackage);
        auto* Source = Board->Soundscape->GetCueSound(static_cast<ECityAudioCue>(Index));
        if (!TestNotNull(Entry.Id, Wave) || !TestNotNull(TEXT("Real component retains the graph/cue"), Source)) continue;
        TestEqual(TEXT("Sound source uses the semantic package"), Source->GetPathName(),
            FString::Printf(TEXT("%s/%s.%s"), CityAudioGraphFolder, Entry.GraphName, Entry.GraphName));
        TestTrue(TEXT("Cook preserves source duration"), FMath::IsNearlyEqual(Wave->Duration, Entry.Seconds, .001f));
        TestEqual(TEXT("Cook preserves mono soundscape sources"), Wave->NumChannels, 1);
        TestEqual(TEXT("Source volume remains unchanged"), Wave->Volume, 1.f);
        TestEqual(TEXT("Source pitch remains unchanged"), Wave->Pitch, 1.f);
    }
    for (const auto& Clip : Board->ParkCitizenAnimations)
    {
        if (!TestNotNull(TEXT("Actual board retains every park animation"), Clip.Get())) continue;
        TestTrue(TEXT("Park animation has real duration"), Clip->GetPlayLength() > 0.f);
        TestNotNull(TEXT("Park animation retains its skeleton"), Clip->GetSkeleton());
        TestFalse(TEXT("Park animation names have no development task suffix"), Clip->GetName().Contains(TEXT("_ParkAnimation")));
    }
    auto* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySim/Art/Citizens/AS_Citizen_Walk.AS_Citizen_Walk"));
    if (TestNotNull(TEXT("Cook retains the approved citizen walk"), Walk) &&
        TestNotNull(TEXT("Actual board uses the idle/walk blend"), Board->StreetWalkBlend.Get()))
    {
        TestEqual(TEXT("Blend uses semantic name"), Board->StreetWalkBlend->GetName(), FString(TEXT("BS_Citizen_StreetWalk")));
        TestTrue(TEXT("Walk preserves the reviewed duration"), FMath::IsNearlyEqual(Walk->GetPlayLength(), 28.f / 24.f, .002f));
        TestEqual(TEXT("Walk and blend share the original skeleton"), Board->StreetWalkBlend->GetSkeleton(), Walk->GetSkeleton());
        TestFalse(TEXT("Walk has no authoritative root displacement"), Walk->bEnableRootMotion);
        auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySim/Art/Parks/Animations/AS_Park_Idle.AS_Park_Idle"));
        if (TestNotNull(TEXT("Cook retains the actual idle clip"), Idle) &&
            TestEqual(TEXT("Blend retains both samples"), Board->StreetWalkBlend->GetBlendSamples().Num(), 2))
        {
            TestEqual(TEXT("Idle sample resolves the renamed clip"), Board->StreetWalkBlend->GetBlendSample(0).Animation.Get(), Idle);
            TestEqual(TEXT("Walk sample resolves the renamed clip"), Board->StreetWalkBlend->GetBlendSample(1).Animation.Get(), Walk);
        }
    }
    return true;
}
#endif
