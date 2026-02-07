#pragma once
#if defined(_DEBUG)
#define _DEBUG_PHYSICS_VISUALIZATION
#endif
#define NOMINMAX

// C++ 표준 라이브러리 헤더
#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <typeindex>
#include <list>
#include <fstream>
#include <sstream>
#include <numeric>
#include <random>
#include <queue>
#include <unordered_map>
#include <functional>
#include <concepts>
#include <string>
#include <atomic>
#include <mutex>
#include <stack>
#include <thread>
#include <concurrent_queue.h>

//#define _WITH_SWAPCHAIN_FULLSCREEN_STATE
// assmip
#include <assimp/Importer.hpp>      // Assimp 로더
#include <assimp/scene.h>           // Assimp scene 객체
#include <assimp/postprocess.h>     // Assimp 후처리 옵션
#include <assimp/material.h> // AI_MATKEY_TEXTURE_DIFFUSE, AI_MATKEY_COLOR_DIFFUSE 정의 포함


#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")


#include <tchar.h>

#include <wrl.h>
#include <shellapi.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <d3dcompiler.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXcolors.h>

#include <DirectXCollision.h>
#include <dxgidebug.h>

#include "d3dx12.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

using Microsoft::WRL::ComPtr;

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")


// 필요한 것들 추가
#include "json.hpp"

// DirectXTex
#include "DirectXTex.h"



// 


// Jolt Physics 헤더
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>


#define EXPLOSION_DEBRISES		240

/*정점의 색상을 무작위로(Random) 설정하기 위해 사용한다. 각 정점의 색상은 난수(Random Number)를 생성하여
지정한다.*/
#define RANDOM_COLOR XMFLOAT4(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX))

#define DegreeToRadian(x)		float((x)*3.141592654f/180.0f)

#define RadianToDegree(x)		float((x)*180.0f/3.141592654f)

#define EPSILON					1.0e-6f


// 스왑체인 개수
constexpr UINT SWAP_CHAIN_BUFFERS = 2;


// 이것은 오브젝트 매니저를 위한 변수입니다.
#define ALLARRAYSIZE				5


constexpr uint32_t FRAME_BUFFER_WIDTH = 1200;
constexpr uint32_t FRAME_BUFFER_HEIGHT = 800;


// 라이팅 및 PONG_SHADER 관련 상수 [PONG]
#define MAX_LIGHTS			8 
#define MAX_MATERIALS		8

#define POINT_LIGHT			1
#define SPOT_LIGHT			2
#define DIRECTIONAL_LIGHT	3

extern ID3D12Resource* CreateBufferResource(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nBytes, 
	D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
	ID3D12Resource** ppd3dUploadBuffer = NULL);
//extern void UpdateSubresources(
//	ID3D12GraphicsCommandList* pCmdList,
//	ID3D12Resource* pDestinationResource,
//	ID3D12Resource* pIntermediate,
//	UINT64 IntermediateOffset,
//	UINT FirstSubresource,
//	UINT NumSubresources,
//	D3D12_SUBRESOURCE_DATA* pSrcData);
ID3D12Resource* CreateTextureResourceFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, wchar_t* pszFileName, ID3D12Resource** ppd3dUploadBuffer, D3D12_RESOURCE_STATES d3dResourceStates);
// ==================================================
// 디버그 로그 매크로
// ==================================================
#if defined(_DEBUG)

	#include <sstream>

	// 스트림 기반 로그 함수 (std::string)
	inline void DebugLogStream(const std::ostringstream& oss)
	{
		std::string msg = oss.str() + "\n";
		OutputDebugStringA(msg.c_str());
	}

	// 매크로로 간단하게 사용
	// TODO: 흠.... 콘솔도 같이 띄워서 디버그 로그, 정보, 에러 텍스트 색깔 다르게 출력하는 기능 추가하고 싶다.
	#define CINFO(expr) \
	{std::ostringstream oss; oss << "[" << __FILE__ << ":" << __LINE__ << "] " << expr; DebugLogStream(oss);}
	#define CLOG(expr) \
	        {std::ostringstream oss; oss << "[" << __FILE__ << ":" << __LINE__ << "] " << expr; DebugLogStream(oss);}
	#define CERROR(expr) \
	        {std::ostringstream oss; oss << "[" << __FILE__ << ":" << __LINE__ << "] " << expr; DebugLogStream(oss); DebugBreak();}
