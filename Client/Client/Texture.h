#pragma once

struct Texture
{
	std::string name; // 텍스쳐 이름
	std::wstring w_name; // 텍스쳐 이름 (wide string)

	ComPtr<ID3D12Resource> resource = nullptr; // GPU 리소스
	ComPtr<ID3D12Resource> upload_heap = nullptr; // 업로드 힙 (임시 버퍼)

	std::unique_ptr<uint8_t[]> dds_data; // DDS 파일에서 읽은 데이터

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_srv_handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_srv_handle{};
};