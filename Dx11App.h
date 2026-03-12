#pragma once
#include <wrl/client.h>
#include <d3d11.h>

class Dx11App
{
public:
    bool Init(HWND hwnd, int width, int height);
    void Cleanup();

    void Render();
    void OnResize(int width, int height);

private:
    void CreateRenderTarget();
    void ReleaseRenderTarget();

private:
    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

    D3D11_VIEWPORT m_viewport{};
};