#else
	#define CLOG(expr)
	#define CERROR(expr)
	#define CINFO(expr)
#endif

#include "Packet.h"
#include "PacketStream.h"

using f2 = XMFLOAT2;
using f3 = XMFLOAT3;
using f4 = XMFLOAT4;
using f4x4 = XMFLOAT4X4;
using m4 = XMMATRIX;
using v4 = XMVECTOR;

constexpr f3 F3_ONE = f3(1.0f, 1.0f, 1.0f);
constexpr f3 F3_ZERO = f3(0.0f, 0.0f, 0.0f);
constexpr f3 F3_UP = f3(0.0f, 1.0f, 0.0f);
constexpr f3 F3_DOWN = f3(0.0f, -1.0f, 0.0f);
constexpr f3 F3_RIGHT = f3(1.0f, 0.0f, 0.0f);
constexpr f3 F3_LEFT = f3(-1.0f, 0.0f, 0.0f);
constexpr f3 F3_FORWARD = f3(0.0f, 0.0f, 1.0f);
constexpr f3 F3_BACKWARD = f3(0.0f, 0.0f, -1.0f);

constexpr f4x4 F4X4_IDENTIFY = f4x4(1.0f, 0.0f, 0.0f, 0.0f,
									0.0f, 1.0f, 0.0f, 0.0f,
									0.0f, 0.0f, 1.0f, 0.0f,
									0.0f, 0.0f, 0.0f, 1.0f);

// ==================================================
template <typename T>
class Singleton
{
protected:
	Singleton() = default;
	virtual ~Singleton() = default;

public:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	virtual void release(){};

	static T* instance()
	{
		static T instance;
		return &instance;
	}
};

