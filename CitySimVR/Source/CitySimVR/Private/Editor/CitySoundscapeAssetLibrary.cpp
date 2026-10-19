#include "Editor/CitySoundscapeAssetLibrary.h"
#if WITH_EDITOR
#include "Presentation/CitySoundscapeAssets.h"
#include "MetasoundSource.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "Sound/SoundWave.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

int32 UCitySoundscapeAssetLibrary::RepairWaveReferences()
{
#if WITH_EDITOR
	int32 ChangedGraphs = 0;
	for (const auto& Entry : CityAudioAssets)
	{
		if (FString(Entry.GraphName).StartsWith(TEXT("SC_"))) continue;
		auto* Wave = LoadObject<USoundWave>(nullptr, Entry.WavePackage);
		const FString GraphPath = FString(CityAudioGraphFolder) / Entry.GraphName;
		auto* Graph = LoadObject<UMetaSoundSource>(nullptr, *GraphPath);
		if (!Wave || !Graph) return -1;
		const auto& Document = Graph->GetConstDocument();
		const auto* PlayerClass = Document.Dependencies.FindByPredicate(
			[](const auto& Item) { return Item.Metadata.GetClassName().Name == TEXT("Wave Player"); });
		if (!PlayerClass) return -1;
		const auto& BeforeGraph = Document.RootGraph.GetConstDefaultGraph();
		const auto* PlayerNode = BeforeGraph.Nodes.FindByPredicate([&](const auto& Item) { return Item.ClassID == PlayerClass->ID; });
		if (!PlayerNode) return -1;
		const auto* WaveInput = PlayerNode->Interface.Inputs.FindByPredicate([](const auto& Item) { return Item.Name == TEXT("Wave Asset"); });
		if (!WaveInput) return -1;
		const auto* WaveLiteral = PlayerNode->InputLiterals.FindByPredicate([&](const auto& Item) { return Item.VertexID == WaveInput->VertexID; });
		if (!WaveLiteral) return -1;
		UObject* ExistingWave = nullptr;
		if (WaveLiteral->Value.TryGet(ExistingWave) && ExistingWave == Wave) continue;
		const FGuid PlayerID = PlayerNode->GetID();
		const FGuid InputID = WaveInput->VertexID;
		const auto BeforeLiterals = PlayerNode->InputLiterals;
		TArray<FGuid> BeforeNodeIDs;
		for (const auto& Node : BeforeGraph.Nodes) BeforeNodeIDs.Add(Node.GetID());
		const auto BeforeEdges = BeforeGraph.Edges;
		Graph->Modify();
		TScriptInterface<IMetaSoundDocumentInterface> SoundDocument;
		SoundDocument.SetObject(Graph);
		SoundDocument.SetInterface(static_cast<IMetaSoundDocumentInterface*>(Graph));
		auto& Builder = Metasound::Frontend::IDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(SoundDocument);
		FMetasoundFrontendLiteral NewWaveLiteral;
		NewWaveLiteral.Set(static_cast<UObject*>(Wave));
		if (!Builder.SetNodeInputDefault(PlayerID, InputID, NewWaveLiteral)) return -1;
		const auto& AfterGraph = Graph->GetConstDocument().RootGraph.GetConstDefaultGraph();
		TArray<FGuid> AfterNodeIDs;
		for (const auto& Node : AfterGraph.Nodes) AfterNodeIDs.Add(Node.GetID());
		if (BeforeNodeIDs != AfterNodeIDs || BeforeEdges != AfterGraph.Edges) return -1;
		const auto* AfterPlayer = AfterGraph.Nodes.FindByPredicate([&](const auto& Node) { return Node.GetID() == PlayerID; });
		if (!AfterPlayer || BeforeLiterals.Num() != AfterPlayer->InputLiterals.Num()) return -1;
		for (const auto& BeforeLiteral : BeforeLiterals)
		{
			const auto* AfterLiteral = AfterPlayer->InputLiterals.FindByPredicate([&](const auto& Item) { return Item.VertexID == BeforeLiteral.VertexID; });
			if (!AfterLiteral) return -1;
			if (BeforeLiteral.VertexID == InputID)
			{
				UObject* UpdatedWave = nullptr;
				if (!AfterLiteral->Value.TryGet(UpdatedWave) || UpdatedWave != Wave) return -1;
			}
			if (BeforeLiteral.VertexID != InputID && !(BeforeLiteral.Value == AfterLiteral->Value)) return -1;
		}
		Graph->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(GraphPath, FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Graph->GetOutermost(), Graph, *Filename, SaveArgs)) return -1;
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_WAVE_REFERENCE_REPAIRED %s"), Entry.Id);
		++ChangedGraphs;
	}
	return ChangedGraphs;
#else
	return -1;
#endif
}
