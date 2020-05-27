// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "NodeDocsGenerator.h"
#include "KantanDocGenLog.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "NodeFactory.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintBoundNodeSpawner.h"
#include "BlueprintComponentNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Message.h"
#include "HighResScreenshot.h"
#include "XmlFile.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "ThreadingHelpers.h"
#include "Stats/StatsMisc.h"
#include "Runtime/ImageWriteQueue/Public/ImageWriteTask.h"
#include "Misc/FileHelper.h"

FNodeDocsGenerator::~FNodeDocsGenerator()
{
	CleanUp();
}

bool FNodeDocsGenerator::GT_Init(FString const& InDocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass)
{
	DummyBP = CastChecked< UBlueprint >(FKismetEditorUtilities::CreateBlueprint(
		BlueprintContextClass,
		::GetTransientPackage(),
		NAME_None,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None
	));
	if(!DummyBP.IsValid())
	{
		return false;
	}

	Graph = FBlueprintEditorUtils::CreateNewGraph(DummyBP.Get(), TEXT("TempoGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

	DummyBP->AddToRoot();
	Graph->AddToRoot();

	GraphPanel = SNew(SGraphPanel)
		.GraphObj(Graph.Get())
		;
	// We want full detail for rendering, passing a super-high zoom value will guarantee the highest LOD.
	GraphPanel->RestoreViewSettings(FVector2D(0, 0), 10.0f);

	DocsTitle = InDocsTitle;

	IndexXml = InitIndexXml(DocsTitle);
	ClassDocsMap.Empty();

	OutputDir = InOutputDir;

	return true;
}

UK2Node* FNodeDocsGenerator::GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject, FNodeProcessingState& OutState)
{
	if(!IsSpawnerDocumentable(Spawner, SourceObject->IsA< UBlueprint >()))
	{
		return nullptr;
	}

	// Spawn an instance into the graph
	auto NodeInst = Spawner->Invoke(Graph.Get(), TSet< TWeakObjectPtr< UObject > >(), FVector2D(0, 0));

	// Currently Blueprint nodes only
	auto K2NodeInst = Cast< UK2Node >(NodeInst);

	if(K2NodeInst == nullptr)
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to create node from spawner of class %s with node class %s."), *Spawner->GetClass()->GetName(), Spawner->NodeClass ? *Spawner->NodeClass->GetName() : TEXT("None"));
		return nullptr;
	}

	auto AssociatedClass = MapToAssociatedClass(K2NodeInst, SourceObject);

	if(!ClassDocsMap.Contains(AssociatedClass))
	{
		// New class xml file needs adding
		ClassDocsMap.Add(AssociatedClass, InitClassDocXml(AssociatedClass));
		// Also update the index xml
		UpdateIndexDocWithClass(IndexXml.Get(), AssociatedClass);
	}

	OutState = FNodeProcessingState();
	OutState.ClassDocXml = ClassDocsMap.FindChecked(AssociatedClass);
	OutState.ClassDocsPath = OutputDir / GetClassDocId(AssociatedClass);

	return K2NodeInst;
}

bool FNodeDocsGenerator::GT_Finalize(FString OutputPath)
{
	if(!SaveClassDocXml(OutputPath))
	{
		return false;
	}

	if(!SaveIndexXml(OutputPath))
	{
		return false;
	}

	if(!CopyStaticAssets(OutputPath))
	{
		return false;
	}

	return true;
}

void FNodeDocsGenerator::CleanUp()
{
	if(GraphPanel.IsValid())
	{
		GraphPanel.Reset();
	}

	if(DummyBP.IsValid())
	{
		DummyBP->RemoveFromRoot();
		DummyBP.Reset();
	}
	if(Graph.IsValid())
	{
		Graph->RemoveFromRoot();
		Graph.Reset();
	}
}

