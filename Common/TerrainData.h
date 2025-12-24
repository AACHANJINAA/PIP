#pragma once

#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iostream> // For std::cout in inline functions

// Common/Vector3.h가 Common 프로젝트에 있다고 가정합니다.
#include "Vector3.h"

// nlohmann/json.hpp가 Common 프로젝트의 include path에 있거나 각 프로젝트에 있어야 합니다.
// 여기서는 Common 프로젝트에서 찾을 수 있다고 가정합니다.
#include "json.hpp" 
#define NOMINMAX
namespace common
{
    struct TerrainInfo
    {
        float min_x, max_x;
        float min_z, max_z;
        float width, height; // Grid Count (e.g., 63x63)
        float height_scale;
        float min_height;
    };

    class TerrainData
    {
    public:
        TerrainData() = default;
        ~TerrainData() = default;

        // 구현부를 헤더에 inline으로 작성하여 Header-Only 클래스로 만듭니다.
        inline bool LoadFromJSON(const std::string& json_path)
        {
            std::ifstream file(json_path);
            if (!file.is_open())
            {
                std::cout << "[TerrainData] Failed to open JSON: " << json_path << std::endl;
                return false;
            }

            nlohmann::json config;
            try { file >> config; }
            catch (const std::exception& e) {
                std::cout << "[TerrainData] JSON Parse Error: " << e.what() << std::endl;
                return false;
            }
            file.close();

            // 필수 필드 확인 (width, height는 파일에서 직접 계산하므로 필수 체크에서 제외해도 되지만, 일단 둡니다)
            if (!config.contains("bounds") || !config["bounds"].contains("min_x") || !config["bounds"].contains("max_x") ||
                !config["bounds"].contains("min_z") || !config["bounds"].contains("max_z") ||
                !config.contains("scale") || !config["scale"].contains("y") || !config.contains("heightmap_file"))
            {
                std::cout << "[TerrainData] JSON missing required fields in " << json_path << std::endl;
                return false;
            }

            _info.min_x = config["bounds"]["min_x"].get<float>();
            _info.max_x = config["bounds"]["max_x"].get<float>();
            _info.min_z = config["bounds"]["min_z"].get<float>();
            _info.max_z = config["bounds"]["max_z"].get<float>();

            // width/height는 JSON에서 읽지 않고(혹은 읽더라도), 아래에서 파일 크기로 덮어씁니다.
            _info.height_scale = config["scale"]["y"].get<float>();

            std::string raw_filename = config["heightmap_file"].get<std::string>();

            std::filesystem::path path_obj(json_path);
            std::filesystem::path dir = path_obj.parent_path();
            std::filesystem::path heightmap_full_path = dir / raw_filename;
            _heightMapPath = heightmap_full_path.string();

            // ★ [수정 1] 파일 끝(ate)으로 열어서 크기 확인
            std::ifstream hm_file(_heightMapPath, std::ios::binary | std::ios::ate);
            if (!hm_file.is_open())
            {
                std::cout << "[TerrainData] Failed to open heightmap raw file: " << _heightMapPath << std::endl;
                return false;
            }

            // 파일 크기 가져오기
            std::streamsize fileSize = hm_file.tellg();
            hm_file.seekg(0, std::ios::beg); // 다시 파일 처음으로 이동!

            // ★ [수정 2] 해상도 역추적 (파일크기 / 2bytes = 픽셀수 -> 제곱근 = 해상도)
            size_t total_bytes = static_cast<size_t>(fileSize);
            size_t total_pixels_from_file = total_bytes / 2;
            size_t calculated_resolution = static_cast<size_t>(std::sqrt(total_pixels_from_file));

            // 검증: 정사각형이 맞는지 확인
            if (calculated_resolution * calculated_resolution != total_pixels_from_file)
            {
                std::cout << "[TerrainData] Error: Heightmap file is not square or corrupted. Size: " << total_bytes << " bytes." << std::endl;
                return false;
            }

            // _info 정보 갱신 (JSON 값 무시하고 실제 파일 규격 사용)
            _info.width = static_cast<float>(calculated_resolution);
            _info.height = static_cast<float>(calculated_resolution);

            //std::cout << "[TerrainData] Auto-detected Resolution: " << _info.width << " x " << _info.height << std::endl;

            // 벡터 리사이즈
            _heights.resize(total_pixels_from_file);

            float min_h = 1.0f;
            uint16_t min_raw_val = 65535; // R16 파일의 최소값 추적

            // R16 읽기
            if (!hm_file.read(reinterpret_cast<char*>(_heights.data()), total_bytes)) // 벡터에 직접 읽기 (속도 최적화 가능)
            {
                // 직접 읽기가 어렵다면 기존 루프 방식 유지 (아래 코드는 기존 방식 유지)
            }
            // 위 read가 벡터 타입 문제로 복잡할 수 있으니 안전하게 기존 루프 방식 사용:
            hm_file.seekg(0, std::ios::beg); // 다시 처음으로 (혹시 모르니)

            for (size_t i = 0; i < total_pixels_from_file; ++i)
            {
                uint16_t raw_val;
                if (!hm_file.read(reinterpret_cast<char*>(&raw_val), sizeof(uint16_t)))
                {
                    break;
                }
                min_raw_val = (std::min)(min_raw_val, raw_val);

                // 0~1 정규화
                float norm = static_cast<float>(raw_val) / 65535.0f;
                _heights[i] = norm;

                // 최소 높이값 추적 (0.0 ~ 1.0 사이 값)
                if (norm < min_h) min_h = norm;
            }
            hm_file.close();

            // 정보용 min_height 저장
            _info.min_height = (static_cast<float>(min_raw_val) / 65535.0f) * _info.height_scale;

            // [수정 3] "가장 낮은 곳을 0으로 맞추기" (바닥 보정)
            for (size_t i = 0; i < total_pixels_from_file; ++i)
            {
                // (현재높이 - 최소높이) * 스케일
                // 결과: 지형의 가장 낮은 지점은 0.0이 되고, 나머지는 그 위로 쌓임
                _heights[i] = (_heights[i] - min_h) * _info.height_scale;
            }

            //std::cout << "[TerrainData] Loaded successfully from " << json_path << std::endl;
            return true;
        }

