// MyExporterBPL.cpp

#include "MyExporterBPL.h"

// --- 헤더 파일들 ---
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Editor.h" // GEditor에 접근하기 위해 필요

void UMyExporterBPL::ExportServerData(UObject* WorldContextObject)
{
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    // AActor로 모든 액터를 가져오기
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem)
    {
        FoundActors = EditorActorSubsystem->GetAllLevelActors();
    }

    for (AActor* Actor : FoundActors)
    {
        // 액터를 'UStaticMeshComponent'를 가지고 있는지 확인
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();

        // 스태틱 메시 컴포넌트가 없는 액터는 건너뛴다. (예: 빛, 카메라, 트리거 등).
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());

        // 1. 기본 정보 추가 (액터 이름, 메시 이름)
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        // 2. 월드 위치 정보 추가
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f; // 미터 단위로 변경
        FVector ExportedLocation(UnrealLocation.Y, UnrealLocation.Z, UnrealLocation.X); // Y-up, 왼손 좌표계로 변환
        ActorJsonObject->SetStringField(TEXT("Location"), ExportedLocation.ToString());

        // 3. AABB(Axis-Aligned Bounding Box)를 위한 볼륨 정보 추가
        // 컴포넌트의 월드 바운드를 가져와서 Min/Max 점을 추출
        FBox WorldBounds = MeshComponent->Bounds.GetBox();

        // Min/Max 지점을 미터 단위로 변환
        FVector UnrealMin = WorldBounds.Min / 100.0f;
        FVector UnrealMax = WorldBounds.Max / 100.0f;

        // Y-up, 왼손 좌표계로 변환
        FVector ExportedMin(UnrealMin.Y, UnrealMin.Z, UnrealMin.X);
        FVector ExportedMax(UnrealMax.Y, UnrealMax.Z, UnrealMax.X);

        // AABB Json 객체 생성
        TSharedPtr<FJsonObject> AABBObject = MakeShareable(new FJsonObject());
        AABBObject->SetStringField(TEXT("Min"), ExportedMin.ToString());
        AABBObject->SetStringField(TEXT("Max"), ExportedMax.ToString());
        ActorJsonObject->SetObjectField(TEXT("AABB"), AABBObject);

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // 파일 저장 (Json으로)
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);

    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedServerData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);

    UE_LOG(LogTemp, Warning, TEXT("Server data export complete! File saved to: %s"), *FilePath);
}


void UMyExporterBPL::ExportClientData(UObject* WorldContextObject)
{
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    // AActor로 모든 액터를 가져오기
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem)
    {
        FoundActors = EditorActorSubsystem->GetAllLevelActors();
    }

    for (AActor* Actor : FoundActors)
    {
        // 액터를 'UStaticMeshComponent'를 가지고 있는지 확인
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();

        // 스태틱 메시 컴포넌트가 없는 액터는 건너뛴다. (예: 빛, 카메라, 트리거 등).
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());

        // 기본 정보 추가 (액터 이름, 메시 이름)
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        // 원본 트랜스폼 데이터 가져오기 (언리얼 Z-up, 왼손 좌표계)
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f; // 미터 단위로 변경
        FQuat UnrealQuat = MeshComponent->GetComponentQuat();
        FVector UnrealScale = MeshComponent->GetComponentScale();

        // Y-up, 왼손 좌표계로 변환
        FVector ExportedLocation(UnrealLocation.Y, UnrealLocation.Z, UnrealLocation.X);
        FQuat ExportedQuat(UnrealQuat.Y, UnrealQuat.Z, UnrealQuat.X, UnrealQuat.W);
        FVector ExportedScale(UnrealScale.Y, UnrealScale.Z, UnrealScale.X);

        // 트랜스폼 정보는 액터가 아닌, 메시 컴포넌트의 것을 가져옴
        TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject());
        TransformObject->SetStringField(TEXT("Location"), ExportedLocation.ToString());
        TransformObject->SetStringField(TEXT("Rotation"), ExportedQuat.Rotator().ToString());
        TransformObject->SetStringField(TEXT("Scale"), ExportedScale.ToString());
        ActorJsonObject->SetObjectField(TEXT("Transform"), TransformObject);

        // 콜리전 정보는 메시 컴포넌트의 스태틱 메시에서 추출 (C++에서만 가능)
        //UBodySetup* BodySetup = MeshComponent->GetStaticMesh()->GetBodySetup();
        //if (BodySetup)
        //{
        //    TArray<TSharedPtr<FJsonValue>> CollisionVerticesArray;
        //    for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
        //    {
        //        for (const FVector& Vertex : ConvexElem.VertexData)
        //        {
        //            FVector VertexInMeters = Vertex / 100.0f; // 미터 단위로 변경
        //      
        //            FVector ExportedVertex(VertexInMeters.Y, VertexInMeters.Z, VertexInMeters.X);
        //            CollisionVerticesArray.Add(MakeShareable(new FJsonValueString(ExportedVertex.ToString())));
        //        }
        //    }
        //    ActorJsonObject->SetArrayField(TEXT("CollisionVertices"), CollisionVerticesArray);
        //}

        // --- 텍스처 정보 추출 로직 (여기에 추가) ---
        TArray<TSharedPtr<FJsonValue>> TexturesJsonArray;
        int32 NumMaterials = MeshComponent->GetNumMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
        {
            UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
            if (Material)
            {
                TArray<UTexture*> UsedTextures;
                Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);

                for (UTexture* Texture : UsedTextures)
                {
                    if (Texture)
                    {
                        TexturesJsonArray.Add(MakeShareable(new FJsonValueString(Texture->GetPathName())));
                    }
                }
            }
        }

        ActorJsonObject->SetArrayField(TEXT("Textures"), TexturesJsonArray);
        // --- 텍스처 정보 추출 끝 ---

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // 파일 저장 (Json으로)
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);

    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedClientData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);

    UE_LOG(LogTemp, Warning, TEXT("Export Complete! File saved to: %s"), *FilePath);
}