#include "GameXXKBlueprintInspectorLibrary.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"

namespace GameXXKBlueprintInspector
{
	static FString JsonString(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	static TSharedRef<FJsonObject> MakeResult(bool bOk)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("ok"), bOk);
		return Object;
	}

	static FString ObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	static FString FieldClassName(const UObject* Object)
	{
		return Object && Object->GetClass() ? Object->GetClass()->GetName() : FString();
	}

	static FString PropertyClassName(const FProperty* Property)
	{
		return Property && Property->GetClass() ? Property->GetClass()->GetName() : FString();
	}

	static FString PinTypeToString(const FEdGraphPinType& Type)
	{
		FString Result = Type.PinCategory.ToString();
		if (!Type.PinSubCategory.IsNone())
		{
			Result += TEXT(":");
			Result += Type.PinSubCategory.ToString();
		}
		if (Type.PinSubCategoryObject.IsValid())
		{
			Result += TEXT(":");
			Result += Type.PinSubCategoryObject->GetPathName();
		}
		if (Type.ContainerType != EPinContainerType::None)
		{
			Result += FString::Printf(TEXT("[%d]"), static_cast<int32>(Type.ContainerType));
		}
		return Result;
	}

	static TSharedRef<FJsonObject> InspectProperty(const FProperty* Property)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		if (!Property)
		{
			return Object;
		}
		Object->SetStringField(TEXT("name"), Property->GetName());
		Object->SetStringField(TEXT("class"), PropertyClassName(Property));
		Object->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
		Object->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
		return Object;
	}

	static TSharedRef<FJsonObject> InspectPin(const UEdGraphPin* Pin)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		if (!Pin)
		{
			return Object;
		}
		Object->SetStringField(TEXT("name"), Pin->PinName.ToString());
		Object->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
		Object->SetStringField(TEXT("type"), PinTypeToString(Pin->PinType));
		Object->SetStringField(TEXT("default_value"), Pin->DefaultValue);
		Object->SetStringField(TEXT("default_object"), ObjectPath(Pin->DefaultObject));
		Object->SetBoolField(TEXT("hidden"), Pin->bHidden);
		Object->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);

		TArray<TSharedPtr<FJsonValue>> Links;
		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
			Link->SetStringField(TEXT("node"), LinkedPin && LinkedPin->GetOwningNode() ? LinkedPin->GetOwningNode()->GetName() : FString());
			Link->SetStringField(TEXT("pin"), LinkedPin ? LinkedPin->PinName.ToString() : FString());
			Links.Add(MakeShared<FJsonValueObject>(Link));
		}
		Object->SetArrayField(TEXT("linked_to"), Links);
		return Object;
	}

	static TSharedRef<FJsonObject> InspectNode(const UEdGraphNode* Node)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		if (!Node)
		{
			return Object;
		}
		Object->SetStringField(TEXT("name"), Node->GetName());
		Object->SetStringField(TEXT("class"), FieldClassName(Node));
		Object->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		Object->SetNumberField(TEXT("x"), Node->NodePosX);
		Object->SetNumberField(TEXT("y"), Node->NodePosY);
		Object->SetBoolField(TEXT("enabled"), Node->IsNodeEnabled());

		TArray<TSharedPtr<FJsonValue>> Pins;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			Pins.Add(MakeShared<FJsonValueObject>(InspectPin(Pin)));
		}
		Object->SetArrayField(TEXT("pins"), Pins);
		return Object;
	}

	static TSharedRef<FJsonObject> InspectGraph(const UEdGraph* Graph)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		if (!Graph)
		{
			Object->SetNumberField(TEXT("node_count"), 0);
			Object->SetArrayField(TEXT("nodes"), {});
			return Object;
		}
		Object->SetStringField(TEXT("name"), Graph->GetName());
		Object->SetStringField(TEXT("class"), FieldClassName(Graph));
		Object->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			Nodes.Add(MakeShared<FJsonValueObject>(InspectNode(Node)));
		}
		Object->SetArrayField(TEXT("nodes"), Nodes);
		return Object;
	}

	static void InspectWidgetRecursive(const UWidget* Widget, TArray<TSharedPtr<FJsonValue>>& Widgets, int32 Depth)
	{
		if (!Widget)
		{
			return;
		}

		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("name"), Widget->GetName());
		Object->SetStringField(TEXT("class"), FieldClassName(Widget));
		Object->SetNumberField(TEXT("depth"), Depth);
		Object->SetStringField(TEXT("visibility"), StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())));
		Object->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);

		TArray<TSharedPtr<FJsonValue>> Children;
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				const UWidget* Child = Panel->GetChildAt(Index);
				if (Child)
				{
					Children.Add(MakeShared<FJsonValueString>(Child->GetName()));
					InspectWidgetRecursive(Child, Widgets, Depth + 1);
				}
			}
		}
		Object->SetArrayField(TEXT("children"), Children);
		Widgets.Add(MakeShared<FJsonValueObject>(Object));
	}

	static TSharedRef<FJsonObject> InspectWidgetTree(const UBlueprint* Blueprint)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("root"), FString());
		Object->SetNumberField(TEXT("widget_count"), 0);
		Object->SetArrayField(TEXT("widgets"), {});

		const UWidgetBlueprintGeneratedClass* WidgetClass = Blueprint ? Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass) : nullptr;
		const UWidgetTree* WidgetTree = WidgetClass ? WidgetClass->GetWidgetTreeArchetype() : nullptr;
		const UWidget* RootWidget = WidgetTree ? WidgetTree->RootWidget : nullptr;
		if (!RootWidget)
		{
			return Object;
		}

		TArray<TSharedPtr<FJsonValue>> Widgets;
		InspectWidgetRecursive(RootWidget, Widgets, 0);
		Object->SetStringField(TEXT("root"), RootWidget->GetName());
		Object->SetNumberField(TEXT("widget_count"), Widgets.Num());
		Object->SetArrayField(TEXT("widgets"), Widgets);
		return Object;
	}

	static TSharedRef<FJsonObject> InspectBlueprint(UBlueprint* Blueprint, const FString& AssetPath)
	{
		TSharedRef<FJsonObject> Object = MakeResult(true);
		Object->SetStringField(TEXT("asset_path"), AssetPath);
		Object->SetStringField(TEXT("class"), FieldClassName(Blueprint));
		Object->SetStringField(TEXT("parent_class"), Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
		Object->SetStringField(TEXT("generated_class"), Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());

		TArray<TSharedPtr<FJsonValue>> Variables;
		if (Blueprint)
		{
			for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
			{
				TSharedRef<FJsonObject> VariableObject = MakeShared<FJsonObject>();
				VariableObject->SetStringField(TEXT("name"), Variable.VarName.ToString());
				VariableObject->SetStringField(TEXT("type"), PinTypeToString(Variable.VarType));
				VariableObject->SetStringField(TEXT("category"), Variable.Category.ToString());
				VariableObject->SetStringField(TEXT("default_value"), Variable.DefaultValue);
				Variables.Add(MakeShared<FJsonValueObject>(VariableObject));
			}
		}
		Object->SetArrayField(TEXT("variables"), Variables);

		TArray<UEdGraph*> Graphs;
		if (Blueprint)
		{
			Blueprint->GetAllGraphs(Graphs);
		}

		TArray<TSharedPtr<FJsonValue>> GraphValues;
		for (const UEdGraph* Graph : Graphs)
		{
			GraphValues.Add(MakeShared<FJsonValueObject>(InspectGraph(Graph)));
		}
		Object->SetArrayField(TEXT("graphs"), GraphValues);
		Object->SetNumberField(TEXT("graph_count"), GraphValues.Num());
		Object->SetObjectField(TEXT("widget_tree"), InspectWidgetTree(Blueprint));
		return Object;
	}

	static TSharedRef<FJsonObject> InspectEnum(UEnum* Enum, const FString& AssetPath)
	{
		TSharedRef<FJsonObject> Object = MakeResult(true);
		Object->SetStringField(TEXT("asset_path"), AssetPath);
		Object->SetStringField(TEXT("class"), FieldClassName(Enum));

		TArray<TSharedPtr<FJsonValue>> Enumerators;
		if (Enum)
		{
			for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
			{
				TSharedRef<FJsonObject> Enumerator = MakeShared<FJsonObject>();
				Enumerator->SetStringField(TEXT("name"), Enum->GetNameStringByIndex(Index));
				Enumerator->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(Index).ToString());
				Enumerator->SetNumberField(TEXT("value"), Enum->GetValueByIndex(Index));
				Enumerator->SetBoolField(TEXT("hidden"), Enum->HasMetaData(TEXT("Hidden"), Index));
				Enumerators.Add(MakeShared<FJsonValueObject>(Enumerator));
			}
		}
		Object->SetArrayField(TEXT("enumerators"), Enumerators);
		return Object;
	}

	static TSharedRef<FJsonObject> InspectStruct(UScriptStruct* Struct, const FString& AssetPath)
	{
		TSharedRef<FJsonObject> Object = MakeResult(true);
		Object->SetStringField(TEXT("asset_path"), AssetPath);
		Object->SetStringField(TEXT("class"), FieldClassName(Struct));

		TArray<TSharedPtr<FJsonValue>> Fields;
		if (Struct)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				Fields.Add(MakeShared<FJsonValueObject>(InspectProperty(*It)));
			}
		}
		Object->SetArrayField(TEXT("fields"), Fields);
		return Object;
	}
}

