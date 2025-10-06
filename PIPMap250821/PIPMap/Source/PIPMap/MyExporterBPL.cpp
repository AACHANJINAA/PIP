#include "MyExporterBPL.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Exporters/Exporter.h"         // CJ설명 : UExporter 사용을 위해 추가
#include "HAL/PlatformFileManager.h"    // CJ설명 : 파일/디렉토리 관리를 위해 추가
#include "GLTFExporterModule.h" // GLTFExporter 관련 헤더 추가
#include "Exporters/GLTFExporter.h"
#endif

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h" // UTexture2D로 캐스팅하기 위해 추가
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"


// FVector를 { "X": 값, "Y": 값, "Z": 값 } 형태의 FJsonObject로 변환하는 헬퍼 함수
TSharedPtr<FJsonObject> VectorToJsonObject(const FVector& InVector)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("X"), InVector.X);
    JsonObject->SetNumberField(TEXT("Y"), InVector.Y);
    JsonObject->SetNumberField(TEXT("Z"), InVector.Z);
    return JsonObject;
}

// FRotator를 { "Pitch": 값, "Yaw": 값, "Roll": 값 } 형태의 FJsonObject로 변환하는 헬퍼 함수
TSharedPtr<FJsonObject> RotatorToJsonObject(const FRotator& InRotator)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("Pitch"), InRotator.Pitch);
    JsonObject->SetNumberField(TEXT("Yaw"), InRotator.Yaw);
    JsonObject->SetNumberField(TEXT("Roll"), InRotator.Roll);
    return JsonObject;
}

void UMyExporterBPL::ExportServerData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem)
    {
        FoundActors = EditorActorSubsystem->GetAllLevelActors();
    }

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        // 위치 정보 변환 및 저장
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Y, UnrealLocation.Z);
        ActorJsonObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));

        // AABB 정보 변환 및 저장
        FBox WorldBounds = MeshComponent->Bounds.GetBox();
        FVector UnrealMin = WorldBounds.Min / 100.0f;
        FVector UnrealMax = WorldBounds.Max / 100.0f;
        FVector ExportedMin(UnrealMin.X, UnrealMin.Y, UnrealMin.Z);
        FVector ExportedMax(UnrealMax.X, UnrealMax.Y, UnrealMax.Z);

        TSharedPtr<FJsonObject> AABBObject = MakeShareable(new FJsonObject());
        AABBObject->SetObjectField(TEXT("Min"), VectorToJsonObject(ExportedMin));
        AABBObject->SetObjectField(TEXT("Max"), VectorToJsonObject(ExportedMax));
        ActorJsonObject->SetObjectField(TEXT("AABB"), AABBObject);

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // 파일 저장
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedServerData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Server data export complete! File saved to: %s"), *FilePath);
#else
    UE_LOG(LogTemp, Error, TEXT("ExportServerData can only be called from the editor."));
#endif
}

void UMyExporterBPL::ExportClientData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    const FString BaseExportDir = FPaths::ProjectSavedDir() + TEXT("MapData/");
    const FString TextureExportDir = BaseExportDir + TEXT("Textures/");
    const FString MeshExportDir = BaseExportDir + TEXT("Meshes/");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TextureExportDir)) PlatformFile.CreateDirectoryTree(*TextureExportDir);
    if (!PlatformFile.DirectoryExists(*MeshExportDir)) PlatformFile.CreateDirectoryTree(*MeshExportDir);

    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem) FoundActors = EditorActorSubsystem->GetAllLevelActors();

    TSet<UStaticMesh*> ExportedMeshes;
    TSet<UTexture*> ExportedTextures;

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;

        if (!StaticMesh) continue;

        // --- 메시 익스포트 로직 (간단 버전) ---
        if (!ExportedMeshes.Contains(StaticMesh))
        {
            FString MeshFileName = StaticMesh->GetName() + TEXT(".gltf");
            FString MeshFullFilePath = MeshExportDir + MeshFileName;

            // UExporter::FindExporter를 통해 gltf 익스포터를 찾고, 옵션 없이 바로 익스포트
            // glTF 파일 내부에 텍스처 정보가 포함될 수 있지만, 파이프라인에서 무시하므로 문제 없음
            UExporter* Exporter = UExporter::FindExporter(StaticMesh, TEXT("gltf"));
            if (Exporter)
            {
                int32 ExportResult = UExporter::ExportToFile(StaticMesh, Exporter, *MeshFullFilePath, false, false);
                if (ExportResult == 1)
                {
                    UE_LOG(LogTemp, Log, TEXT("Exported Mesh: %s"), *MeshFullFilePath);
                    ExportedMeshes.Add(StaticMesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to export mesh: %s"), *StaticMesh->GetName());
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("GLTF Exporter not found for mesh: %s"), *StaticMesh->GetName());
            }
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());

        FString RelativeMeshPath = TEXT("Meshes/") + StaticMesh->GetName() + TEXT(".gltf");
        ActorJsonObject->SetStringField(TEXT("MeshFile"), RelativeMeshPath);


        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FQuat UnrealQuat = MeshComponent->GetComponentQuat();
        FVector UnrealScale = MeshComponent->GetComponentScale();

        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Y, UnrealLocation.Z);

        TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject());
        TransformObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));
        TransformObject->SetObjectField(TEXT("Rotation"), RotatorToJsonObject(UnrealQuat.Rotator()));
        TransformObject->SetObjectField(TEXT("Scale"), VectorToJsonObject(UnrealScale));
        ActorJsonObject->SetObjectField(TEXT("Transform"), TransformObject);

        // --- 텍스처 정보 저장 로직 (PNG로 추출) ---
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
                    UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
                    if (Texture2D && !ExportedTextures.Contains(Texture2D))
                    {
                        FString TextureFileName = Texture2D->GetName() + TEXT(".png");
                        FString TextureFullFilePath = TextureExportDir + TextureFileName;
                        UExporter* Exporter = UExporter::FindExporter(Texture2D, TEXT("PNG"));
                        if (Exporter)
                        {
                            int32 ExportResult = UExporter::ExportToFile(Texture2D, Exporter, *TextureFullFilePath, false, false);
                            if (ExportResult == 1)
                            {
                                FString RelativeDdsPath = TEXT("Textures/") + Texture2D->GetName() + TEXT(".dds");
                                TexturesJsonArray.Add(MakeShareable(new FJsonValueString(RelativeDdsPath)));
                                UE_LOG(LogTemp, Log, TEXT("Exported Source Texture: %s"), *TextureFullFilePath);
                            }
                        }
                        ExportedTextures.Add(Texture2D);
                    }
                }
            }
        }
        ActorJsonObject->SetArrayField(TEXT("Textures"), TexturesJsonArray);
        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // --- 파일 저장 로직 ---
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = BaseExportDir + TEXT("ExportedClientData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Export Complete! File saved to: %s"), *FilePath);
#endif
}