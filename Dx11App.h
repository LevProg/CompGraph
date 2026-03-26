#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>

struct GeomBuffer {
    DirectX::XMMATRIX model;
};

struct SceneBuffer {
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 cameraPos;
};

struct SkyboxGeomBuffer {
    DirectX::XMMATRIX model;
    DirectX::XMFLOAT4 size;
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
    bool InitSkybox();
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
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureView;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyboxVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyboxIB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyboxGeomBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_skyboxVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_skyboxPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_skyboxInputLayout;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cubemapTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cubemapView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_skyboxDSS;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_sceneBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;

    D3D11_VIEWPORT m_viewport{};
    int m_width = 0;
    int m_height = 0;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.3f;
    LARGE_INTEGER m_startTime{};
    LARGE_INTEGER m_freq{};
    UINT m_skyboxIndexCount = 0;
};
