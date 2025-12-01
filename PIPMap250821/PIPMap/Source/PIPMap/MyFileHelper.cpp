#include "MyFileHelper.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

bool UMyFileHelper::SaveStringToFile(FString SaveDirectory, FString FileName, FString SaveText, bool AllowOverwriting)
{
    // 경로 합치기
    FString FinalPath = SaveDirectory + "/" + FileName;

    // 파일 쓰기 (성공하면 true 리턴)
    return FFileHelper::SaveStringToFile(SaveText, *FinalPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}