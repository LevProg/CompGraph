#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>

struct GeomBuffer {
    DirectX::XMMATRIX m;
};

struct SceneBuffer {
    DirectX::XMMATRIX vp;
};

class Dx11App
{
public:
    bool Init(HWND hwnd, int width, int height);
    void Cleanup();

    void Render();
    void OnResize(int width, int height);
    void OnKeyDown(WPARAM key);

private:
    void CreateRenderTarget();
    void ReleaseRenderTarget();
    bool InitGeometry();
    bool CompileShader(const std::wstring& path, const std::string& entryPoint,
                       const std::string& target, ID3DBlob** ppCode);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_geomBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_sceneBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;

    D3D11_VIEWPORT m_viewport{};
    int m_width = 0;
    int m_height = 0;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.3f;
    LARGE_INTEGER m_startTime{};
    LARGE_INTEGER m_freq{};
};
