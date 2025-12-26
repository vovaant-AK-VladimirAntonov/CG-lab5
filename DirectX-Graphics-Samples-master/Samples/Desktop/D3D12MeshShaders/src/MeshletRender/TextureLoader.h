//*********************************************************
//
// Texture Loader for DDS files
//
//*********************************************************

#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;

class TextureLoader
{
public:
    static HRESULT LoadDDSTexture(
        ID3D12Device* device,
        ID3D12CommandQueue* cmdQueue,
        ID3D12CommandAllocator* cmdAlloc,
        ID3D12GraphicsCommandList* cmdList,
        const wchar_t* filename,
        ComPtr<ID3D12Resource>& texture,
        D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);

private:
    static HRESULT CreateTextureFromDDS(
        ID3D12Device* device,
        const uint8_t* ddsData,
        size_t ddsDataSize,
        ComPtr<ID3D12Resource>& texture,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
};