bool FNodeDocsGenerator::GenerateNodeImage(UEdGraphNode* Node, FNodeProcessingState& State)
{
	SCOPE_SECONDS_COUNTER(GenerateNodeImageTime);

	const FVector2D DrawSize(1024.0f, 1024.0f);

	bool bSuccess = false;

	AdjustNodeForSnapshot(Node);

	FString NodeName = GetNodeDocId(Node);

	FIntRect Rect;

	TUniquePtr<TImagePixelData<FColor>> PixelData;

	bSuccess = DocGenThreads::RunOnGameThreadRetVal([this, Node, DrawSize, &Rect, &PixelData]
	{
		auto NodeWidget = FNodeFactory::CreateNodeWidget(Node);
		NodeWidget->SetOwner(GraphPanel.ToSharedRef());

		const bool bUseGammaCorrection = false;
		FWidgetRenderer Renderer(bUseGammaCorrection);
		Renderer.SetIsPrepassNeeded(true);
		auto RenderTarget = Renderer.DrawWidget(NodeWidget.ToSharedRef(), DrawSize);

		auto Desired = NodeWidget->GetDesiredSize();

		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		Rect = FIntRect(0, 0, (int32)Desired.X, (int32)Desired.Y);
		FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
		ReadPixelFlags.SetLinearToGamma(true); // @TODO: is this gamma correction, or something else?

		PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint((int32)Desired.X, (int32)Desired.Y));

		if(RTResource->ReadPixels(PixelData->Pixels, ReadPixelFlags, Rect) == false)
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to read pixels for node image."));
			return false;
		}

		return true;
	});

	if(!bSuccess)
	{
		return false;
	}

	State.RelImageBasePath = TEXT(".");
	FString ImageBasePath = State.ClassDocsPath / NodeName;
	FString ImgFilename;
	if(Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown)
	{
		ImgFilename = FString::Printf(TEXT("%s_advanced.png"), *NodeName);
	}
	else
	{
		ImgFilename = FString::Printf(TEXT("%s.png"), *NodeName);
	}

	FString ScreenshotSaveName = ImageBasePath / ImgFilename;

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
	ImageTask->PixelData = MoveTemp(PixelData);
	ImageTask->Filename = ScreenshotSaveName;
	ImageTask->Format = EImageFormat::PNG;
	ImageTask->CompressionQuality = (int32)EImageCompressionQuality::Default;
	ImageTask->bOverwriteFile = true;
	ImageTask->PixelPreProcessors.Add(TAsyncAlphaWrite<FColor>(255));

	if(ImageTask->RunTask())
	{
		// Success!
		bSuccess = true;
		if(Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown)
		{
			State.AdvancedImageFilename = ImgFilename;
		}
		else
		{
			State.ImageFilename = ImgFilename;
		}
	}
	else
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to save screenshot image for node: %s"), *NodeName);
	}

	return bSuccess;
}

inline FString WrapAsCDATA(FString const& InString)
{
	return TEXT("<![CDATA[") + InString + TEXT("]]>");
}

inline FXmlNode* AppendChild(FXmlNode* Parent, FString const& Name)
{
	Parent->AppendChildNode(Name, FString());
	return Parent->GetChildrenNodes().Last();
}

inline FXmlNode* AppendChildRaw(FXmlNode* Parent, FString const& Name, FString const& TextContent)
{
	Parent->AppendChildNode(Name, TextContent);
	return Parent->GetChildrenNodes().Last();
}

inline FXmlNode* AppendChildCDATA(FXmlNode* Parent, FString const& Name, FString const& TextContent)
{
	Parent->AppendChildNode(Name, WrapAsCDATA(TextContent));
	return Parent->GetChildrenNodes().Last();
}

// For K2 pins only!
bool ExtractPinInformation(UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription)
{
	FString Tooltip;
	Pin->GetOwningNode()->GetPinHoverText(*Pin, Tooltip);

	if(!Tooltip.IsEmpty())
	{
		// @NOTE: This is based on the formatting in UEdGraphSchema_K2::ConstructBasicPinTooltip.
		// If that is changed, this will fail!

		auto TooltipPtr = *Tooltip;

		// Parse name line
		FParse::Line(&TooltipPtr, OutName);
		// Parse type line
		FParse::Line(&TooltipPtr, OutType);

		// Currently there is an empty line here, but FParse::Line seems to gobble up empty lines as part of the previous call.
		// Anyway, attempting here to deal with this generically in case that weird behaviour changes.
		while(*TooltipPtr == TEXT('\n'))
		{
			FString Buf;
			FParse::Line(&TooltipPtr, Buf);
		}

		// What remains is the description
		OutDescription = TooltipPtr;
	}

	// @NOTE: Currently overwriting the name and type as suspect this is more robust to future engine changes.

	OutName = Pin->GetDisplayName().ToString();
	if(OutName.IsEmpty() && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutName = Pin->Direction == EEdGraphPinDirection::EGPD_Input ? TEXT("In") : TEXT("Out");
	}

	OutType = UEdGraphSchema_K2::TypeToText(Pin->PinType).ToString();

	return true;
}

