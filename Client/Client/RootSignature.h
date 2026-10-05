#pragma once
class IRootSignatureGenerator
{
public:
    virtual ~IRootSignatureGenerator() = default;

    // 자신이 생성할 루트 시그니처의 이름을 반환해야 합니다.
    virtual const std::string& name() const = 0;

    // device를 받아 실제 루트 시그니처 객체를 생성하고 반환해야 합니다.
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) = 0;
};

class DefaultRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class GltfRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

//class GltfHpRootSignatureGenerator : public IRootSignatureGenerator
//{
//public:
//    virtual const std::string& name() const override;
//    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
//};

class SkinnedRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};


class SkyBoxRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class TerrainRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class UIRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;

    ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class BillboardUIRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class MonsterHPUIRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;

    ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class DebugRootSignatureGenerator : public IRootSignatureGenerator {
public:
    virtual const std::string& name() const override {
        static const std::string name = "debug";
        return name;
    }
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class CsmDepthRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class CsmDepthSkinnedRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class UIFrameRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class MinimapRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class OcclusionRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class ComputeParticleRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

class ParticleRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

// 범용 파티클 방출·갱신 컴퓨트: b0 방출기 상수, t0 방출 요청, u0 파티클 풀
class ParticleComputeRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

// 범용 파티클 빌보드: b0 오브젝트(엔진 호환), b1 카메라, b2 방출기 상수, t0 파티클 풀
class ParticleBillboardRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};