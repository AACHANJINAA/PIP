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
                // 로그는 std::cout 사용 (각 프로젝트의 로그 매크로 의존성 제거)
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

            // 필수 필드 확인
            if (!config.contains("bounds") || !config["bounds"].contains("min_x") || !config["bounds"].contains("max_x") ||
                !config["bounds"].contains("min_z") || !config["bounds"].contains("max_z") ||
                !config.contains("width") || !config.contains("height") || !config.contains("scale") ||
                !config["scale"].contains("y") || !config.contains("heightmap_file"))
            {
                std::cout << "[TerrainData] JSON missing required fields in " << json_path << std::endl;
                return false;
            }

            _info.min_x = config["bounds"]["min_x"].get<float>();
            _info.max_x = config["bounds"]["max_x"].get<float>();
            _info.min_z = config["bounds"]["min_z"].get<float>();
            _info.max_z = config["bounds"]["max_z"].get<float>();
            _info.width = static_cast<float>(config["width"].get<int>());
            _info.height = static_cast<float>(config["height"].get<int>());
            _info.height_scale = config["scale"]["y"].get<float>();

            std::string raw_filename = config["heightmap_file"].get<std::string>();
            
            // JSON 파일 기준 상대 경로 처리 (Raw 파일의 절대 경로를 얻기 위함)
            std::filesystem::path path_obj(json_path);
            std::filesystem::path dir = path_obj.parent_path();
            std::filesystem::path heightmap_full_path = dir / raw_filename;
            _heightMapPath = heightmap_full_path.string();

            std::ifstream hm_file(_heightMapPath, std::ios::binary);
            if (!hm_file.is_open())
            {
                std::cout << "[TerrainData] Failed to open heightmap raw file: " << _heightMapPath << std::endl;
                return false;
            }

            size_t grid_w = static_cast<size_t>(_info.width);
            size_t grid_h = static_cast<size_t>(_info.height);
            size_t total_pixels = grid_w * grid_h;

            _heights.resize(total_pixels);

            uint16_t min_raw_val = 65535; // R16 파일의 최소값 추적
            // R16 읽기 (16비트 unsigned integer)
            for (size_t i = 0; i < total_pixels; ++i)
            {
                uint16_t raw_val;
                if (!hm_file.read(reinterpret_cast<char*>(&raw_val), sizeof(uint16_t)))
                {
                    std::cout << "[TerrainData] Failed to read R16 data at index " << i << std::endl;
                    break; // 읽기 실패 시 루프 중단
                }
                min_raw_val = (std::min)(min_raw_val, raw_val);
                
                // 0~1 정규화 후 height_scale 곱하여 최종 높이 저장
                _heights[i] = (static_cast<float>(raw_val) / 65535.0f) * _info.height_scale;
            }            
        	hm_file.close();
            // 지형의 최소 높이 정보 (클라이언트 렌더링에 유용할 수 있음)
            _info.min_height = (static_cast<float>(min_raw_val) / 65535.0f) * _info.height_scale;

            std::cout << "[TerrainData] Loaded successfully from " << json_path << std::endl;
            return true;
        }

        inline float GetHeightAt(float x, float z) const
        {
            // 1. 기본 범위 체크
            if (x < _info.min_x || x > _info.max_x || z < _info.min_z || z > _info.max_z)
                return 0.0f; // 범위 밖이면 0 반환 (혹은 가장 가까운 경계 높이 등 다른 정책)

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

            // 7. 삼각형 보간 (Slash 방향 /)
            // 현재 지점이 그리드 셀의 어느 삼각형에 속하는지에 따라 보간
            if (fx + fz <= 1.0f)
            {
                // 좌상단 삼각형 (h00, h10, h01을 잇는 삼각형)
                return h00 + (h10 - h00) * fx + (h01 - h00) * fz;
            }
            else
            {
                // 우하단 삼각형 (h10, h01, h11을 잇는 삼각형)
                // 또는 h11을 기준으로 역으로 보간
                return h11 + (h10 - h11) * (1.0f - fz) + (h01 - h11) * (1.0f - fx);
            }
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