TSharedPtr< FXmlFile > FNodeDocsGenerator::InitIndexXml(FString const& IndexTitle)
{
	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<root></root>)xxx";

	TSharedPtr< FXmlFile > File = MakeShareable(new FXmlFile(FileTemplate, EConstructMethod::ConstructFromBuffer));
	auto Root = File->GetRootNode();

	AppendChildRaw(Root, TEXT("name"), IndexTitle);
	AppendChild(Root, TEXT("classes"));

	return File;
}

TSharedPtr< FXmlFile > FNodeDocsGenerator::InitClassDocXml(UClass* Class)
{
	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<class></class>)xxx";

	TSharedPtr< FXmlFile > File = MakeShareable(new FXmlFile(FileTemplate, EConstructMethod::ConstructFromBuffer));
	auto Root = File->GetRootNode();

	AppendChildRaw(Root, TEXT("id"), GetClassDocId(Class));
	AppendChildRaw(Root, TEXT("name"), FBlueprintEditorUtils::GetFriendlyClassDisplayName(Class).ToString());
	AppendChild(Root, TEXT("functions"));

	auto PathNode = AppendChild(Root, TEXT("path"));
	auto ParentPathNode = AppendChild(PathNode, TEXT("parent"));
	AppendChildRaw(ParentPathNode, TEXT("uri"), TEXT("../index.xml"));
	AppendChildRaw(ParentPathNode, TEXT("name"), DocsTitle);

	return File;
}

bool FNodeDocsGenerator::UpdateIndexDocWithClass(FXmlFile* DocFile, UClass* Class)
{
	auto ClassId = GetClassDocId(Class);
	auto Classes = DocFile->GetRootNode()->FindChildNode(TEXT("classes"));
	auto ClassElem = AppendChild(Classes, TEXT("class"));
	AppendChildRaw(ClassElem, TEXT("id"), ClassId);
	AppendChildRaw(ClassElem, TEXT("name"), FBlueprintEditorUtils::GetFriendlyClassDisplayName(Class).ToString());
	return true;
}

