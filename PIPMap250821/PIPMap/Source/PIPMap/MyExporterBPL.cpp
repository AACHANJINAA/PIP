#include "MyExporterBPL.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "Exporters/Exporter.h"
#include "Exporters/GLTFExporter.h"
#include "Options/GLTFExportOptions.h"
#include "AssetExportTask.h"
#endif

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
// FVector 헬퍼 함수
TSharedPtr<FJsonObject> VectorToJsonObject(const FVector& InVector)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("X"), InVector.X);
    JsonObject->SetNumberField(TEXT("Y"), InVector.Y);
    JsonObject->SetNumberField(TEXT("Z"), InVector.Z);
    return JsonObject;
}

// FRotator 헬퍼 함수
TSharedPtr<FJsonObject> RotatorToJsonObject(const FRotator& InRotator)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("Pitch"), InRotator.Pitch);
    JsonObject->SetNumberField(TEXT("Yaw"), InRotator.Yaw);
    JsonObject->SetNumberField(TEXT("Roll"), InRotator.Roll);
    return JsonObject;
}

// ExportServerData 함수 (기존과 동일)
void UMyExporterBPL::ExportServerData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem) FoundActors = EditorActorSubsystem->GetAllLevelActors();
    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComponent || !MeshComponent->GetStaticMesh()) continue;
        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Z, UnrealLocation.Y);
        ActorJsonObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));

        FBox WorldBounds = MeshComponent->Bounds.GetBox();
        FVector UnrealMin = WorldBounds.Min / 100.0f;
        FVector UnrealMax = WorldBounds.Max / 100.0f;
        FVector ExportedMin(UnrealMin.X, UnrealMin.Z, UnrealMin.Y);
        FVector ExportedMax(UnrealMax.X, UnrealMax.Z, UnrealMax.Y);

        TSharedPtr<FJsonObject> AABBObject = MakeShareable(new FJsonObject());
        AABBObject->SetObjectField(TEXT("Min"), VectorToJsonObject(ExportedMin));
        AABBObject->SetObjectField(TEXT("Max"), VectorToJsonObject(ExportedMax));
        ActorJsonObject->SetObjectField(TEXT("AABB"), AABBObject);
        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedServerData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Server data export complete! File saved to: %s"), *FilePath);
#endif
}

// Quaternion을 { "X": 값, "Y": 값, "Z": 값, "W": 값 } 형태의 FJsonObject로 변환하는 헬퍼 함수
TSharedPtr<FJsonObject> QuaternionToJsonObject(const FQuat& InQuat)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("X"), InQuat.X);
    JsonObject->SetNumberField(TEXT("Y"), InQuat.Y);
    JsonObject->SetNumberField(TEXT("Z"), InQuat.Z);
    JsonObject->SetNumberField(TEXT("W"), InQuat.W);
    return JsonObject;
}

void UMyExporterBPL::ExportClientData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    const FString BaseExportDir = FPaths::ProjectSavedDir() + TEXT("MapData/");
    const FString MeshExportDir = BaseExportDir + TEXT("Meshes/");

    // --- 폴더 생성 로직 (Meshes만) ---
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*MeshExportDir)) PlatformFile.CreateDirectoryTree(*MeshExportDir);
    // Textures 폴더 생성 로직도 제거

    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem) FoundActors = EditorActorSubsystem->GetAllLevelActors();

    TSet<UStaticMesh*> ExportedMeshes;

    // --- [1. 전역 옵션 설정] ---
    UGLTFExportOptions* Options = GetMutableDefault<UGLTFExportOptions>();
    const EGLTFTextureImageFormat OldTextureFormat = Options->TextureImageFormat;
    const bool bOldAdjustNormalmaps = Options->bAdjustNormalmaps;
    const bool bOldExportUnlitMaterials = Options->bExportUnlitMaterials;

    // .png 파일이 .gltf와 같은 폴더(Meshes/)에 생성되도록 설정
    Options->TextureImageFormat = EGLTFTextureImageFormat::PNG;
    Options->bAdjustNormalmaps = true;
    Options->bExportUnlitMaterials = true;

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;

        if (!StaticMesh) continue;

        // --- [2. 자동화된 익스포트] (파일이 Meshes/ 폴더에 모두 생성됨) ---
        if (!ExportedMeshes.Contains(StaticMesh))
        {
            FString MeshFileName = StaticMesh->GetName() + TEXT(".gltf");
            FString MeshFullFilePath = MeshExportDir + MeshFileName;

            UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
            ExportTask->Object = StaticMesh;
            ExportTask->Exporter = UExporter::FindExporter(StaticMesh, TEXT("gltf"));
            ExportTask->Filename = MeshFullFilePath;
            ExportTask->bSelected = false;
            ExportTask->bReplaceIdentical = true;
            ExportTask->bPrompt = false;
            ExportTask->bAutomated = true;
            ExportTask->bUseFileArchive = false;
            ExportTask->bWriteEmptyFiles = false;

            if (UExporter::RunAssetExportTask(ExportTask))
            {
                UE_LOG(LogTemp, Log, TEXT("Exported asset to: %s"), *MeshFullFilePath);
                ExportedMeshes.Add(StaticMesh);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Automated Export FAILED for: %s"), *MeshFullFilePath);
            }
        }

        // --- [3. 씬(MapData) JSON 구성] ---
        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());

        // ★★★ 원본 상태 유지: 씬 파일은 Meshes/의 gltf를 가리킵니다. ★★★
        FString RelativeMeshPath = TEXT("Meshes/") + StaticMesh->GetName() + TEXT(".gltf");
        ActorJsonObject->SetStringField(TEXT("MeshFile"), RelativeMeshPath);

        // (트랜스폼 정보 ... 동일)
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FQuat UnrealQuat = MeshComponent->GetComponentQuat();
        FVector UnrealScale = MeshComponent->GetComponentScale();
        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Z, UnrealLocation.Y);
        FVector ExportedScale(UnrealScale.X, UnrealScale.Z, UnrealScale.Y);
        FQuat ExportedQuat = FQuat(UnrealQuat.X, UnrealQuat.Z, UnrealQuat.Y, -UnrealQuat.W);

        TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject());
        TransformObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));
        TransformObject->SetObjectField(TEXT("Rotation"), QuaternionToJsonObject(ExportedQuat));
        TransformObject->SetObjectField(TEXT("Scale"), VectorToJsonObject(ExportedScale));
        ActorJsonObject->SetObjectField(TEXT("Transform"), TransformObject);

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // --- [4. 전역 옵션 복원] ---
    Options->TextureImageFormat = OldTextureFormat;
    Options->bAdjustNormalmaps = bOldAdjustNormalmaps;
    Options->bExportUnlitMaterials = bOldExportUnlitMaterials;

    // --- [6. 최종 씬(MapData) JSON 저장] ---
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = BaseExportDir + TEXT("ExportedClientData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);

    UE_LOG(LogTemp, Warning, TEXT("--- Base Export Complete! ---"));
    UE_LOG(LogTemp, Warning, TEXT("Scene file saved to: %s"), *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Meshes/Textures saved to: %s"), *MeshExportDir);
    UE_LOG(LogTemp, Warning, TEXT("Textures are embedded/referenced relative to the gltf file in %s."), *MeshExportDir);
#endif
}
