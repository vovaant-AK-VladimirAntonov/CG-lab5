//*********************************************************
//
// DirectStorage Loader for mesh data
//
//*********************************************************

#pragma once

#include <dstorage.h>
#include <wrl/client.h>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;

class DirectStorageLoader
{
public:
    DirectStorageLoader();
    ~DirectStorageLoader();

    HRESULT Initialize(ID3D12Device* device);
    HRESULT LoadFileToMemory(const wchar_t* filename, std::vector<uint8_t>& outBuffer);
    HRESULT LoadFileToGpuBuffer(const wchar_t* filename, ID3D12Resource* destResource, uint64_t destOffset = 0);
    void WaitForCompletion();
    bool IsSupported() const { return m_isSupported; }

private:
    ComPtr<IDStorageFactory> m_factory;
    ComPtr<IDStorageQueue> m_queue;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    uint64_t m_fenceValue;
    bool m_isSupported;
};
