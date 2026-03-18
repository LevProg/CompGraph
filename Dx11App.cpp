#include "Dx11App.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cassert>
#include <vector>
#include <windows.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

static HRESULT SetResourceName(ID3D11DeviceChild* pResource, const std::string& name)
{
    return pResource->SetPrivateData(WKPDID_D3DDebugObjectName,
        (UINT)name.length(), name.c_str());
}

struct Vertex {
    float x, y, z;
    unsigned char r, g, b, a;
};

static const Vertex Vertices[] = {
    {-0.5f, -0.5f, 0.0f,  255, 0,   0,   255},
    { 0.5f, -0.5f, 0.0f,  0,   255, 0,   255},
    { 0.0f,  0.5f, 0.0f,  0,   0,   255, 255}
};

static const unsigned short Indices[] = { 0, 2, 1 };

bool Dx11App::CompileShader(const std::wstring& path, const std::string& entryPoint,
                             const std::string& target, ID3DBlob** ppCode)
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, path.c_str(), L"rb");
    if (!pFile)
        return false;

    fseek(pFile, 0, SEEK_END);
    long sz = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    std::vector<char> src(sz);
    fread(src.data(), 1, sz, pFile);
    fclose(pFile);

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrMsg = nullptr;
    HRESULT hr = D3DCompile(src.data(), src.size(), nullptr, nullptr, nullptr,
        entryPoint.c_str(), target.c_str(), flags, 0, ppCode, &pErrMsg);

    if (FAILED(hr) && pErrMsg)
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());

    if (pErrMsg)
        pErrMsg->Release();

    return SUCCEEDED(hr);
}

bool Dx11App::InitTriangle()
{
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Vertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = Vertices;

        if (FAILED(m_device->CreateBuffer(&desc, &data, m_vertexBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_vertexBuffer.Get(), "VertexBuffer");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(Indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = Indices;

        if (FAILED(m_device->CreateBuffer(&desc, &data, m_indexBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_indexBuffer.Get(), "IndexBuffer");
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);

    ID3DBlob* pVSCode = nullptr;
    if (!CompileShader(dir + L"triangle.vs", "vs", "vs_5_0", &pVSCode))
        return false;

    if (FAILED(m_device->CreateVertexShader(pVSCode->GetBufferPointer(),
        pVSCode->GetBufferSize(), nullptr, m_vertexShader.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }
    SetResourceName(m_vertexShader.Get(), "VertexShader");

    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(m_device->CreateInputLayout(InputDesc, 2,
        pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(),
        m_inputLayout.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }
    SetResourceName(m_inputLayout.Get(), "InputLayout");
    pVSCode->Release();

    ID3DBlob* pPSCode = nullptr;
    if (!CompileShader(dir + L"triangle.ps", "ps", "ps_5_0", &pPSCode))
        return false;

    if (FAILED(m_device->CreatePixelShader(pPSCode->GetBufferPointer(),
        pPSCode->GetBufferSize(), nullptr, m_pixelShader.GetAddressOf())))
    {
        pPSCode->Release();
        return false;
    }
    SetResourceName(m_pixelShader.Get(), "PixelShader");
    pPSCode->Release();

    return true;
}


bool Dx11App::Init(HWND hwnd, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &fl,
        m_context.GetAddressOf())))
    {
        return false;
    }

    CreateRenderTarget();
    OnResize(width, height);

    if (!InitTriangle())
        return false;

    return true;
}

void Dx11App::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
}

void Dx11App::ReleaseRenderTarget()
{
    m_rtv.Reset();
}

void Dx11App::OnResize(int width, int height)
{
    if (!m_swapChain || width == 0 || height == 0)
        return;

    ReleaseRenderTarget();
    m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &m_viewport);
}

void Dx11App::Render()
{
    m_context->ClearState();

    ID3D11RenderTargetView* views[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, views, nullptr);

    const float clearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);

    m_context->RSSetViewports(1, &m_viewport);

    D3D11_RECT rect = { 0, 0, (LONG)m_viewport.Width, (LONG)m_viewport.Height };
    m_context->RSSetScissorRects(1, &rect);

    // Draw triangle
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* vertexBuffers[] = { m_vertexBuffer.Get() };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    m_context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->DrawIndexed(3, 0, 0);

    m_swapChain->Present(1, 0);
}

void Dx11App::Cleanup()
{
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}