bool FNodeDocsGenerator::UpdateClassDocWithNode(FXmlFile* DocFile, UEdGraphNode* Node)
{
	auto NodeId = GetNodeDocId(Node);
	auto Nodes = DocFile->GetRootNode()->FindChildNode(TEXT("functions"));
	auto NodeElem = AppendChild(Nodes, TEXT("function"));
	AppendChildRaw(NodeElem, TEXT("id"), NodeId);
	AppendChildRaw(NodeElem, TEXT("name"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	return true;
}

inline bool ShouldDocumentPin(UEdGraphPin* Pin)
{
	return !Pin->bHidden;
}

bool FNodeDocsGenerator::GenerateNodeDocs(UK2Node* Node, FNodeProcessingState& State)
{
	SCOPE_SECONDS_COUNTER(GenerateNodeDocsTime);

	FString DocFilePath = State.ClassDocsPath / GetNodeDocId(Node) / TEXT("index.xml");

	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<function></function>)xxx";

	FXmlFile File(FileTemplate, EConstructMethod::ConstructFromBuffer);
	auto Root = File.GetRootNode();

	auto PathNode = AppendChild(Root, TEXT("path"));
	auto ParentPathNode = AppendChild(PathNode, TEXT("parent"));
	AppendChildRaw(ParentPathNode, TEXT("uri"), TEXT("../../index.xml"));
	AppendChildRaw(ParentPathNode, TEXT("name"), DocsTitle);

	ParentPathNode = AppendChild(PathNode, TEXT("parent"));
	AppendChildRaw(ParentPathNode, TEXT("uri"), TEXT("../index.xml"));
	AppendChildRaw(ParentPathNode, TEXT("name"), State.ClassDocXml->GetRootNode()->FindChildNode(TEXT("id"))->GetContent());

	// Since we pull these from the class xml file, the entries are already CDATA wrapped
	AppendChildRaw(Root, TEXT("id"), GetNodeDocId(Node));

	FString NodeShortTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	AppendChildRaw(Root, TEXT("shorttitle"), NodeShortTitle.TrimEnd());

	FString NodeFullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	auto TargetIdx = NodeFullTitle.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if(TargetIdx != INDEX_NONE)
	{
		NodeFullTitle = NodeFullTitle.Left(TargetIdx).TrimEnd();
	}
	AppendChildRaw(Root, TEXT("name"), NodeFullTitle);

	FString NodeDesc = Node->GetTooltipText().ToString();
	TargetIdx = NodeDesc.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if(TargetIdx != INDEX_NONE)
	{
		NodeDesc = NodeDesc.Left(TargetIdx).TrimEnd();
	}
	AppendChildRaw(Root, TEXT("description"), NodeDesc);
	AppendChildRaw(Root, TEXT("category"), Node->GetMenuCategory().ToString());

	auto Images = AppendChild(Root, TEXT("images"));
	AppendChildRaw(Images, TEXT("simple"), State.RelImageBasePath / State.ImageFilename);
	if(State.AdvancedImageFilename != "")
	{
		AppendChildRaw(Images, TEXT("advanced"), State.RelImageBasePath / State.AdvancedImageFilename);
	}

	auto Inputs = AppendChild(Root, TEXT("inputs"));
	for(auto Pin : Node->Pins)
	{
		if(Pin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			if(ShouldDocumentPin(Pin))
			{
				auto Input = AppendChild(Inputs, TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				AppendChildRaw(Input, TEXT("name"), PinName);
				AppendChildRaw(Input, TEXT("type"), PinType);
				AppendChildRaw(Input, TEXT("description"), PinDesc);
			}
		}
	}

	auto Outputs = AppendChild(Root, TEXT("outputs"));
	for(auto Pin : Node->Pins)
	{
		if(Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			if(ShouldDocumentPin(Pin))
			{
				auto Output = AppendChild(Outputs, TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				AppendChildRaw(Output, TEXT("name"), PinName);
				AppendChildRaw(Output, TEXT("type"), PinType);
				AppendChildRaw(Output, TEXT("description"), PinDesc);
			}
		}
	}

	if(!File.Save(DocFilePath))
	{
		return false;
	}

	if(!AddStylesheetToXml(DocFilePath, TEXT("../../static/transform.xslt")))
	{
		return false;
	}

	if(!UpdateClassDocWithNode(State.ClassDocXml.Get(), Node))
	{
		return false;
	}

	return true;
}

bool FNodeDocsGenerator::AddStylesheetToXml(FString const XmlFile, FString const Stylesheet)
{
	TArray<FString> Lines;
	if(FFileHelper::LoadFileToStringArray(Lines, *XmlFile))
	{
		Lines.Insert(TEXT("<?xml-stylesheet type=\"text/xsl\" href=\"" + Stylesheet + "\"?>"), 1);
		return FFileHelper::SaveStringArrayToFile(Lines, *XmlFile);
	}
	else
	{
		return false;
	}
}

bool FNodeDocsGenerator::SaveIndexXml(FString const& OutDir)
{
	auto Path = OutDir / TEXT("index.xml");
	IndexXml->Save(Path);

	AddStylesheetToXml(Path, TEXT("static/transform.xslt"));

	return true;
}

bool FNodeDocsGenerator::SaveClassDocXml(FString const& OutDir)
{
	for(auto const& Entry : ClassDocsMap)
	{
		auto ClassId = GetClassDocId(Entry.Key.Get());
		auto Path = OutDir / ClassId / TEXT("index.xml");
		Entry.Value->Save(Path);

		AddStylesheetToXml(Path, TEXT("../static/transform.xslt"));
	}

	return true;
}

bool FNodeDocsGenerator::CopyStaticAssets(FString const& OutDir)
{
	UE_LOG(LogKantanDocGen, Log, TEXT("Copying static assets"));
	auto& PluginManager = IPluginManager::Get();
	auto Plugin = PluginManager.FindPlugin(TEXT("KantanDocGen"));
	if(!Plugin.IsValid())
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Failed to locate plugin info"));
		return false;
	}

	TArray<FString> Filenames;
	const FString AssetPath = Plugin->GetBaseDir() / TEXT("Source") / TEXT("static");
	IFileManager& FileManager = IFileManager::Get();
	FileManager.FindFilesRecursive(Filenames, *AssetPath, TEXT("*"), true, false, false);

	for(FString Filename : Filenames)
	{
		FString Destination = Filename;
		Destination.RemoveFromStart(AssetPath);
		Destination = OutDir / TEXT("static") / Destination;

		FileManager.Copy(*Destination, *Filename);
	}

	FileManager.FindFilesRecursive(Filenames, *OutDir, TEXT("*"), false, true, true);
	FString IndexHTML = OutDir / TEXT("static") / TEXT("index.html");
	for(FString Filename : Filenames)
	{
		FString Destination = Filename / TEXT("index.html");
		FileManager.Copy(*Destination, *IndexHTML);
	}

	return true;
}

void FNodeDocsGenerator::AdjustNodeForSnapshot(UEdGraphNode* Node)
{
	// Hide default value box containing 'self' for Target pin
	if(auto K2_Schema = Cast< UEdGraphSchema_K2 >(Node->GetSchema()))
	{
		if(auto TargetPin = Node->FindPin(K2_Schema->PN_Self))
		{
			TargetPin->bDefaultValueIsIgnored = true;
		}
	}
}

FString FNodeDocsGenerator::GetClassDocId(UClass* Class)
{
	return Class->GetName();
}

FString FNodeDocsGenerator::GetNodeDocId(UEdGraphNode* Node)
{
	// @TODO: Not sure this is right thing to use
	return Node->GetDocumentationExcerptName();
}


#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

/*
This takes a graph node object and attempts to map it to the class which the node conceptually belong to.
If there is no special mapping for the node, the function determines the class from the source object.
*/
UClass* FNodeDocsGenerator::MapToAssociatedClass(UK2Node* NodeInst, UObject* Source)
{
	// For nodes derived from UK2Node_CallFunction, associate with the class owning the called function.
	if(auto FuncNode = Cast< UK2Node_CallFunction >(NodeInst))
	{
		auto Func = FuncNode->GetTargetFunction();
		if(Func)
		{
			return Func->GetOwnerClass();
		}
	}

	// Default fallback
	if(auto SourceClass = Cast< UClass >(Source))
	{
		return SourceClass;
	}
	else if(auto SourceBP = Cast< UBlueprint >(Source))
	{
		return SourceBP->GeneratedClass;
	}
	else
	{
		return nullptr;
	}
}

bool FNodeDocsGenerator::IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint)
{
	// Spawners of or deriving from the following classes will be excluded
	static const TSubclassOf< UBlueprintNodeSpawner > ExcludedSpawnerClasses[] = {
		UBlueprintVariableNodeSpawner::StaticClass(),
		UBlueprintDelegateNodeSpawner::StaticClass(),
		UBlueprintBoundNodeSpawner::StaticClass(),
		UBlueprintComponentNodeSpawner::StaticClass(),
	};

	// Spawners of or deriving from the following classes will be excluded in a blueprint context
	static const TSubclassOf< UBlueprintNodeSpawner > BlueprintOnlyExcludedSpawnerClasses[] = {
		UBlueprintEventNodeSpawner::StaticClass(),
	};

	// Spawners for nodes of these types (or their subclasses) will be excluded
	static const TSubclassOf< UK2Node > ExcludedNodeClasses[] = {
		UK2Node_DynamicCast::StaticClass(),
		UK2Node_Message::StaticClass(),
	};

	// Function spawners for functions with any of the following metadata tags will also be excluded
	static const FName ExcludedFunctionMeta[] = {
		TEXT("BlueprintAutocast")
	};

	static const uint32 PermittedAccessSpecifiers = (FUNC_Public | FUNC_Protected);


	for(auto ExclSpawnerClass : ExcludedSpawnerClasses)
	{
		if(Spawner->IsA(ExclSpawnerClass))
		{
			return false;
		}
	}

	if(bIsBlueprint)
	{
		for(auto ExclSpawnerClass : BlueprintOnlyExcludedSpawnerClasses)
		{
			if(Spawner->IsA(ExclSpawnerClass))
			{
				return false;
			}
		}
	}

	for(auto ExclNodeClass : ExcludedNodeClasses)
	{
		if(Spawner->NodeClass->IsChildOf(ExclNodeClass))
		{
			return false;
		}
	}

	if(auto FuncSpawner = Cast< UBlueprintFunctionNodeSpawner >(Spawner))
	{
		auto Func = FuncSpawner->GetFunction();

		// @NOTE: We exclude based on access level, but only if this is not a spawner for a blueprint event
		// (custom events do not have any access specifiers)
		if((Func->FunctionFlags & FUNC_BlueprintEvent) == 0 && (Func->FunctionFlags & PermittedAccessSpecifiers) == 0)
		{
			return false;
		}

		for(auto const& Meta : ExcludedFunctionMeta)
		{
			if(Func->HasMetaData(Meta))
			{
				return false;
			}
		}
	}

	return true;
}

