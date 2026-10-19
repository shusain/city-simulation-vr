#include "Editor/CityDesktopUiAssetsCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopChart.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/CityDesktopIcon.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Presentation/CityDesktopTerrainPreview.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
bool AddConditions(UWidgetTree* Tree)
{
	auto* Canvas = Cast<UCanvasPanel>(Tree->RootWidget);
	if (!Canvas)
		return false;
	if (!Tree->FindWidget(TEXT("ConditionProvider")))
		if (auto* Summary = Tree->FindWidget(TEXT("InspectionSummary")))
			if (auto* Parent = Cast<UVerticalBox>(Summary->GetParent()))
			{
				auto* Provider = Tree->ConstructWidget<UCityDesktopActionButton>(
				    UCityDesktopActionButton::StaticClass(), TEXT("ConditionProvider"));
				Parent->InsertChildAt(Parent->GetChildIndex(Summary) + 2, Provider);
				auto* ProviderLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				                                                        TEXT("ConditionProviderLabel"));
				auto ProviderFont = ProviderLabel->GetFont();
				ProviderFont.Size = 12;
				ProviderLabel->SetFont(ProviderFont);
				ProviderLabel->SetAutoWrapText(true);
				Provider->AddChild(ProviderLabel);
				Provider->SetVisibility(ESlateVisibility::Collapsed);
			}
	if (!Tree->FindWidget(TEXT("ConditionDetailIcons")))
		if (auto* Summary = Tree->FindWidget(TEXT("InspectionSummary")))
			if (auto* Parent = Cast<UVerticalBox>(Summary->GetParent()))
			{
				auto* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
				                                                  TEXT("ConditionDetailIcons"));
				Parent->InsertChildAt(Parent->GetChildIndex(Summary) + 1, Row);

				for (int32 I = 0; I < 10; ++I)
				{
					auto* Icon = Tree->ConstructWidget<UCityDesktopIcon>(
					    UCityDesktopIcon::StaticClass(),
					    FName(FString::Printf(TEXT("ConditionDetail%d"), I)));
					Row->AddChild(Icon);
					Cast<UHorizontalBoxSlot>(Icon->Slot)->SetPadding(FMargin(0, 4, 8, 4));
				}
			}
	if (Tree->FindWidget(TEXT("ConditionCanvas")))
		return true;
	auto* Layer = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ConditionCanvas"));
	Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* LayerSlot = Canvas->AddChildToCanvas(Layer);
	LayerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	LayerSlot->SetOffsets(FMargin(0));
	LayerSlot->SetZOrder(-1);
	for (int32 I = -1; I < 16; ++I)
	{
		const FString Name = I < 0 ? TEXT("ConditionNext") : FString::Printf(TEXT("Condition%d"), I);
		auto* Button = Tree->ConstructWidget<UCityDesktopActionButton>(
		    UCityDesktopActionButton::StaticClass(), FName(Name));
		auto* Slot = Layer->AddChildToCanvas(Button);
		Slot->SetSize(I < 0 ? FVector2D(330, 38) : FVector2D(72, 42));
		if (I < 0)
		{
			Slot->SetAnchors(FAnchors(0, 1));
			Slot->SetPosition(FVector2D(20, -220));
		}
		auto* Content = Tree->ConstructWidget<UHorizontalBox>();
		Button->AddChild(Content);
		if (I >= 0)
		{
			auto* Icon = Tree->ConstructWidget<UCityDesktopIcon>(UCityDesktopIcon::StaticClass(),
			                                                     FName(Name + TEXT("Icon")));
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			Content->AddChild(Icon);
		}
		auto* Label =
		    Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name + TEXT("Label")));
		auto Font = Label->GetFont();
		Font.Size = I < 0 ? 12 : 11;
		Label->SetFont(Font);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		Content->AddChild(Label);
		Cast<UHorizontalBoxSlot>(Label->Slot)->SetVerticalAlignment(VAlign_Center);
		Button->SetVisibility(ESlateVisibility::Collapsed);
	}
	return true;
}
UFont* CreateNewsTypeface()
{
	const FString FontPath = TEXT("/Game/CitySim/UI/Fonts/F_CityNewsSerif");
	if (auto* Existing =
	        LoadObject<UFont>(nullptr, *(FontPath + TEXT(".F_CityNewsSerif")), nullptr, LOAD_NoWarn))
		return Existing;
	UFontFace* Faces[2] = {};
	const TCHAR* Styles[] = {TEXT("Regular"), TEXT("Bold")};
	for (int32 I = 0; I < 2; ++I)
	{
		TArray<uint8> Bytes;
		const FString Source = FPaths::ProjectDir() / TEXT("../ArtSource/Fonts/DejaVu") /
		                       (I ? TEXT("DejaVuSerif-Bold.ttf") : TEXT("DejaVuSerif.ttf"));
		if (!FFileHelper::LoadFileToArray(Bytes, *Source))
			return nullptr;
		const FString Name = FString::Printf(TEXT("FF_CityNews%s"), Styles[I]);
		const FString Path = TEXT("/Game/CitySim/UI/Fonts/") + Name;
		auto* Package = CreatePackage(*Path);
		auto* Face = NewObject<UFontFace>(Package, FName(Name), RF_Public | RF_Standalone);
		Face->SourceFilename = Source;
		Face->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
		Face->LoadingPolicy = EFontLoadingPolicy::Inline;
		FAssetRegistryModule::AssetCreated(Face);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(
		        Package, Face,
		        *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()),
		        Args))
			return nullptr;
		Faces[I] = Face;
	}
	auto* Package = CreatePackage(*FontPath);
	auto* Font = NewObject<UFont>(Package, TEXT("F_CityNewsSerif"), RF_Public | RF_Standalone);
	Font->FontCacheType = EFontCacheType::Runtime;
	for (int32 I = 0; I < 2; ++I)
	{
		auto& Entry = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Emplace_GetRef();
		Entry.Name = Styles[I];
		Entry.Font = FFontData(Faces[I]);
	}
	FAssetRegistryModule::AssetCreated(Font);
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(
	           Package, Font,
	           *FPackageName::LongPackageNameToFilename(FontPath, FPackageName::GetAssetPackageExtension()),
	           Args)
	           ? Font
	           : nullptr;
}
bool AddNewspaper(UWidgetTree* Tree)
{
	auto* Body = Tree->FindWidget(TEXT("MenuBody"));
	auto* Content = Body ? Cast<UVerticalBox>(Body->GetParent()) : nullptr;
	auto* Scroll = Content ? Cast<UScrollBox>(Content->GetParent()) : nullptr;
	auto* Menu = Scroll ? Cast<UVerticalBox>(Scroll->GetParent()) : nullptr;
	if (!Menu)
		return false;
	auto* Font = CreateNewsTypeface();
	if (!Font)
		return false;
	auto* Paper = Cast<UCityDesktopNewspaper>(Tree->FindWidget(TEXT("Newspaper")));
	if (!Paper)
	{
		Paper = Tree->ConstructWidget<UCityDesktopNewspaper>(UCityDesktopNewspaper::StaticClass(),
		                                                     TEXT("Newspaper"));
		Content->InsertChildAt(Content->GetChildIndex(Body), Paper);
	}
	Paper->Typeface = Font;
	Paper->SetVisibility(ESlateVisibility::Collapsed);
	if (!Tree->FindWidget(TEXT("NewsNavigation")))
	{
		auto* Navigation = Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("NewsNavigation"));
		Navigation->SetExplicitWrapSize(true);
		Navigation->SetInnerSlotPadding(FVector2D(6, 6));
		Menu->InsertChildAt(Menu->GetChildIndex(Scroll), Navigation);
		Cast<UVerticalBoxSlot>(Navigation->Slot)->SetPadding(FMargin(0, 6));
		auto* Original = Cast<UCityDesktopActionButton>(Tree->FindWidget(TEXT("Menu0")));
		auto* OriginalLabel = Cast<UTextBlock>(Tree->FindWidget(TEXT("Menu0Label")));
		if (!Original || !OriginalLabel)
			return false;
		for (int32 I = 0; I < 8; ++I)
		{
			const FString Name = FString::Printf(TEXT("NewsAction%d"), I);
			auto* Button = Tree->ConstructWidget<UCityDesktopActionButton>(
			    UCityDesktopActionButton::StaticClass(), FName(Name));
			Button->SetStyle(Original->GetStyle());
			Navigation->AddChild(Button);
			auto* Label =
			    Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name + TEXT("Label")));
			Label->SetFont(OriginalLabel->GetFont());
			Button->AddChild(Label);
		}
		Navigation->SetVisibility(ESlateVisibility::Collapsed);
	}
	return true;
}
bool AddInspectionRefinements(UWidgetTree* Tree)
{
	auto* Legacy = Tree->FindWidget(TEXT("Inspection"));
	auto* Content = Legacy ? Cast<UVerticalBox>(Legacy->GetParent()) : nullptr;
	auto* Original = Cast<UCityDesktopActionButton>(Tree->FindWidget(TEXT("InspectionDetails")));
	if (!Content || !Original)
		return false;
	if (auto* Preview = Cast<USizeBox>(Tree->FindWidget(TEXT("PreviewSize"))))
		Preview->SetHeightOverride(104);
	for (int32 I = 0; I < 6; ++I)
		if (auto* Header = Cast<UTextBlock>(
		        Tree->FindWidget(FName(FString::Printf(TEXT("InspectionSection%dLabel"), I)))))
			Header->SetAutoWrapText(false);
	if (Tree->FindWidget(TEXT("Overview0Row")))
	{
		for (int32 I = 0; I < 6; ++I)
		{
			const FString Name = FString::Printf(TEXT("InspectionSection%dTable"), I);
			if (!Tree->FindWidget(FName(Name)))
			{
				auto* Body = Cast<UVerticalBox>(
				    Tree->FindWidget(FName(FString::Printf(TEXT("InspectionSection%dBody"), I))));
				if (!Body)
					return false;
				Body->InsertChildAt(1, Tree->ConstructWidget<UCityDesktopChart>(
				                           UCityDesktopChart::StaticClass(), FName(Name)));
			}
		}
		return true;
	}
	auto Label = [Tree](const FString& Name, int32 Size) {
		auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		auto Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		Text->SetColorAndOpacity(FLinearColor(.88f, .94f, 1.f));
		return Text;
	};
	int32 Insert = Content->GetChildIndex(Tree->FindWidget(TEXT("InspectionChart")));
	for (int32 I = 0; I < 3; ++I)
	{
		const FString Name = FString::Printf(TEXT("Overview%d"), I);
		auto* Row =
		    Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(Name + TEXT("Row")));
		Content->InsertChildAt(Insert++, Row);
		Cast<UVerticalBoxSlot>(Row->Slot)->SetPadding(FMargin(0, 2));
		auto* Icon = Tree->ConstructWidget<UCityDesktopIcon>(UCityDesktopIcon::StaticClass(),
		                                                     FName(Name + TEXT("Icon")));
		Row->AddChild(Icon);
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* Text = Label(Name, 13);
		Row->AddChild(Text);
		Cast<UHorizontalBoxSlot>(Text->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Cast<UHorizontalBoxSlot>(Text->Slot)->SetVerticalAlignment(VAlign_Center);
		Cast<UHorizontalBoxSlot>(Text->Slot)->SetPadding(FMargin(8, 0, 0, 0));
	}
	for (int32 I = 0; I < 6; ++I)
	{
		const FString Name = FString::Printf(TEXT("InspectionSection%d"), I);
		auto* Header = Tree->ConstructWidget<UCityDesktopActionButton>(
		    UCityDesktopActionButton::StaticClass(), FName(Name));
		Header->SetStyle(Original->GetStyle());
		auto* HeaderLabel = Label(Name + TEXT("Label"), 14);
		HeaderLabel->SetAutoWrapText(false);
		Header->AddChild(HeaderLabel);
		Cast<UButtonSlot>(Header->GetContent()->Slot)->SetPadding(FMargin(8, 7));
		Content->AddChild(Header);
		Cast<UVerticalBoxSlot>(Header->Slot)->SetPadding(FMargin(0, 3));
		auto* Body =
		    Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(Name + TEXT("Body")));
		Content->AddChild(Body);
		Cast<UVerticalBoxSlot>(Body->Slot)->SetPadding(FMargin(8, 3, 8, 7));
		Body->AddChild(Tree->ConstructWidget<UCityDesktopChart>(UCityDesktopChart::StaticClass(),
		                                                        FName(Name + TEXT("Chart"))));
		Body->AddChild(Tree->ConstructWidget<UCityDesktopChart>(UCityDesktopChart::StaticClass(),
		                                                        FName(Name + TEXT("Table"))));
		Body->AddChild(Label(Name + TEXT("Detail"), 13));
	}
	return true;
}
    bool AddToolRefinements(UWidgetTree* Tree)
	{
		auto* Tools = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("ToolButtons")));
		auto* Text = Tree->FindWidget(TEXT("Construction"));
		if (!Tools || !Text)
			return false;
		if (auto* Scroll = Cast<UScrollBox>(Text->GetParent()))
		{
			auto* Parent = Cast<UVerticalBox>(Scroll->GetParent());
			if (!Parent)
				return false;
			Scroll->RemoveChild(Text);
			Parent->RemoveChild(Scroll);
			Parent->AddChild(Text);
			Scroll->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
		if (!Tree->FindWidget(TEXT("ToolbarSize7")))
		{
			auto* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ToolbarSize7"));
			Box->SetWidthOverride(98);
			Box->SetHeightOverride(62);
			auto* Button = Tree->ConstructWidget<UCityDesktopActionButton>(UCityDesktopActionButton::StaticClass(),
			                                                               TEXT("Toolbar7"));
			if (auto* Original = Cast<UCityDesktopActionButton>(Tree->FindWidget(TEXT("Toolbar0"))))
				Button->SetStyle(Original->GetStyle());
			auto* Content = Tree->ConstructWidget<UVerticalBox>();
			Button->AddChild(Content);
			Cast<UButtonSlot>(Content->Slot)->SetPadding(FMargin(0));
			auto* Icon = Tree->ConstructWidget<UCityDesktopIcon>(UCityDesktopIcon::StaticClass(), TEXT("Toolbar7Icon"));
			Content->AddChild(Icon);
			Cast<UVerticalBoxSlot>(Icon->Slot)->SetHorizontalAlignment(HAlign_Center);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Toolbar7Label"));
			Label->SetText(FText::FromString(TEXT("Repair")));
			Label->SetAutoWrapText(false);
			Label->SetJustification(ETextJustify::Center);
			auto Font = Label->GetFont();
			Font.Size = 16;
			Label->SetFont(Font);
			Label->SetColorAndOpacity(FLinearColor(.88f, .94f, 1.f));
			Content->AddChild(Label);
			Cast<UVerticalBoxSlot>(Label->Slot)->SetHorizontalAlignment(HAlign_Center);
			Box->AddChild(Button);
			Tools->AddChild(Box);
		}
		TArray<UWidget*> Ordered;
		for (const int32 Index : {0, 7, 1, 2, 5, 3, 4, 6})
		{
			auto* Box = Tree->FindWidget(FName(FString::Printf(TEXT("ToolbarSize%d"), Index)));
			if (!Box)
				return false;
			Ordered.Add(Box);
		}
		Tools->ClearChildren();
		for (int32 I = 0; I < Ordered.Num(); ++I)
			Cast<UHorizontalBoxSlot>(Tools->AddChild(Ordered[I]))->SetPadding(FMargin(I == 5 ? 13 : 3, 0, 3, 0));
		return AddInspectionRefinements(Tree);
	}
	bool AddMenuRefinements(UWidgetTree* Tree)
	{
		auto* Choices = Tree->FindWidget(TEXT("MenuButtons"));
		auto* Content = Choices ? Cast<UVerticalBox>(Choices->GetParent()) : nullptr;
		auto* Canvas = Cast<UCanvasPanel>(Tree->RootWidget);
		if (!Content || !Canvas)
			return false;
		if (!Tree->FindWidget(TEXT("TaxRows")))
		{
			auto* Rows = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TaxRows"));
			Content->InsertChildAt(Content->GetChildIndex(Choices), Rows);
			Cast<UVerticalBoxSlot>(Rows->Slot)->SetPadding(FMargin(0, 12));
			for (int32 Index = 0; Index < 3; ++Index)
			{
				auto* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
				                                                  FName(FString::Printf(TEXT("TaxRow%d"), Index)));
				Rows->AddChild(Row);
				Cast<UVerticalBoxSlot>(Row->Slot)->SetPadding(FMargin(0, 4));
				auto* Rate = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				                                               FName(FString::Printf(TEXT("TaxRate%d"), Index)));
				Rate->SetText(FText::FromString(TEXT("Residential 9%")));
				auto Font = Rate->GetFont();
				Font.Size = 16;
				Rate->SetFont(Font);
				Rate->SetColorAndOpacity(Index == 0   ? FLinearColor(.3f, .9f, .45f)
				                         : Index == 1 ? FLinearColor(.35f, .7f, 1.f)
				                                      : FLinearColor(1.f, .82f, .3f));
				Row->AddChild(Rate);
				Cast<UHorizontalBoxSlot>(Rate->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Cast<UHorizontalBoxSlot>(Rate->Slot)->SetVerticalAlignment(VAlign_Center);
				for (const TCHAR* Direction : {TEXT("Down"), TEXT("Up")})
				{
					const FString Name = FString::Printf(TEXT("Tax%s%d"), Direction, Index);
					auto* Button = Tree->ConstructWidget<UCityDesktopActionButton>(
					    UCityDesktopActionButton::StaticClass(), FName(Name));
					auto* Label =
					    Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name + TEXT("Label")));
					Label->SetText(FText::FromString(TEXT("- 1%")));
					Label->SetColorAndOpacity(FLinearColor(.9f, .94f, 1));
					Label->SetJustification(ETextJustify::Center);
					Button->AddChild(Label);
					auto* Size = Tree->ConstructWidget<USizeBox>();
					Size->SetWidthOverride(78);
					Size->SetHeightOverride(42);
					Size->AddChild(Button);
					Row->AddChild(Size);
					Cast<UHorizontalBoxSlot>(Size->Slot)->SetPadding(FMargin(4, 0));
				}
			}
		}
		for (int32 Index = 0; Index < 3; ++Index)
			for (const TCHAR* Direction : {TEXT("Down"), TEXT("Up")})
			{
				const FString Name = FString::Printf(TEXT("Tax%s%dLabel"), Direction, Index);
				if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(FName(Name))))
				{
					auto Font = Label->GetFont();
					Font.Size = 13;
					Label->SetFont(Font);
					Cast<UButtonSlot>(Label->Slot)->SetPadding(FMargin(0));
				}
			}
		if (!Tree->FindWidget(TEXT("TerrainPanel")))
		{
			auto* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TerrainPanel"));
			Panel->SetBrushColor(FLinearColor(.012f, .025f, .04f, .97f));
			Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 12.f, FLinearColor(.05f, .09f, .12f), 1.f));
			Panel->SetPadding(FMargin(10));
			Canvas->AddChildToCanvas(Panel)->SetOffsets(FMargin(500, 100, 320, 400));
			Panel->AddChild(Tree->ConstructWidget<UCityDesktopTerrainPreview>(UCityDesktopTerrainPreview::StaticClass(),
			                                                                  TEXT("TerrainPreview")));
		}
		return true;
	}
	bool AddCharts(UWidgetTree* Tree)
	{
		auto Add = [Tree](const TCHAR* Name, const TCHAR* Before)
		{
			if (Tree->FindWidget(Name))
				return true;
			auto* Anchor = Tree->FindWidget(Before);
			auto* Parent = Anchor ? Cast<UVerticalBox>(Anchor->GetParent()) : nullptr;
			if (!Parent)
				return false;
			auto* Chart = Tree->ConstructWidget<UCityDesktopChart>(UCityDesktopChart::StaticClass(), Name);
			Parent->InsertChildAt(Parent->GetChildIndex(Anchor), Chart);
			Cast<UVerticalBoxSlot>(Chart->Slot)->SetPadding(FMargin(0, 4));
			return true;
		};
		return Add(TEXT("ReportChart"), TEXT("MenuButtons")) && Add(TEXT("DemandChart"), TEXT("CityCard0")) &&
		       Add(TEXT("CapacityChart"), TEXT("CityCard0")) && Add(TEXT("DensityChart"), TEXT("Growth"));
	}
	int32 SaveDesktopBlueprint(UWidgetBlueprint* Blueprint, UPackage* Package, const TCHAR* Path)
	{
		TSet<FName> WidgetNames;
		Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget) { WidgetNames.Add(Widget->GetFName()); });
		for (auto It = Blueprint->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
			if (!WidgetNames.Contains(It.Key()))
				It.RemoveCurrent();
		Blueprint->WidgetTree->ForEachWidget(
		    [Blueprint](UWidget* Widget)
		    {
			    if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
				    Blueprint->OnVariableAdded(Widget->GetFName());
		    });
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
			return 2;
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(
		        Package, Blueprint,
		        *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), Args))
			return 3;
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_DESKTOP_UI_ASSET_SAVED SharedContent=1 BlueprintComposition=1 Charts=1"));
		return 0;
	}
} // namespace
#endif