        inline float GetHeightAt(float x, float z) const
        {
            // 1. 기본 범위 체크

            if (x < _info.min_x || x > _info.max_x || z < _info.min_z || z > _info.max_z)
            {
                /*std::cout << "[Terrain] Out of Bounds! Input(" << x << ", " << z << ") "
                    << "Range X[" << _info.min_x << "~" << _info.max_x << "] "
                    << "Z[" << _info.min_z << "~" << _info.max_z << "]" << std::endl;*/
                return 0.0f;
            }

            if (_heights.empty())
                return 0.0f; // 높이 데이터가 없으면 0 반환

            // 2. 월드 좌표를 0~1 사이의 정규화된 좌표로 변환
            float width_world = _info.max_x - _info.min_x;
            float height_world = _info.max_z - _info.min_z;

            // 0으로 나누기 방지
            if (width_world <= 0.0f || height_world <= 0.0f)
                return _heights[0]; // 유효하지 않은 범위일 경우 첫 높이 반환 또는 0

            float norm_x = (x - _info.min_x) / width_world;
            float norm_z = (z - _info.min_z) / height_world;

            // 3. 정규화된 좌표 클램핑 (0.0 ~ 1.0 강제)
            norm_x = (std::max)(0.0f, (std::min)(norm_x, 1.0f));
            norm_z = (std::max)(0.0f, (std::min)(norm_z, 1.0f));

            // 4. 그리드 인덱스 및 소수부 계산
            int grid_w = static_cast<int>(_info.width);
            int grid_h = static_cast<int>(_info.height);

            // (width-1) 또는 (height-1)을 곱해야 인덱스가 올바른 범위에 들어감
            float f_idx_x = norm_x * (grid_w - 1);
            float f_idx_z = norm_z * (grid_h - 1);

            int x0 = static_cast<int>(f_idx_x);
            int z0 = static_cast<int>(f_idx_z);
            
            // 5. 배열 인덱스 안전 장치 (경계값 처리)
            // x0, z0가 마지막 그리드 셀의 시작 인덱스가 되도록 제한
            if (x0 >= grid_w - 1) x0 = grid_w - 2;
            if (z0 >= grid_h - 1) z0 = grid_h - 2;
			x0 = (std::max)(x0, 0);
			z0 = (std::max)(z0, 0);

			int x1 = x0 + 1;
            int z1 = z0 + 1;

            float fx = f_idx_x - x0; // x 방향 소수부 (0.0 ~ 1.0)
            float fz = f_idx_z - z0; // z 방향 소수부 (0.0 ~ 1.0)

            // 6. 높이값 4개 조회
            // 1차원 배열 접근: _heights[row_index * width + col_index]
            float h00 = _heights[z0 * grid_w + x0];         // 좌하단 (z0, x0)
            float h10 = _heights[z0 * grid_w + x1];         // 우하단 (z0, x1)
            float h01 = _heights[z1 * grid_w + x0];         // 좌상단 (z1, x0)
            float h11 = _heights[z1 * grid_w + x1];         // 우상단 (z1, x1)

            // 양선형 보간 (Bilinear Interpolation)
            float h0 = h00 * (1.0f - fx) + h10 * fx; // 하단 보간
            float h1 = h01 * (1.0f - fx) + h11 * fx; // 상단 보간
            return h0 * (1.0f - fz) + h1 * fz;      // 최종 보간
        }
        inline bool IsInsideMap(float x, float z) const
        {
            return (x >= _info.min_x && x <= _info.max_x &&
                    z >= _info.min_z && z <= _info.max_z);
		}
        const TerrainInfo& GetInfo() const { return _info; }
        const std::vector<float>& GetHeightData() const { return _heights; }
        const std::string& GetHeightMapPath() const { return _heightMapPath; }

    private:
        TerrainInfo _info{};
        std::vector<float> _heights;
        std::string _heightMapPath;
    };
}