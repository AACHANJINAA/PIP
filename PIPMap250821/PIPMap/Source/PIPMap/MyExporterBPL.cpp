#include "MyExporterBPL.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Exporters/Exporter.h"
#include "HAL/PlatformFileManager.h"
#include "Exporters/GLTFExporter.h"
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

        if (!ExportedMeshes.Contains(StaticMesh))
        {
            FString MeshFileName = StaticMesh->GetName() + TEXT(".gltf");
            FString MeshFullFilePath = MeshExportDir + MeshFileName;
            UExporter* Exporter = UExporter::FindExporter(StaticMesh, TEXT("gltf"));
            if (Exporter)
            {
                if (UExporter::ExportToFile(StaticMesh, Exporter, *MeshFullFilePath, false, false) == 1)
                {
                    UE_LOG(LogTemp, Log, TEXT("Exported Mesh: %s"), *MeshFullFilePath);
                    ExportedMeshes.Add(StaticMesh);
                }
            }
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        FString RelativeMeshPath = TEXT("Meshes/") + StaticMesh->GetName() + TEXT(".gltf");
        ActorJsonObject->SetStringField(TEXT("MeshFile"), RelativeMeshPath);

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

        // --- 텍스처 정보 저장 (최종 수정: 머티리얼 슬롯 지원) ---
// 1. 최종 MaterialOverrides 배열을 담을 JsonArray를 선언합니다.
        TArray<TSharedPtr<FJsonValue>> MaterialOverridesArray;

        int32 NumMaterials = MeshComponent->GetNumMaterials();
        // 2. 머티리얼 '슬롯' 개수만큼 루프를 돕니다. (0번, 1번, 2번...)
        for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
        {
            // 각 슬롯별로 텍스처 정보를 담을 TMap을 새로 생성합니다.
            TMap<FString, FString> CurrentSlotTexturesMap;
            UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);

            if (Material)
            {
                TArray<UTexture*> UsedTextures;
                Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);

                for (UTexture* Texture : UsedTextures)
                {
                    UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
                    if (Texture2D)
                    {
                        FString TextureName = Texture2D->GetName();
                        FString RelativeDdsPath = TEXT("Textures/") + TextureName + TEXT(".dds");

                        // 텍스처 이름 분석 후 역할에 맞는 Key와 함께 Map에 저장
                        if (TextureName.EndsWith(TEXT("_N"), ESearchCase::IgnoreCase)) {
                            CurrentSlotTexturesMap.Add(TEXT("normalTexture"), RelativeDdsPath);
                        }
                        else if (TextureName.EndsWith(TEXT("_ORM"), ESearchCase::IgnoreCase) || TextureName.EndsWith(TEXT("_MRA"), ESearchCase::IgnoreCase)) {
                            CurrentSlotTexturesMap.Add(TEXT("ormTexture"), RelativeDdsPath);
                        }
                        else if (TextureName.EndsWith(TEXT("_E"), ESearchCase::IgnoreCase) || TextureName.EndsWith(TEXT("_Emissive"), ESearchCase::IgnoreCase)) {
                            CurrentSlotTexturesMap.Add(TEXT("emissiveTexture"), RelativeDdsPath);
                        }
                        else {
                            CurrentSlotTexturesMap.Add(TEXT("baseColorTexture"), RelativeDdsPath);
                        }

                        // PNG 파일 익스포트 로직은 기존과 동일
                        if (!ExportedTextures.Contains(Texture2D))
                        {
                            FString TextureFileName = TextureName + TEXT(".png");
                            FString TextureFullFilePath = TextureExportDir + TextureFileName;
                            UExporter* Exporter = UExporter::FindExporter(Texture2D, TEXT("PNG"));
                            if (Exporter) {
                                if (UExporter::ExportToFile(Texture2D, Exporter, *TextureFullFilePath, false, false) == 1) {
                                    UE_LOG(LogTemp, Log, TEXT("Exported New Source Texture: %s"), *TextureFullFilePath);
                                }
                            }
                            ExportedTextures.Add(Texture2D);
                        }
                    }
                }
            }

            // 3. 현재 슬롯의 텍스처 맵(TMap)을 JsonObject로 변환합니다.
            TSharedPtr<FJsonObject> SlotMaterialObject = MakeShareable(new FJsonObject());
            for (const TPair<FString, FString>& Pair : CurrentSlotTexturesMap)
            {
                SlotMaterialObject->SetStringField(Pair.Key, Pair.Value);
            }

            // 4. 변환된 JsonObject를 최종 배열에 추가합니다.
            MaterialOverridesArray.Add(MakeShareable(new FJsonValueObject(SlotMaterialObject)));
        }

        // 5. 완성된 배열을 ActorJsonObject에 추가합니다.
        ActorJsonObject->SetArrayField(TEXT("MaterialOverrides"), MaterialOverridesArray);
        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = BaseExportDir + TEXT("ExportedClientData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Export Complete! File saved to: %s"), *FilePath);
#endif
}