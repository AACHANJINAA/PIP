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
        inline bool LoadFromJSON(const std::string& json_path, bool apply_floor_offset = true)
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

            // ========================================
            // [핵심 수정] 두 가지 JSON 형식 지원
            // ========================================

            // 1. MainLandscape 형식 체크 (grid_info 존재 여부)
            if (config.contains("grid_info"))
            {
                // MainLandscape 형식: grid_info.bounds 구조
                const auto& grid_info = config["grid_info"];

                if (!grid_info.contains("bounds"))
                {
                    std::cout << "[TerrainData] grid_info missing bounds" << std::endl;
                    return false;
                }

                const auto& bounds = grid_info["bounds"];

                _info.min_x = bounds.value("min_x", 0.0f);
                _info.max_x = bounds.value("max_x", 504.0f);
                _info.min_z = bounds.value("min_z", 0.0f);
                _info.max_z = bounds.value("max_z", 504.0f);

                // grid_info에 width, height가 있으면 읽기 (나중에 파일 크기로 덮어쓸 수 있음)
                _info.width = grid_info.value("width", 505.0f);
                _info.height = grid_info.value("height", 505.0f);
            }
            // 2. 기존 형식 체크 (최상위 bounds)
            else if (config.contains("bounds"))
            {
                // 기존 ../../Common/MapData/Heightmap.json 형식
                if (!config["bounds"].contains("min_x") || !config["bounds"].contains("max_x") ||
                    !config["bounds"].contains("min_z") || !config["bounds"].contains("max_z"))
                {
                    std::cout << "[TerrainData] bounds incomplete" << std::endl;
                    return false;
                }

                _info.min_x = config["bounds"]["min_x"].get<float>();
                _info.max_x = config["bounds"]["max_x"].get<float>();
                _info.min_z = config["bounds"]["min_z"].get<float>();
                _info.max_z = config["bounds"]["max_z"].get<float>();
            }
            else
            {
                std::cout << "[TerrainData] JSON missing 'bounds' or 'grid_info'" << std::endl;
                return false;
            }

            // scale.y 추출 (공통)
            if (!config.contains("scale") || !config["scale"].contains("y"))
            {
                std::cout << "[TerrainData] JSON missing scale.y" << std::endl;
                return false;
            }
            _info.height_scale = config["scale"]["y"].get<float>();

            // heightmap_file 추출 (공통)
            if (!config.contains("heightmap_file"))
            {
                std::cout << "[TerrainData] JSON missing heightmap_file" << std::endl;
                return false;
            }
            std::string raw_filename = config["heightmap_file"].get<std::string>();

            // Heightmap 파일 전체 경로 구성
            std::filesystem::path path_obj(json_path);
            std::filesystem::path dir = path_obj.parent_path();
            std::filesystem::path heightmap_full_path = dir / raw_filename;
            _heightMapPath = heightmap_full_path.string();

            // Heightmap R16 파일 로드
            std::ifstream hm_file(_heightMapPath, std::ios::binary | std::ios::ate);
            if (!hm_file.is_open())
            {
                std::cout << "[TerrainData] Failed to open heightmap: " << _heightMapPath << std::endl;
                return false;
            }

            // 파일 크기로부터 해상도 자동 계산
            std::streamsize fileSize = hm_file.tellg();
            hm_file.seekg(0, std::ios::beg);

            size_t total_bytes = static_cast<size_t>(fileSize);
            size_t total_pixels_from_file = total_bytes / 2; // R16 = 2 bytes per pixel
            size_t calculated_resolution = static_cast<size_t>(std::sqrt(total_pixels_from_file));

            // 정사각형 검증
            if (calculated_resolution * calculated_resolution != total_pixels_from_file)
            {
                std::cout << "[TerrainData] Error: Heightmap not square. Size: " << total_bytes << " bytes" << std::endl;
                return false;
            }

            // 실제 파일 크기로 검증된 해상도로 덮어쓰기
            _info.width = static_cast<float>(calculated_resolution);
            _info.height = static_cast<float>(calculated_resolution);

            // 높이 데이터 벡터 준비
            _heights.resize(total_pixels_from_file);


            float min_h_m = (std::numeric_limits<float>::max)(); // 최소 높이(m) 추적용
            uint16_t min_raw_val = 65535;

            // R16 바이너리 읽기
            hm_file.seekg(0, std::ios::beg);

            for (size_t i = 0; i < total_pixels_from_file; ++i)
            {
                uint16_t raw_val;
                if (!hm_file.read(reinterpret_cast<char*>(&raw_val), sizeof(uint16_t)))
                {
                    break;
                }
                min_raw_val = (std::min)(min_raw_val, raw_val);

                // [핵심 수정] 0~1 정규화 대신 언리얼 공식 적용 및 미터(m) 단위 변환
                // 공식: ((Raw - 32768) / 128) * ScaleZ * 0.01
                float height_m = ((static_cast<float>(raw_val) - 32768.0f) / 128.0f) * _info.height_scale * 0.01f;

                _heights[i] = height_m;

                // 최소 높이값 추적
                min_h_m = std::min(height_m, min_h_m);
            }
            hm_file.close();

            // 정보용 최소 높이 저장 (마찬가지로 언리얼 공식 적용)
            _info.min_height = (static_cast<float>(min_raw_val) - 32768.0f) / 128.0f * _info.height_scale * 0.01f;

            // [수정] 조건부 바닥 보정
            if (apply_floor_offset)
            {
                // 단일 지형 또는 서버: 바닥을 0m로 맞춤
                for (size_t i = 0; i < total_pixels_from_file; ++i)
                {
                    // 이미 height_m으로 스케일 계산이 끝났으므로, 최하단 높이만 빼주면 됨
                    _heights[i] = _heights[i] - min_h_m;
                }
            }
            else
            {
                // 다중 타일 Landscape: 절대 높이 유지
                /*for (size_t i = 0; i < total_pixels_from_file; ++i)
                {
                    _heights[i] = (_heights[i] * _info.height_scale) - 10.0f;
                }*/
                // 다중 타일 Landscape: 절대 높이 유지
                // 타일 간 경계가 완벽히 맞물려야 하므로, 계산된 절대 높이를 그대로 둡니다.
                // (기존의 곱하기나 -10.0f 빼는 로직은 삭제!)
            }

            std::cout << "[TerrainData] Loaded: " << json_path
                << " (Resolution: " << _info.width << "x" << _info.height << ")" << std::endl;
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

            float fx = f_idx_x - static_cast<float>(x0); // x 방향 소수부 (0.0 ~ 1.0)
            float fz = f_idx_z - static_cast<float>(z0); // z 방향 소수부 (0.0 ~ 1.0)

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
        const std::vector<float>& GetRawData() const { return _heights; }

    private:
        TerrainInfo _info{};
        std::vector<float> _heights;
        std::string _heightMapPath;
    };
}