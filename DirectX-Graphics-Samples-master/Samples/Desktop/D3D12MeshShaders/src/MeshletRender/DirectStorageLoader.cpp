//*********************************************************
//
// DirectStorage Loader implementation
//
//*********************************************************

#include "stdafx.h"
#include "DirectStorageLoader.h"
#include "DXSampleHelper.h"

DirectStorageLoader::DirectStorageLoader()
    : m_fenceEvent(nullptr)
    , m_fenceValue(0)
    , m_isSupported(false)
{
}

DirectStorageLoader::~DirectStorageLoader()
{
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
    }
}

HRESULT DirectStorageLoader::Initialize(ID3D12Device* device)
{
    // Try to create DirectStorage factory
    HRESULT hr = DStorageGetFactory(IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
    {
        OutputDebugStringA("DirectStorage not available, falling back to standard I/O\n");
        m_isSupported = false;
        return S_OK; // Not an error, just not supported
    }

    // Create DirectStorage queue for GPU uploads
    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = device;

    hr = m_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_queue));
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create DirectStorage queue\n");
        m_isSupported = false;
        return S_OK;
    }

    // Create fence for synchronization
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
    {
        return hr;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_isSupported = true;
    OutputDebugStringA("DirectStorage initialized successfully\n");
    return S_OK;
}

HRESULT DirectStorageLoader::LoadFileToMemory(const wchar_t* filename, std::vector<uint8_t>& outBuffer)
{
    if (!m_isSupported)
    {
        // Fallback to standard file I/O
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return E_INVALIDARG;
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        outBuffer.resize(fileSize);
        file.read(reinterpret_cast<char*>(outBuffer.data()), fileSize);
        file.close();

        return S_OK;
    }

    // Open file with DirectStorage
    ComPtr<IDStorageFile> file;
    HRESULT hr = m_factory->OpenFile(filename, IID_PPV_ARGS(&file));
    if (FAILED(hr))
    {
        return hr;
    }

    BY_HANDLE_FILE_INFORMATION fileInfo;
    hr = file->GetFileInformation(&fileInfo);
    if (FAILED(hr))
    {
        return hr;
    }

    uint64_t fileSize = (static_cast<uint64_t>(fileInfo.nFileSizeHigh) << 32) | fileInfo.nFileSizeLow;
    outBuffer.resize(static_cast<size_t>(fileSize));

    // Create request for memory destination
    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = 0;
    request.Source.File.Size = static_cast<uint32_t>(fileSize);
    request.Destination.Memory.Buffer = outBuffer.data();
    request.Destination.Memory.Size = static_cast<uint32_t>(fileSize);
    request.UncompressedSize = static_cast<uint32_t>(fileSize);

    m_queue->EnqueueRequest(&request);

    // Signal and wait
    m_fenceValue++;
    m_queue->EnqueueSignal(m_fence.Get(), m_fenceValue);
    m_queue->Submit();

    WaitForCompletion();

    return S_OK;
}


HRESULT DirectStorageLoader::LoadFileToGpuBuffer(const wchar_t* filename, ID3D12Resource* destResource, uint64_t destOffset)
{
    if (!m_isSupported)
    {
        return E_NOTIMPL; // Caller should use fallback
    }

    // Open file with DirectStorage
    ComPtr<IDStorageFile> file;
    HRESULT hr = m_factory->OpenFile(filename, IID_PPV_ARGS(&file));
    if (FAILED(hr))
    {
        return hr;
    }

    BY_HANDLE_FILE_INFORMATION fileInfo;
    hr = file->GetFileInformation(&fileInfo);
    if (FAILED(hr))
    {
        return hr;
    }

    uint64_t fileSize = (static_cast<uint64_t>(fileInfo.nFileSizeHigh) << 32) | fileInfo.nFileSizeLow;

    // Create request for GPU buffer destination
    DSTORAGE_REQUEST request = {};
    request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    request.Source.File.Source = file.Get();
    request.Source.File.Offset = 0;
    request.Source.File.Size = static_cast<uint32_t>(fileSize);
    request.Destination.Buffer.Resource = destResource;
    request.Destination.Buffer.Offset = destOffset;
    request.Destination.Buffer.Size = static_cast<uint32_t>(fileSize);
    request.UncompressedSize = static_cast<uint32_t>(fileSize);

    m_queue->EnqueueRequest(&request);

    return S_OK;
}

void DirectStorageLoader::WaitForCompletion()
{
    if (!m_isSupported || !m_fence)
    {
        return;
    }

    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Check for errors
    DSTORAGE_ERROR_RECORD errorRecord = {};
    m_queue->RetrieveErrorRecord(&errorRecord);
    if (FAILED(errorRecord.FirstFailure.HResult))
    {
        OutputDebugStringA("DirectStorage error occurred\n");
    }
}