FString UGameXXKBlueprintInspectorLibrary::InspectAssetToJson(const FString& AssetPath)
{
	using namespace GameXXKBlueprintInspector;

	UObject* Asset = nullptr;
	const FSoftObjectPath SoftObjectPath(AssetPath);
	if (SoftObjectPath.IsValid())
	{
		Asset = SoftObjectPath.TryLoad();
	}
	if (!Asset)
	{
		Asset = LoadObject<UObject>(nullptr, *AssetPath);
	}
	if (!Asset)
	{
		TSharedRef<FJsonObject> Object = MakeResult(false);
		Object->SetStringField(TEXT("asset_path"), AssetPath);
		Object->SetStringField(TEXT("error"), TEXT("asset_load_failed"));
		return JsonString(Object);
	}

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		return JsonString(InspectBlueprint(Blueprint, AssetPath));
	}
	if (UEnum* Enum = Cast<UEnum>(Asset))
	{
		return JsonString(InspectEnum(Enum, AssetPath));
	}
	if (UScriptStruct* Struct = Cast<UScriptStruct>(Asset))
	{
		return JsonString(InspectStruct(Struct, AssetPath));
	}

	TSharedRef<FJsonObject> Object = MakeResult(true);
	Object->SetStringField(TEXT("asset_path"), AssetPath);
	Object->SetStringField(TEXT("class"), FieldClassName(Asset));
	Object->SetStringField(TEXT("note"), TEXT("unsupported_asset_type"));
	return JsonString(Object);
}
