// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "SvcGeometryDrawerSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void USvcGeometryDrawerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	m_bHasInitialized = false;
}

void USvcGeometryDrawerSubsystem::Deinitialize()
{
	m_bHasInitialized = false;

	if (m_DrawerManager)
	{
		m_DrawerManager->DeinitializeManager();
		m_DrawerManager = nullptr;
	}
	Super::Deinitialize();
}

void USvcGeometryDrawerSubsystem::PostInitialize()
{
	if (m_bHasInitialized)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
	{
		// 世界还不可用时，不做初始化，等待下次使用再尝试
		return;
	}

	if (!m_DrawerManager)
	{
		m_DrawerManager = NewObject<USvcGeometryDrawerManager>(this);
	}

	if (m_DrawerManager)
	{
		m_DrawerManager->InitializeManager(World);
		m_bHasInitialized = true;
	}
}

FString USvcGeometryDrawerSubsystem::DrawLine(const TArray<FVector>& Positions, const FLinearColor& Color, float Thickness)
{
	PostInitialize();
	return m_DrawerManager ? m_DrawerManager->DrawLine(Positions, Color, Thickness) : TEXT("");
}

FString USvcGeometryDrawerSubsystem::DrawPoint(const FVector& Location, const FLinearColor& Color, float Size)
{
	PostInitialize();
	return m_DrawerManager ? m_DrawerManager->DrawPoint(Location, Color, Size) : TEXT("");
}

FString USvcGeometryDrawerSubsystem::DrawCube(const FVector& Center, const FVector& Scale, const FLinearColor& Color)
{
	PostInitialize();
	return m_DrawerManager ? m_DrawerManager->DrawCube(Center, Scale, Color) : TEXT("");
}

bool USvcGeometryDrawerSubsystem::RemoveDrawObject(FString ObjectId)
{
	PostInitialize();

	if (!m_DrawerManager)
		return false;
	
	return m_DrawerManager->RemoveDrawObject(ObjectId);
	
}

bool USvcGeometryDrawerSubsystem::ClearDrawObjects(EGeometryDrawType Type)
{
	PostInitialize();

	if (!m_DrawerManager)
		return false;
	
	return m_DrawerManager->ClearDrawObjects(Type);
}

FString USvcGeometryDrawerSubsystem::DrawPointJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> RetObj = MakeShareable(new FJsonObject);

    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Invalid JSON."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    const TSharedPtr<FJsonObject>* LocObj = nullptr;
    if (!Obj->TryGetObjectField(TEXT("location"), LocObj)) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Missing location."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    double x=0, y=0, z=0;
    (*LocObj)->TryGetNumberField(TEXT("x"), x);
    (*LocObj)->TryGetNumberField(TEXT("y"), y);
    (*LocObj)->TryGetNumberField(TEXT("z"), z);
    FVector Location(x*100.f, y*100.f, z*100.f);
    FLinearColor Color = FLinearColor::Green;
    if (const TSharedPtr<FJsonObject>* ColorObj; Obj->TryGetObjectField(TEXT("color"), ColorObj))
    {
        double r=0,g=1,b=0,a=1;
        (*ColorObj)->TryGetNumberField(TEXT("r"), r);
        (*ColorObj)->TryGetNumberField(TEXT("g"), g);
        (*ColorObj)->TryGetNumberField(TEXT("b"), b);
        (*ColorObj)->TryGetNumberField(TEXT("a"), a);
        Color = FLinearColor(r, g, b, a);
    }
    double scale = 10.0;
    Obj->TryGetNumberField(TEXT("scale"), scale);
    FString Id = DrawPoint(Location, Color, static_cast<float>(scale));
    if (!Id.IsEmpty()) {
        RetObj->SetBoolField(TEXT("ok"), true);
        RetObj->SetStringField(TEXT("id"), Id);
    } else {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("DrawPoint operation failed."));
    }
    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
    return OutStr;
}

FString USvcGeometryDrawerSubsystem::DrawLineJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> RetObj = MakeShareable(new FJsonObject);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Invalid JSON."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    if (!Obj->TryGetArrayField(TEXT("locations"), Points)) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Missing locations."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    TArray<FVector> Positions;
    for (const auto& V : *Points) {
        const TSharedPtr<FJsonObject>* PO = nullptr;
        if (!V->TryGetObject(PO) || !PO) {
            continue;
        }
        double x = 0, y = 0, z = 0;
        (*PO)->TryGetNumberField(TEXT("x"), x);
        (*PO)->TryGetNumberField(TEXT("y"), y);
        (*PO)->TryGetNumberField(TEXT("z"), z);
        Positions.Add(FVector(x*100.f, y*100.f, z*100.f));
    }
    if (Positions.Num() < 2) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("At least two locations required."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    FLinearColor Color = FLinearColor::Red;
    if (const TSharedPtr<FJsonObject>* ColorObj; Obj->TryGetObjectField(TEXT("color"), ColorObj)) {
        double r = 1, g = 0, b = 0, a = 1;
        (*ColorObj)->TryGetNumberField(TEXT("r"), r);
        (*ColorObj)->TryGetNumberField(TEXT("g"), g);
        (*ColorObj)->TryGetNumberField(TEXT("b"), b);
        (*ColorObj)->TryGetNumberField(TEXT("a"), a);
        Color = FLinearColor(r, g, b, a);
    }
    double scale = 2.0;
    Obj->TryGetNumberField(TEXT("scale"), scale);
    FString Id = DrawLine(Positions, Color, static_cast<float>(scale));
    if (!Id.IsEmpty()) {
        RetObj->SetBoolField(TEXT("ok"), true);
        RetObj->SetStringField(TEXT("id"), Id);
    } else {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("DrawLine operation failed."));
    }
    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
    return OutStr;
}