//3차원 벡터의 연산 
namespace Vector3
{
	inline XMFLOAT3 XMVectorToFloat3(const XMVECTOR& xmvVector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, xmvVector);
		return(xmf3Result);
	}
	inline XMFLOAT3 ScalarProduct(const XMFLOAT3& xmf3Vector, float fScalar, bool bNormalize =
		true)
	{
		XMFLOAT3 xmf3Result;
		if (bNormalize)
			XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)) *
				fScalar);
		else
			XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector) * fScalar);
		return(xmf3Result);
	}
	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + XMLoadFloat3(&xmf3Vector2));
		return(xmf3Result);
	}
	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, float fScalar)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + (XMLoadFloat3(&xmf3Vector2)
			* fScalar));
		return(xmf3Result);
	}
	inline XMFLOAT3 Subtract(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) -
			XMLoadFloat3(&xmf3Vector2));
		return(xmf3Result);
	}
	inline float DotProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Dot(XMLoadFloat3(&xmf3Vector1),
			XMLoadFloat3(&xmf3Vector2)));
		return(xmf3Result.x);
	}
	inline XMFLOAT3 CrossProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, bool bNormalize = true)
	{
		XMFLOAT3 xmf3Result;
		if (bNormalize)
			XMStoreFloat3(&xmf3Result,
				XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&xmf3Vector1),
					XMLoadFloat3(&xmf3Vector2))));
		else
			XMStoreFloat3(&xmf3Result, XMVector3Cross(XMLoadFloat3(&xmf3Vector1),
				XMLoadFloat3(&xmf3Vector2)));
		return(xmf3Result);
	}
	inline XMFLOAT3 Normalize(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 m_xmf3Normal;
		XMStoreFloat3(&m_xmf3Normal, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)));
		return(m_xmf3Normal);
	}
	inline float Length(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Length(XMLoadFloat3(&xmf3Vector)));
		return(xmf3Result.x);
	}
	inline float Angle(const XMVECTOR xmvVector1, XMVECTOR&& xmvVector2) { // DW수정 이거 문제 생기면 말하기
		XMVECTOR xmvAngle = XMVector3AngleBetweenNormals(xmvVector1, xmvVector2);
		return(XMConvertToDegrees(acosf(XMVectorGetX(xmvAngle))));
	}
	inline float Angle(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		return(Angle(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
	}
	inline XMFLOAT3 TransformNormal(const XMFLOAT3& xmf3Vector, const XMMATRIX& transform)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformNormal(XMLoadFloat3(&xmf3Vector),
			transform));
		return(xmf3Result);
	}
	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMMATRIX& transform) // DW수정 이거 문제 생기면 말하기
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformCoord(XMLoadFloat3(&xmf3Vector),
			transform));
		return(xmf3Result);
	}
	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMFLOAT4X4& Matrix)
	{
		return(TransformCoord(xmf3Vector, XMLoadFloat4x4(&Matrix)));
	}
}
//4차원 벡터의 연산
namespace Vector4
{
	inline XMFLOAT4 Add(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) +
			XMLoadFloat4(&xmf4Vector2));
		return(xmf4Result);
	}
	inline XMFLOAT4 Multiply(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) *
			XMLoadFloat4(&xmf4Vector2));
		return(xmf4Result);
	}
	inline XMFLOAT4 Multiply(float fScalar, const XMFLOAT4& xmf4Vector)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, fScalar * XMLoadFloat4(&xmf4Vector));
		return(xmf4Result);
	}
}
//행렬의 연산
namespace Matrix4x4
{
	inline XMFLOAT4X4 Identity()
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixIdentity());
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) *
			XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMMATRIX& xmmtxMatrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) * xmmtxMatrix2);
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 Multiply(const XMMATRIX& xmmtxMatrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, xmmtxMatrix1 * XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 Inverse(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixInverse(NULL,
			XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 Transpose(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result,
			XMMatrixTranspose(XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 PerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ,
		float FarZ)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 LookAtLH(const XMFLOAT3& xmf3EyePosition, const XMFLOAT3& xmf3LookAtPosition,
	                           const XMFLOAT3& xmf3UpDirection)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixLookAtLH(XMLoadFloat3(&xmf3EyePosition),
			XMLoadFloat3(&xmf3LookAtPosition), XMLoadFloat3(&xmf3UpDirection)));
		return(xmmtx4x4Result);
	}
	inline XMFLOAT4X4 LookToLH(const XMFLOAT3& xmf3EyePosition, const XMFLOAT3& xmf3LookTo, const XMFLOAT3& xmf3UpDirection)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixLookToLH(XMLoadFloat3(&xmf3EyePosition), XMLoadFloat3(&xmf3LookTo), XMLoadFloat3(&xmf3UpDirection)));
		return(xmmtx4x4Result);
	}
}

using json = nlohmann::json;

namespace JsonHelper
{
	/**
	 * @brief Location 또는 Scale 같은 3D 벡터 JSON 객체를 파싱합니다.
	 * @param data "X", "Y", "Z" 키를 가진 JSON 객체.
	 * @return 파싱된 XMFLOAT3 값.
	 */
	inline XMFLOAT3 ParseVector3(const json& data)
	{
		XMFLOAT3 vector;
		// json 객체에서 직접 "X", "Y", "Z" 키의 값을 float으로 가져옵니다.
		vector.x = data.at("X").get<float>();
		vector.y = data.at("Y").get<float>();
		vector.z = data.at("Z").get<float>();
		return vector;
	}

	/**
	 * @brief Rotation JSON 객체를 파싱합니다.
	 * @param data "Pitch", "Yaw", "Roll" 키를 가진 JSON 객체.
	 * @return 파싱된 XMFLOAT3 값.
	 */
	inline XMFLOAT3 ParseRotation(const json& data)
	{
		XMFLOAT3 rotation;
		// JSON 파일의 키 이름("Pitch", "Yaw", "Roll")과 정확히 일치해야 합니다.
		rotation.x = data.at("Pitch").get<float>();
		rotation.y = data.at("Yaw").get<float>();
		rotation.z = data.at("Roll").get<float>();
		return rotation;
	}
}
