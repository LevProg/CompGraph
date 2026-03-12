#include "Dx11App.h"
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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
    const float clearColor[4] = { 0.5f, 1.0f, 0.5f, 1.0f };

    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_swapChain->Present(1, 0);
}

void Dx11App::Cleanup()
{
    m_rtv.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}
