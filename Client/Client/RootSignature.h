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

class GltfHpRootSignatureGenerator : public IRootSignatureGenerator
{
public:
    virtual const std::string& name() const override;
    virtual ComPtr<ID3D12RootSignature> create(ID3D12Device* device) override;
};

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
