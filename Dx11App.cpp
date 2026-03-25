#include "Dx11App.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cassert>
#include <cmath>
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
    {-0.5f, -0.5f,  0.5f, 255,   0,   0, 255},
    { 0.5f, -0.5f,  0.5f,   0, 255,   0, 255},
    { 0.5f,  0.5f,  0.5f,   0,   0, 255, 255},
    {-0.5f,  0.5f,  0.5f, 255, 255,   0, 255},
    {-0.5f, -0.5f, -0.5f, 255,   0, 255, 255},
    { 0.5f, -0.5f, -0.5f,   0, 255, 255, 255},
    { 0.5f,  0.5f, -0.5f, 128, 128, 128, 255},
    {-0.5f,  0.5f, -0.5f, 255, 128,   0, 255},
};

static const unsigned short Indices[] = {
    0, 2, 1,  0, 3, 2,
    4, 5, 6,  4, 6, 7,
    0, 4, 7,  0, 7, 3,
    1, 6, 5,  1, 2, 6,
    0, 1, 5,  0, 5, 4,
    3, 7, 6,  3, 6, 2,
};

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

bool Dx11App::InitGeometry()
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

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(GeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(m_device->CreateBuffer(&desc, nullptr, m_geomBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_geomBuffer.Get(), "GeomBuffer");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SceneBuffer);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(m_device->CreateBuffer(&desc, nullptr, m_sceneBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_sceneBuffer.Get(), "SceneBuffer");
    }

    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;

        if (FAILED(m_device->CreateRasterizerState(&desc, m_rasterizerState.GetAddressOf())))
            return false;
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);

    ID3DBlob* pVSCode = nullptr;
    if (!CompileShader(dir + L"cube.vs", "vs", "vs_5_0", &pVSCode))
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
    if (!CompileShader(dir + L"cube.ps", "ps", "ps_5_0", &pPSCode))
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

    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_startTime);

    OnResize(width, height);

    if (!InitGeometry())
        return false;

    return true;
}

void Dx11App::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());

    if (m_width > 0 && m_height > 0)
    {
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        m_device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.GetAddressOf());
        m_device->CreateDepthStencilView(m_depthTexture.Get(), nullptr, m_dsv.GetAddressOf());
    }
}

void Dx11App::ReleaseRenderTarget()
{
    m_dsv.Reset();
    m_depthTexture.Reset();
    m_rtv.Reset();
}

void Dx11App::OnResize(int width, int height)
{
    if (!m_swapChain || width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    ReleaseRenderTarget();
    m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
}

void Dx11App::OnKeyDown(WPARAM key)
{
    const float step = 0.05f;
    if (key == VK_LEFT)  m_cameraYaw -= step;
    if (key == VK_RIGHT) m_cameraYaw += step;
    if (key == VK_UP)    m_cameraPitch += step;
    if (key == VK_DOWN)  m_cameraPitch -= step;
    const float limit = DirectX::XM_PIDIV2 - 0.01f;
    if (m_cameraPitch >  limit) m_cameraPitch =  limit;
    if (m_cameraPitch < -limit) m_cameraPitch = -limit;
}

void Dx11App::Render()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float elapsed = (float)((double)(now.QuadPart - m_startTime.QuadPart) / m_freq.QuadPart);

    {
        GeomBuffer gb;
        gb.m = DirectX::XMMatrixRotationY(elapsed * DirectX::XM_PI * 0.5f);
        m_context->UpdateSubresource(m_geomBuffer.Get(), 0, nullptr, &gb, 0, 0);
    }

    {
        float radius = 3.0f;
        DirectX::XMVECTOR eye = DirectX::XMVectorSet(
            radius * sinf(m_cameraYaw) * cosf(m_cameraPitch),
            radius * sinf(m_cameraPitch),
            -radius * cosf(m_cameraYaw) * cosf(m_cameraPitch),
            1.0f);
        DirectX::XMVECTOR at = DirectX::XMVectorZero();
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        DirectX::XMMATRIX v = DirectX::XMMatrixLookAtLH(eye, at, up);

        float n = 0.1f, f = 100.0f;
        float fov = DirectX::XM_PI / 3.0f;
        float aspect = (float)m_width / (float)m_height;
        DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, n, f);

        D3D11_MAPPED_SUBRESOURCE sub;
        if (SUCCEEDED(m_context->Map(m_sceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sub)))
        {
            SceneBuffer& sb = *reinterpret_cast<SceneBuffer*>(sub.pData);
            sb.vp = DirectX::XMMatrixMultiply(v, p);
            m_context->Unmap(m_sceneBuffer.Get(), 0);
        }
    }

    m_context->ClearState();

    ID3D11RenderTargetView* views[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, views, m_dsv.Get());

    const float clearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_context->RSSetViewports(1, &m_viewport);
    m_context->RSSetState(m_rasterizerState.Get());

    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* vertexBuffers[] = { m_vertexBuffer.Get() };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    m_context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    ID3D11Buffer* cbs[] = { m_geomBuffer.Get(), m_sceneBuffer.Get() };
    m_context->VSSetConstantBuffers(0, 2, cbs);

    m_context->DrawIndexed(36, 0, 0);

    m_swapChain->Present(1, 0);
}

void Dx11App::Cleanup()
{
    m_rasterizerState.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_sceneBuffer.Reset();
    m_geomBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_dsv.Reset();
    m_depthTexture.Reset();
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}