FString USvcGeometryDrawerSubsystem::DrawCubeJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> RetObj = MakeShareable(new FJsonObject);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Invalid JSON."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    const TSharedPtr<FJsonObject>* locationObj = nullptr;
    if (!Obj->TryGetObjectField(TEXT("location"), locationObj)) {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("Missing location."));
        FString OutStr;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
        FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
        return OutStr;
    }
    double cx = 0, cy = 0, cz = 0;
    (*locationObj)->TryGetNumberField(TEXT("x"), cx);
    (*locationObj)->TryGetNumberField(TEXT("y"), cy);
    (*locationObj)->TryGetNumberField(TEXT("z"), cz);
    FVector Center(cx*100.f, cy*100.f, cz*100.f);
    FVector Scale(1.f, 1.f, 1.f);
    if (const TSharedPtr<FJsonObject>* SObj; Obj->TryGetObjectField(TEXT("scale"), SObj)) {
        double sx = 1, sy = 1, sz = 1;
        (*SObj)->TryGetNumberField(TEXT("x"), sx);
        (*SObj)->TryGetNumberField(TEXT("y"), sy);
        (*SObj)->TryGetNumberField(TEXT("z"), sz);
        Scale = FVector(sx, sy, sz);
    }
    FLinearColor Color = FLinearColor::Blue;
    if (const TSharedPtr<FJsonObject>* ColorObj; Obj->TryGetObjectField(TEXT("color"), ColorObj)) {
        double r = 0, g = 0, b = 1, a = 1;
        (*ColorObj)->TryGetNumberField(TEXT("r"), r);
        (*ColorObj)->TryGetNumberField(TEXT("g"), g);
        (*ColorObj)->TryGetNumberField(TEXT("b"), b);
        (*ColorObj)->TryGetNumberField(TEXT("a"), a);
        Color = FLinearColor(r, g, b, a);
    }
    FString Id = DrawCube(Center, Scale, Color);
    if (!Id.IsEmpty()) {
        RetObj->SetBoolField(TEXT("ok"), true);
        RetObj->SetStringField(TEXT("id"), Id);
    } else {
        RetObj->SetBoolField(TEXT("ok"), false);
        RetObj->SetStringField(TEXT("error"), TEXT("DrawCube operation failed."));
    }
    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
    return OutStr;
}

FString USvcGeometryDrawerSubsystem::RemoveJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> RetObj = MakeShareable(new FJsonObject);
    bool ok = false;
    FString error, Id;
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        error = TEXT("Invalid JSON.");
    } else if (!Obj->TryGetStringField(TEXT("id"), Id)) {
        error = TEXT("Missing id.");
    } else {
        ok = RemoveDrawObject(Id);
        if (!ok)
            error = TEXT("Remove operation failed.");
    }
    RetObj->SetBoolField(TEXT("ok"), ok);
    if (!ok)
        RetObj->SetStringField(TEXT("error"), error);
    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
    return OutStr;
}

FString USvcGeometryDrawerSubsystem::ClearJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TSharedPtr<FJsonObject> RetObj = MakeShareable(new FJsonObject);
    bool ok = false;
    FString error, TypeStr(TEXT("all"));
    EGeometryDrawType Type = EGeometryDrawType::All;
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        error = TEXT("Invalid JSON.");
    } else {
        Obj->TryGetStringField(TEXT("type"), TypeStr);
        if (TypeStr.Equals(TEXT("point"), ESearchCase::IgnoreCase))
            Type = EGeometryDrawType::Point;
        else if (TypeStr.Equals(TEXT("line"), ESearchCase::IgnoreCase))
            Type = EGeometryDrawType::Line;
        else if (TypeStr.Equals(TEXT("cube"), ESearchCase::IgnoreCase))
            Type = EGeometryDrawType::Cube;
        ok = ClearDrawObjects(Type);
        if (!ok)
            error = TEXT("Clear operation failed.");
    }
    RetObj->SetBoolField(TEXT("ok"), ok);
    if (!ok)
        RetObj->SetStringField(TEXT("error"), error);
    FString OutStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
    FJsonSerializer::Serialize(RetObj.ToSharedRef(), Writer);
    return OutStr;
}