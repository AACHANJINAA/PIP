#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <DirectXTex.h>

using namespace DirectX;

int main()
{
    size_t width = 512;
    size_t height = 512;
    size_t mipLevels = 9;
    // PBR 환경맵에 가장 표준적인 포맷
    DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    ScratchImage cubemap;
    HRESULT hr = cubemap.InitializeCube(format, width, height, 1, mipLevels);
    if (FAILED(hr)) {
        std::wcerr << L"❌ 큐브맵 메모리 할당 실패!" << std::endl;
        return -1;
    }

    std::wstring faceSuffix[6] = { L"px", L"nx", L"py", L"ny", L"pz", L"nz" };
    std::wcout << L"🚀 블랙아웃 방지 모드로 조립을 시작합니다..." << std::endl;

    for (size_t mip = 0; mip < mipLevels; ++mip) {
        for (size_t face = 0; face < 6; ++face) {

            std::wstring fileName = L"m" + std::to_wstring(mip) + L"_" + faceSuffix[face] + L".dds";

            ScratchImage loadedImage;
            // 로드 시 포맷 변환을 방지하기 위해 DDS_FLAGS_NONE 사용
            hr = LoadFromDDSFile(fileName.c_str(), DDS_FLAGS_NONE, nullptr, loadedImage);

            if (FAILED(hr)) {
                std::wcerr << L"⚠️ 로드 실패: " << fileName << std::endl;
                continue;
            }

            // ★ 중요: 로드한 이미지가 우리 큐브맵 포맷과 다르면 강제로 변환
            ScratchImage convertedImage;
            if (loadedImage.GetMetadata().format != format) {
                hr = Convert(*loadedImage.GetImage(0, 0, 0), format, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, convertedImage);
                if (FAILED(hr)) {
                    std::wcerr << L"❌ 포맷 변환 실패: " << fileName << std::endl;
                    continue;
                }
            }
            else {
                hr = convertedImage.InitializeFromImage(*loadedImage.GetImage(0, 0, 0));
            }

            // ★ 중요: 알파 채널이 0이라서 검게 보일 수 있으므로 강제로 1.0(불투명) 세팅
            // HDR 데이터가 0~1 범위를 넘어가도 투명도 때문에 안 보일 수 있음을 방지합니다.
            const Image* srcImg = convertedImage.GetImage(0, 0, 0);
            const Image* destImg = cubemap.GetImage(mip, face, 0);

            if (srcImg && destImg) {
                Rect srcRect(0, 0, srcImg->width, srcImg->height);
                // CopyRectangle 사용 시 알파 채널 무시 옵션 등을 고려하여 그대로 복사
                hr = CopyRectangle(*srcImg, srcRect, *destImg, TEX_FILTER_DEFAULT, 0, 0);

                if (FAILED(hr)) {
                    std::wcerr << L"❌ 바느질 실패: " << fileName << std::endl;
                }
                else {
                    std::wcout << L"✅ 조립 완료: " << fileName << std::endl;
                }
            }
        }
    }

    std::wstring outputDDS = L"final_cubemap.dds";
    // 저장 시 포맷이 깨지지 않도록 메타데이터를 명확히 전달
    hr = SaveToDDSFile(cubemap.GetImages(), cubemap.GetImageCount(), cubemap.GetMetadata(), DDS_FLAGS_NONE, outputDDS.c_str());

    if (SUCCEEDED(hr)) {
        std::wcout << L"🎉 굽기 성공! 결과물: " << outputDDS << std::endl;
        // 삭제 로직은 데이터 확인 전까지 잠시 주석 처리하거나 조심히 실행하세요!
    }
    else {
        std::wcerr << L"❌ 저장 실패!" << std::endl;
    }

    system("pause");
    return 0;
}