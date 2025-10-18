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

        TMap<FString, FString> AllMaterialTexturesMap;
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
                    if (Texture2D)
                    {
                        FString TextureName = Texture2D->GetName();
                        FString RelativeDdsPath = TEXT("Textures/") + TextureName + TEXT(".dds");

                        if (TextureName.EndsWith(TEXT("_N"), ESearchCase::IgnoreCase)) {
                            AllMaterialTexturesMap.Add(TEXT("normalTexture"), RelativeDdsPath);
                        }
                        else if (TextureName.EndsWith(TEXT("_ORM"), ESearchCase::IgnoreCase) || TextureName.EndsWith(TEXT("_MRA"), ESearchCase::IgnoreCase)) {
                            AllMaterialTexturesMap.Add(TEXT("ormTexture"), RelativeDdsPath);
                        }
                        else if (TextureName.EndsWith(TEXT("_E"), ESearchCase::IgnoreCase) || TextureName.EndsWith(TEXT("_Emissive"), ESearchCase::IgnoreCase)) {
                            AllMaterialTexturesMap.Add(TEXT("emissiveTexture"), RelativeDdsPath);
                        }
                        else {
                            AllMaterialTexturesMap.Add(TEXT("baseColorTexture"), RelativeDdsPath);
                        }

                        if (!ExportedTextures.Contains(Texture2D))
                        {
                            FString TextureFileName = TextureName + TEXT(".png");
                            FString TextureFullFilePath = TextureExportDir + TextureFileName;
                            UExporter* PngExporter = UExporter::FindExporter(Texture2D, TEXT("PNG"));
                            if (PngExporter) {
                                if (UExporter::ExportToFile(Texture2D, PngExporter, *TextureFullFilePath, false, false) == 1) {
                                    UE_LOG(LogTemp, Log, TEXT("Exported New Source Texture: %s"), *TextureFullFilePath);
                                }
                            }
                            ExportedTextures.Add(Texture2D);
                        }
                    }
                }
            }
        }

        if (AllMaterialTexturesMap.Num() > 0)
        {
            TSharedPtr<FJsonObject> MaterialOverridesObject = MakeShareable(new FJsonObject());
            for (const TPair<FString, FString>& Pair : AllMaterialTexturesMap)
            {
                MaterialOverridesObject->SetStringField(Pair.Key, Pair.Value);
            }
            ActorJsonObject->SetObjectField(TEXT("MaterialOverrides"), MaterialOverridesObject);
        }

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