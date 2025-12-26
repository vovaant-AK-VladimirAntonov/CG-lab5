//*********************************************************
//
// Texture Loader implementation
//
//*********************************************************

#include "stdafx.h"
#include "TextureLoader.h"
#include "DXSampleHelper.h"
#include <fstream>

// DDS file structures
#pragma pack(push, 1)

const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

struct DDS_PIXELFORMAT
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
};

struct DDS_HEADER
{
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

struct DDS_HEADER_DXT10
{
    DXGI_FORMAT dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

#pragma pack(pop)

#define DDPF_FOURCC 0x00000004

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
    ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif

static DXGI_FORMAT GetDXGIFormat(const DDS_PIXELFORMAT& ddpf)
{
    if (ddpf.flags & DDPF_FOURCC)
    {
        if (ddpf.fourCC == MAKEFOURCC('D', 'X', 'T', '1'))
            return DXGI_FORMAT_BC1_UNORM;
        if (ddpf.fourCC == MAKEFOURCC('D', 'X', 'T', '3'))
            return DXGI_FORMAT_BC2_UNORM;
        if (ddpf.fourCC == MAKEFOURCC('D', 'X', 'T', '5'))
            return DXGI_FORMAT_BC3_UNORM;
        if (ddpf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
            return DXGI_FORMAT_UNKNOWN; // Need DX10 header
        if (ddpf.fourCC == MAKEFOURCC('B', 'C', '4', 'U'))
            return DXGI_FORMAT_BC4_UNORM;
        if (ddpf.fourCC == MAKEFOURCC('B', 'C', '5', 'U'))
            return DXGI_FORMAT_BC5_UNORM;
    }
    
    if (ddpf.RGBBitCount == 32)
    {
        if (ddpf.RBitMask == 0x000000ff && ddpf.GBitMask == 0x0000ff00 &&
            ddpf.BBitMask == 0x00ff0000 && ddpf.ABitMask == 0xff000000)
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        if (ddpf.RBitMask == 0x00ff0000 && ddpf.GBitMask == 0x0000ff00 &&
            ddpf.BBitMask == 0x000000ff && ddpf.ABitMask == 0xff000000)
            return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    
    return DXGI_FORMAT_R8G8B8A8_UNORM; // Default fallback
}

static size_t BitsPerPixel(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return 32;
    default:
        return 32;
    }
}

static bool IsCompressed(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}


HRESULT TextureLoader::CreateTextureFromDDS(
    ID3D12Device* device,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    ComPtr<ID3D12Resource>& texture,
    std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
    D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
    if (ddsDataSize < sizeof(uint32_t) + sizeof(DDS_HEADER))
        return E_FAIL;

    const uint32_t* magicPtr = reinterpret_cast<const uint32_t*>(ddsData);
    if (*magicPtr != DDS_MAGIC)
        return E_FAIL;

    const DDS_HEADER* header = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t));
    
    size_t offset = sizeof(uint32_t) + sizeof(DDS_HEADER);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    
    // Check for DX10 extended header
    if ((header->ddspf.flags & DDPF_FOURCC) && 
        header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
    {
        const DDS_HEADER_DXT10* dx10Header = reinterpret_cast<const DDS_HEADER_DXT10*>(ddsData + offset);
        format = dx10Header->dxgiFormat;
        offset += sizeof(DDS_HEADER_DXT10);
    }
    else
    {
        format = GetDXGIFormat(header->ddspf);
    }

    if (format == DXGI_FORMAT_UNKNOWN)
        return E_FAIL;

    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t mipCount = header->mipMapCount > 0 ? header->mipMapCount : 1;

    // Create texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = static_cast<UINT16>(mipCount);
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture));

    if (FAILED(hr))
        return hr;

    // Prepare subresource data
    const uint8_t* srcData = ddsData + offset;
    subresources.resize(mipCount);

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        uint32_t mipWidth = max(1u, width >> mip);
        uint32_t mipHeight = max(1u, height >> mip);

        size_t rowPitch, slicePitch;
        
        if (IsCompressed(format))
        {
            size_t blockWidth = max(1u, (mipWidth + 3) / 4);
            size_t blockHeight = max(1u, (mipHeight + 3) / 4);
            size_t bytesPerBlock = (format == DXGI_FORMAT_BC1_UNORM || 
                                   format == DXGI_FORMAT_BC4_UNORM) ? 8 : 16;
            rowPitch = blockWidth * bytesPerBlock;
            slicePitch = rowPitch * blockHeight;
        }
        else
        {
            rowPitch = (mipWidth * BitsPerPixel(format) + 7) / 8;
            slicePitch = rowPitch * mipHeight;
        }

        subresources[mip].pData = srcData;
        subresources[mip].RowPitch = rowPitch;
        subresources[mip].SlicePitch = slicePitch;

        srcData += slicePitch;
    }

    // Setup SRV desc
    srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = mipCount;
    srvDesc.Texture2D.MostDetailedMip = 0;

    return S_OK;
}

HRESULT TextureLoader::LoadDDSTexture(
    ID3D12Device* device,
    ID3D12CommandQueue* cmdQueue,
    ID3D12CommandAllocator* cmdAlloc,
    ID3D12GraphicsCommandList* cmdList,
    const wchar_t* filename,
    ComPtr<ID3D12Resource>& texture,
    D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
    // Load file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return E_INVALIDARG;

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> ddsData(fileSize);
    file.read(reinterpret_cast<char*>(ddsData.data()), fileSize);
    file.close();

    // Parse DDS and create texture
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    HRESULT hr = CreateTextureFromDDS(device, ddsData.data(), ddsData.size(), 
                                       texture, subresources, srvDesc);
    if (FAILED(hr))
        return hr;

    // Create upload buffer
    UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 
                                                          static_cast<UINT>(subresources.size()));
    
    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    
    ComPtr<ID3D12Resource> uploadBuffer;
    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));

    if (FAILED(hr))
        return hr;

    // Upload texture data
    cmdList->Reset(cmdAlloc, nullptr);

    UpdateSubresources(cmdList, texture.Get(), uploadBuffer.Get(), 0, 0,
                       static_cast<UINT>(subresources.size()), subresources.data());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->Close();

    ID3D12CommandList* cmdLists[] = { cmdList };
    cmdQueue->ExecuteCommandLists(1, cmdLists);

    // Wait for upload
    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    cmdQueue->Signal(fence.Get(), 1);

    if (fence->GetCompletedValue() < 1)
    {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(1, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }

    return S_OK;
}