UCityDesktopUiAssetsCommandlet::UCityDesktopUiAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}
int32 UCityDesktopUiAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const TCHAR* Path = TEXT("/Game/CitySim/UI/WBP_CityDesktopHud");
	auto* Blueprint = LoadObject<UWidgetBlueprint>(
	    nullptr, TEXT("/Game/CitySim/UI/WBP_CityDesktopHud.WBP_CityDesktopHud"), nullptr, LOAD_DisableCompileOnLoad);
	if (Blueprint && !FParse::Param(*Params, TEXT("Rebuild")))
	{
		if (FParse::Param(*Params, TEXT("Conditions")))
		{
			if (!AddConditions(Blueprint->WidgetTree))
				return 4;
			return SaveDesktopBlueprint(Blueprint, Blueprint->GetOutermost(), Path);
		}
		if (FParse::Param(*Params,TEXT("Newspaper")))
		{
			if (!AddNewspaper(Blueprint->WidgetTree)) return 4;
			return SaveDesktopBlueprint(Blueprint,Blueprint->GetOutermost(),Path);
		}
		if (FParse::Param(*Params, TEXT("ToolRefinements")))
		{
			if (!AddToolRefinements(Blueprint->WidgetTree))
				return 4;
			return SaveDesktopBlueprint(Blueprint, Blueprint->GetOutermost(), Path);
		}
		if (FParse::Param(*Params, TEXT("MenuRefinements")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("ReportChart")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("DemandChart")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("CapacityChart")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("DensityChart")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("TaxRows")) ||
		    !Blueprint->WidgetTree->FindWidget(TEXT("TerrainPanel")))
		{
			if (!AddCharts(Blueprint->WidgetTree) || !AddMenuRefinements(Blueprint->WidgetTree))
				return 4;
			return SaveDesktopBlueprint(Blueprint, Blueprint->GetOutermost(), Path);
		}
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_UI_ASSET_RETAINED Existing Blueprint layout preserved"));
		return 0;
	}
	UPackage* Package = CreatePackage(Path);
	if (!Blueprint)
	{
		Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		    UCityDesktopHud::StaticClass(), Package, TEXT("WBP_CityDesktopHud"), BPTYPE_Normal,
		    UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		FAssetRegistryModule::AssetCreated(Blueprint);
	}
	if (Blueprint->WidgetTree)
		Blueprint->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	auto* Tree = Blueprint->WidgetTree.Get();
	auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesktopCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Tree->RootWidget = Canvas;
	auto Label = [Tree](UPanelWidget* Parent, const FString& Name, const FString& Initial, const int32 Size = 15)
	{
		auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		Text->SetText(FText::FromString(Initial));
		// Keep UTextBlock's serializable Engine font asset. CoreStyle's transient
		// composite font pointer does not survive Blueprint saving/cooking.
		auto Font = Text->GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = TEXT("Regular");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FLinearColor(.9, .94, 1));
		Text->SetAutoWrapText(true);
		Parent->AddChild(Text);
		return Text;
	};
	auto Button = [Tree, &Label](UPanelWidget* Parent, const FString& Name, const FString& Initial)
	{
		auto* Item =
		    Tree->ConstructWidget<UCityDesktopActionButton>(UCityDesktopActionButton::StaticClass(), FName(Name));
		Item->SetBackgroundColor(FLinearColor(.08, .13, .20));
		auto Style = Item->GetStyle();
		Style.Normal = FSlateRoundedBoxBrush(FLinearColor::White, 8.f);
		Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(.72f, .87f, .87f), 8.f);
		Style.Pressed = FSlateRoundedBoxBrush(FLinearColor(.5f, .75f, .7f), 8.f);
		Style.NormalPadding = FMargin(10, 7);
		Style.PressedPadding = FMargin(10, 7);
		Item->SetStyle(Style);
		auto* Content = Tree->ConstructWidget<UVerticalBox>();
		Item->AddChild(Content);
		Cast<UButtonSlot>(Content->Slot)->SetPadding(FMargin(0));
		auto* Icon =
		    Tree->ConstructWidget<UCityDesktopIcon>(UCityDesktopIcon::StaticClass(), FName(Name + TEXT("Icon")));
		Content->AddChild(Icon);
		Cast<UVerticalBoxSlot>(Icon->Slot)->SetHorizontalAlignment(HAlign_Center);
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		auto* ButtonText = Label(Content, Name + TEXT("Label"), Initial, Name.StartsWith(TEXT("Toolbar")) ? 16 : 13);
		ButtonText->SetAutoWrapText(false);
		ButtonText->SetJustification(ETextJustify::Center);
		Cast<UVerticalBoxSlot>(ButtonText->Slot)->SetHorizontalAlignment(HAlign_Center);
		Parent->AddChild(Item);
		if (auto* Slot = Cast<UHorizontalBoxSlot>(Item->Slot))
			Slot->SetPadding(FMargin(2, 0));
		return Item;
	};
	auto Panel = [Tree, Canvas](const TCHAR* Name, const FAnchors& Anchors, const FMargin& Offsets)
	{
		auto* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(FLinearColor(.012, .025, .04, .97));
		Border->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 12.f, FLinearColor(.05f, .09f, .12f), 1.f));
		Border->SetPadding(FMargin(10));
		auto* Slot = Canvas->AddChildToCanvas(Border);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		auto* Column = Tree->ConstructWidget<UVerticalBox>();
		Border->AddChild(Column);
		return Column;
	};
	auto* Top = Panel(TEXT("TopPanel"), FAnchors(0, 0, 1, 0), FMargin(20, 16, 20, 64));
	auto* HeaderRow = Tree->ConstructWidget<UHorizontalBox>();
	Top->AddChild(HeaderRow);
	auto* Status = Tree->ConstructWidget<UHorizontalBox>();
	HeaderRow->AddChild(Status);
	Cast<UHorizontalBoxSlot>(Status->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Cast<UHorizontalBoxSlot>(Status->Slot)->SetVerticalAlignment(VAlign_Center);
	for (const TCHAR* Name : {TEXT("Funds"), TEXT("Population"), TEXT("Calendar")})
	{
		auto* Icon = Tree->ConstructWidget<UCityDesktopIcon>(UCityDesktopIcon::StaticClass(),
		                                                     FName(FString(Name) + TEXT("Icon")));
		Icon->Symbol = FCString::Strcmp(Name, TEXT("Funds")) == 0        ? TEXT("Money")
		               : FCString::Strcmp(Name, TEXT("Population")) == 0 ? TEXT("People")
		                                                                 : TEXT("Calendar");
		Status->AddChild(Icon);
		Icon->Tint = UCityDesktopIcon::ColorForSymbol(Icon->Symbol);
		Cast<UHorizontalBoxSlot>(Icon->Slot)->SetPadding(FMargin(0, 0, 8, 0));
		auto* Value = Label(Status, Name, Name, 15);
		Value->SetAutoWrapText(false);
		Cast<UHorizontalBoxSlot>(Value->Slot)->SetVerticalAlignment(VAlign_Center);
		Cast<UHorizontalBoxSlot>(Value->Slot)->SetPadding(FMargin(0, 0, 20, 0));
	}
	Label(Top, TEXT("Totals"), TEXT("City totals"), 13)->SetVisibility(ESlateVisibility::Collapsed);
	Label(Top, TEXT("Weather"), TEXT("Weather"), 12)->SetVisibility(ESlateVisibility::Collapsed);
	Label(Top, TEXT("Trends"), TEXT("Monthly net after full bills"), 12)->SetVisibility(ESlateVisibility::Collapsed);
	Label(Top, TEXT("Budget"), TEXT("Income and full monthly bills"), 13);
	Label(Top, TEXT("LastNet"), TEXT("Last settled net"), 13);
	auto* Time = Tree->ConstructWidget<UHorizontalBox>();
	HeaderRow->AddChild(Time);
	Cast<UHorizontalBoxSlot>(Time->Slot)->SetVerticalAlignment(VAlign_Center);
	for (const TCHAR* Name : {TEXT("Pause"), TEXT("Speed1"), TEXT("Speed3"), TEXT("Speed5")})
	{
		Button(Time, Name, Name);
	}
	auto* CityActions = Tree->ConstructWidget<UHorizontalBox>();
	Time->AddChild(CityActions);
	for (const TCHAR* Name :
	     {TEXT("CityToggle"), TEXT("ObjectivesToggle"), TEXT("MenuToggle"), TEXT("Save"), TEXT("Load")})
		Button(CityActions, Name, Name);
	auto* Toolbar = Panel(TEXT("ToolbarPanel"), FAnchors(.5, 1), FMargin(-390, -98, 780, 82));
	auto* Tools = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ToolButtons"));
	Toolbar->AddChild(Tools);
	// Keep editing tools together, followed by overlays/city/settings navigation.
	for (const int32 Index : {0, 7, 1, 2, 5, 3, 4, 6})
	{
		auto* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		                                            FName(FString::Printf(TEXT("ToolbarSize%d"), Index)));
		Box->SetWidthOverride(98);
		Box->SetHeightOverride(62);
		Tools->AddChild(Box);
		Cast<UHorizontalBoxSlot>(Box->Slot)->SetPadding(FMargin(Index == 3 ? 13 : 3, 0, 3, 0));
		const FString Name = FString::Printf(TEXT("Toolbar%d"), Index);
		Button(Box, Name, TEXT("Tool"));
		Tree->FindWidget(FName(Name + TEXT("Icon")))->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	auto* Context = Panel(TEXT("ToolContextPanel"), FAnchors(.5, 1), FMargin(-390, -178, 780, 72));
	auto* ContextHeader = Tree->ConstructWidget<UHorizontalBox>();
	Context->AddChild(ContextHeader);
	auto* ActiveTool = Label(ContextHeader, TEXT("Tool"), TEXT("Inspect"), 15);
	Cast<UHorizontalBoxSlot>(ActiveTool->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ActiveTool->SetAutoWrapText(false);
	for (const TCHAR* Name : {TEXT("Rotate"), TEXT("RoadDown"), TEXT("RoadUp"), TEXT("Cancel")})
		Button(ContextHeader, Name, Name);
	Label(Context, TEXT("Construction"), TEXT("Placement cost"), 13);
	auto* Menu = Panel(TEXT("MenuPanel"), FAnchors(0, 0), FMargin(20, 132, 460, 360));
	auto* MenuHeader = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MenuHeader"));
	Menu->AddChild(MenuHeader);
	auto* MenuTitle = Label(MenuHeader, TEXT("MenuTitle"), TEXT("Main Menu"), 20);
	MenuTitle->SetAutoWrapText(false);
	Cast<UHorizontalBoxSlot>(MenuTitle->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Button(MenuHeader, TEXT("MenuClose"), TEXT("CLOSE"));
	auto* MenuScroll = Tree->ConstructWidget<UScrollBox>();
	Menu->AddChild(MenuScroll);
	Cast<UVerticalBoxSlot>(MenuScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* MenuContent = Tree->ConstructWidget<UVerticalBox>();
	MenuScroll->AddChild(MenuContent);
	Label(MenuContent, TEXT("MenuBody"), TEXT("Choose a city"));
	auto* MenuButtons = Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("MenuButtons"));
	MenuButtons->SetExplicitWrapSize(true);
	MenuButtons->SetInnerSlotPadding(FVector2D(6, 6));
	MenuContent->AddChild(MenuButtons);
	for (int32 Index = 0; Index < 19; ++Index)
		Button(MenuButtons, FString::Printf(TEXT("Menu%d"), Index), TEXT("Action"));
	auto* Objectives = Panel(TEXT("ObjectivesPanel"), FAnchors(0, 0), FMargin(20, 132, 320, 160));
	auto* ObjectivesHeader = Tree->ConstructWidget<UHorizontalBox>();
	Objectives->AddChild(ObjectivesHeader);
	auto* ObjectivesTitle = Label(ObjectivesHeader, TEXT("ObjectivesTitle"), TEXT("Objectives"), 17);
	Cast<UHorizontalBoxSlot>(ObjectivesTitle->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Button(ObjectivesHeader, TEXT("ObjectivesClose"), TEXT("HIDE"));
	auto* ObjectivesScroll = Tree->ConstructWidget<UScrollBox>();
	Objectives->AddChild(ObjectivesScroll);
	Cast<UVerticalBoxSlot>(ObjectivesScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* ObjectivesContent = Tree->ConstructWidget<UVerticalBox>();
	ObjectivesScroll->AddChild(ObjectivesContent);
	Label(ObjectivesContent, TEXT("ObjectiveTitle"), TEXT("Connect your city"), 15);
	Label(ObjectivesContent, TEXT("Objectives"), TEXT("Objectives"), 12);
	auto* History = Tree->ConstructWidget<UHorizontalBox>();
	ObjectivesContent->AddChild(History);
	Button(History, TEXT("History"), TEXT("HISTORY"));
	Button(History, TEXT("NextObjective"), TEXT("NEXT"));
	auto* Inspector = Panel(TEXT("InspectorPanel"), FAnchors(1, 0), FMargin(-360, 132, 340, 440));
	auto* InspectorHeader = Tree->ConstructWidget<UHorizontalBox>();
	Inspector->AddChild(InspectorHeader);
	auto* InspectorTitle = Label(InspectorHeader, TEXT("InspectionTitle"), TEXT("Building"), 17);
	Cast<UHorizontalBoxSlot>(InspectorTitle->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Button(InspectorHeader, TEXT("InspectorClose"), TEXT("Close"));
	auto* InspectorScroll = Tree->ConstructWidget<UScrollBox>();
	Inspector->AddChild(InspectorScroll);
	Cast<UVerticalBoxSlot>(InspectorScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* InspectorContent = Tree->ConstructWidget<UVerticalBox>();
	InspectorScroll->AddChild(InspectorContent);
	auto* PreviewSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewSize"));
	PreviewSize->SetHeightOverride(170);
	InspectorContent->AddChild(PreviewSize);
	auto* PreviewFit = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PreviewFit"));
	PreviewFit->SetStretch(EStretch::ScaleToFit);
	PreviewSize->AddChild(PreviewFit);
	auto* Preview = Tree->ConstructWidget<UCityDesktopModelPreview>(UCityDesktopModelPreview::StaticClass(),
	                                                                TEXT("BuildingPreview"));
	PreviewFit->AddChild(Preview);
	Label(InspectorContent, TEXT("InspectionSummary"), TEXT("Selected building"), 13);
	auto* InspectionChart =
	    Tree->ConstructWidget<UCityDesktopChart>(UCityDesktopChart::StaticClass(), TEXT("InspectionChart"));
	InspectorContent->AddChild(InspectionChart);
	Button(InspectorContent, TEXT("Upgrade"), TEXT("Upgrade"));
	Button(InspectorContent, TEXT("InspectionDetails"), TEXT("Details"));
	Label(InspectorContent, TEXT("Inspection"), TEXT("Building details"), 13);
	auto* Info = Panel(TEXT("InfoPanel"), FAnchors(1, 0), FMargin(-360, 132, 340, 440));
	auto* InfoHeader = Tree->ConstructWidget<UHorizontalBox>();
	Info->AddChild(InfoHeader);
	auto* InfoTitle = Label(InfoHeader, TEXT("InfoTitle"), TEXT("CITY INFO / INSPECT"), 20);
	Cast<UHorizontalBoxSlot>(InfoTitle->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Button(InfoHeader, TEXT("InfoClose"), TEXT("HIDE"));
	Button(Info, TEXT("CityDetails"), TEXT("Details"));
	auto* InfoScroll = Tree->ConstructWidget<UScrollBox>();
	Info->AddChild(InfoScroll);
	Cast<UVerticalBoxSlot>(InfoScroll->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* InfoContent = Tree->ConstructWidget<UVerticalBox>();
	InfoScroll->AddChild(InfoContent);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FString Name = FString::Printf(TEXT("CityCard%d"), Index);
		auto* Card = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(Name));
		Card->SetBrushColor(FLinearColor(.04, .08, .12));
		Card->SetPadding(FMargin(8));
		InfoContent->AddChild(Card);
		Cast<UVerticalBoxSlot>(Card->Slot)->SetPadding(FMargin(0, 4));
		auto* Content = Tree->ConstructWidget<UVerticalBox>();
		Card->AddChild(Content);
		Label(Content, Name + TEXT("Title"), TEXT("City information"), 16);
		auto* Values = Tree->ConstructWidget<UHorizontalBox>();
		Content->AddChild(Values);
		auto* Labels = Label(Values, Name + TEXT("Labels"), TEXT("Metric"), 14);
		Labels->SetAutoWrapText(false);
		Cast<UHorizontalBoxSlot>(Labels->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		auto* Numbers = Label(Values, Name + TEXT("Values"), TEXT("Value"), 14);
		Numbers->SetAutoWrapText(false);
		Cast<UHorizontalBoxSlot>(Numbers->Slot)->SetPadding(FMargin(10, 0));
	}
	Label(InfoContent, TEXT("Growth"), TEXT("Needs for growth"));
	if (!AddCharts(Tree) || !AddMenuRefinements(Tree))
		return 4;
	if (!AddInspectionRefinements(Tree))
		return 4;
	if (!AddNewspaper(Tree))
		return 4;
	if (!AddConditions(Tree))
		return 4;
	return SaveDesktopBlueprint(Blueprint, Package, Path);
#else
	return 1;
#endif
}
