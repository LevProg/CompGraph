#include "Dx11App.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cassert>
#include <cmath>
#include <algorithm>
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

#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    UINT32 dwSize;
    UINT32 dwFlags;
    UINT32 dwFourCC;
    UINT32 dwRGBBitCount;
    UINT32 dwRBitMask;
    UINT32 dwGBitMask;
    UINT32 dwBBitMask;
    UINT32 dwABitMask;
};

struct DDS_HEADER {
    UINT32 dwSize;
    UINT32 dwFlags;
    UINT32 dwHeight;
    UINT32 dwWidth;
    UINT32 dwPitchOrLinearSize;
    UINT32 dwDepth;
    UINT32 dwMipMapCount;
    UINT32 dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    UINT32 dwCaps;
    UINT32 dwCaps2;
    UINT32 dwCaps3;
    UINT32 dwCaps4;
    UINT32 dwReserved2;
};
#pragma pack(pop)

#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#ifndef MAKEFOURCC
#define MAKEFOURCC(a,b,c,d) ((UINT32)(a)|((UINT32)(b)<<8)|((UINT32)(c)<<16)|((UINT32)(d)<<24))
#endif

static UINT32 DivUp(UINT32 a, UINT32 b) { return (a + b - 1) / b; }

static UINT32 GetBytesPerBlock(DXGI_FORMAT fmt)
{
    switch (fmt) {
    case DXGI_FORMAT_BC1_UNORM: return 8;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC3_UNORM: return 16;
    default: return 0;
    }
}

struct TextureDesc {
    UINT32 pitch = 0;
    UINT32 mipmapsCount = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    UINT32 width = 0;
    UINT32 height = 0;
    void* pData = nullptr;
};

static bool LoadDDS(const wchar_t* filename, TextureDesc& desc, bool singleMip = false)
{
    FILE* f = nullptr;
    _wfopen_s(&f, filename, L"rb");
    if (!f) return false;

    UINT32 magic;
    fread(&magic, 4, 1, f);
    if (magic != 0x20534444) { fclose(f); return false; }

    DDS_HEADER header;
    fread(&header, sizeof(header), 1, f);

    desc.width = header.dwWidth;
    desc.height = header.dwHeight;
    desc.mipmapsCount = singleMip ? 1 : (std::max)(1u, header.dwMipMapCount);
    desc.fmt = DXGI_FORMAT_UNKNOWN;

    if (header.ddspf.dwFlags & DDPF_FOURCC) {
        UINT32 fourcc = header.ddspf.dwFourCC;
        if (fourcc == MAKEFOURCC('D','X','T','1')) desc.fmt = DXGI_FORMAT_BC1_UNORM;
        else if (fourcc == MAKEFOURCC('D','X','T','3')) desc.fmt = DXGI_FORMAT_BC2_UNORM;
        else if (fourcc == MAKEFOURCC('D','X','T','5')) desc.fmt = DXGI_FORMAT_BC3_UNORM;
    }
    else if ((header.ddspf.dwFlags & DDPF_RGB) && header.ddspf.dwRGBBitCount == 32) {
        desc.fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (desc.fmt == DXGI_FORMAT_UNKNOWN) { fclose(f); return false; }

    bool isBC = (desc.fmt == DXGI_FORMAT_BC1_UNORM || desc.fmt == DXGI_FORMAT_BC2_UNORM || desc.fmt == DXGI_FORMAT_BC3_UNORM);
    UINT32 totalSize = 0;
    UINT32 w = desc.width, h = desc.height;
    for (UINT32 i = 0; i < desc.mipmapsCount; i++) {
        if (isBC) {
            totalSize += DivUp(w, 4u) * DivUp(h, 4u) * GetBytesPerBlock(desc.fmt);
        } else {
            totalSize += w * h * 4;
        }
        w = (std::max)(1u, w / 2);
        h = (std::max)(1u, h / 2);
    }

    desc.pData = malloc(totalSize);
    fread(desc.pData, 1, totalSize, f);
    fclose(f);
    return true;
}

static void FreeTextureData(TextureDesc& desc)
{
    if (desc.pData) { free(desc.pData); desc.pData = nullptr; }
}

struct TextureVertex {
    float x, y, z;
    float tx, ty, tz;
    float nx, ny, nz;
    float u, v;
};

struct SkyboxVertex {
    float x, y, z;
};

static const TextureVertex CubeVertices[24] = {
    // Front face (+Z)
    {-0.5f, -0.5f,  0.5f,  1,0,0,  0,0,1,  0, 1},
    { 0.5f, -0.5f,  0.5f,  1,0,0,  0,0,1,  1, 1},
    { 0.5f,  0.5f,  0.5f,  1,0,0,  0,0,1,  1, 0},
    {-0.5f,  0.5f,  0.5f,  1,0,0,  0,0,1,  0, 0},
    // Back face (-Z)
    { 0.5f, -0.5f, -0.5f, -1,0,0,  0,0,-1,  0, 1},
    {-0.5f, -0.5f, -0.5f, -1,0,0,  0,0,-1,  1, 1},
    {-0.5f,  0.5f, -0.5f, -1,0,0,  0,0,-1,  1, 0},
    { 0.5f,  0.5f, -0.5f, -1,0,0,  0,0,-1,  0, 0},
    // Top face (+Y)
    {-0.5f,  0.5f,  0.5f,  1,0,0,  0,1,0,  0, 1},
    { 0.5f,  0.5f,  0.5f,  1,0,0,  0,1,0,  1, 1},
    { 0.5f,  0.5f, -0.5f,  1,0,0,  0,1,0,  1, 0},
    {-0.5f,  0.5f, -0.5f,  1,0,0,  0,1,0,  0, 0},
    // Bottom face (-Y)
    {-0.5f, -0.5f, -0.5f,  1,0,0,  0,-1,0,  0, 1},
    { 0.5f, -0.5f, -0.5f,  1,0,0,  0,-1,0,  1, 1},
    { 0.5f, -0.5f,  0.5f,  1,0,0,  0,-1,0,  1, 0},
    {-0.5f, -0.5f,  0.5f,  1,0,0,  0,-1,0,  0, 0},
    // Left face (-X)
    {-0.5f, -0.5f, -0.5f,  0,0,1,  -1,0,0,  0, 1},
    {-0.5f, -0.5f,  0.5f,  0,0,1,  -1,0,0,  1, 1},
    {-0.5f,  0.5f,  0.5f,  0,0,1,  -1,0,0,  1, 0},
    {-0.5f,  0.5f, -0.5f,  0,0,1,  -1,0,0,  0, 0},
    // Right face (+X)
    { 0.5f, -0.5f,  0.5f,  0,0,-1,  1,0,0,  0, 1},
    { 0.5f, -0.5f, -0.5f,  0,0,-1,  1,0,0,  1, 1},
    { 0.5f,  0.5f, -0.5f,  0,0,-1,  1,0,0,  1, 0},
    { 0.5f,  0.5f,  0.5f,  0,0,-1,  1,0,0,  0, 0},
};

static const UINT16 CubeIndices[36] = {
    0, 2, 1, 0, 3, 2,
    4, 6, 5, 4, 7, 6,
    8, 10, 9, 8, 11, 10,
    12, 14, 13, 12, 15, 14,
    16, 18, 17, 16, 19, 18,
    20, 22, 21, 20, 23, 22
};

static const SkyboxVertex PlaneVertices[4] = {
    {-1.0f, -1.0f, 0.0f},
    { 1.0f, -1.0f, 0.0f},
    { 1.0f,  1.0f, 0.0f},
    {-1.0f,  1.0f, 0.0f},
};

static const UINT16 PlaneIndices[6] = {
    0, 2, 1, 0, 3, 2
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
        desc.ByteWidth = sizeof(CubeVertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = CubeVertices;

        if (FAILED(m_device->CreateBuffer(&desc, &data, m_vertexBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_vertexBuffer.Get(), "VertexBuffer");
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(CubeIndices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = CubeIndices;

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
        desc.ByteWidth = sizeof(GeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(m_device->CreateBuffer(&desc, nullptr, m_geomBuffer2.GetAddressOf())))
            return false;
        SetResourceName(m_geomBuffer2.Get(), "GeomBuffer2");
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);

    TextureDesc texDesc;
    if (!LoadDDS((dir + L"Brick.dds").c_str(), texDesc))
        return false;

    DXGI_FORMAT textureFmt = texDesc.fmt;
    UINT32 mipLevels = texDesc.mipmapsCount;
    bool isBC = (texDesc.fmt == DXGI_FORMAT_BC1_UNORM || texDesc.fmt == DXGI_FORMAT_BC2_UNORM || texDesc.fmt == DXGI_FORMAT_BC3_UNORM);

    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Format = texDesc.fmt;
        td.ArraySize = 1;
        td.MipLevels = texDesc.mipmapsCount;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.SampleDesc.Count = 1;
        td.Width = texDesc.width;
        td.Height = texDesc.height;

        std::vector<D3D11_SUBRESOURCE_DATA> subData(td.MipLevels);
        const char* pSrc = reinterpret_cast<const char*>(texDesc.pData);
        UINT32 w = td.Width, h = td.Height;
        for (UINT32 i = 0; i < td.MipLevels; i++) {
            UINT32 pitch;
            UINT32 sliceSize;
            if (isBC) {
                UINT32 bw = DivUp(w, 4u);
                UINT32 bh = DivUp(h, 4u);
                pitch = bw * GetBytesPerBlock(td.Format);
                sliceSize = pitch * bh;
            } else {
                pitch = w * 4;
                sliceSize = pitch * h;
            }
            subData[i].pSysMem = pSrc;
            subData[i].SysMemPitch = pitch;
            subData[i].SysMemSlicePitch = 0;
            pSrc += sliceSize;
            w = (std::max)(1u, w / 2);
            h = (std::max)(1u, h / 2);
        }
        if (FAILED(m_device->CreateTexture2D(&td, subData.data(), m_texture.GetAddressOf())))
            return false;
        FreeTextureData(texDesc);
    }

    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = textureFmt;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = mipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        if (FAILED(m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_textureView.GetAddressOf())))
            return false;
    }

    {
        TextureDesc nmDesc;
        if (!LoadDDS((dir + L"BrickNM.dds").c_str(), nmDesc))
            return false;

        DXGI_FORMAT nmFmt = nmDesc.fmt;
        UINT32 nmMips = nmDesc.mipmapsCount;
        bool nmBC = (nmFmt == DXGI_FORMAT_BC1_UNORM || nmFmt == DXGI_FORMAT_BC2_UNORM || nmFmt == DXGI_FORMAT_BC3_UNORM);

        D3D11_TEXTURE2D_DESC td = {};
        td.Format = nmFmt;
        td.ArraySize = 1;
        td.MipLevels = nmMips;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.SampleDesc.Count = 1;
        td.Width = nmDesc.width;
        td.Height = nmDesc.height;

        std::vector<D3D11_SUBRESOURCE_DATA> subData(td.MipLevels);
        const char* pSrc = reinterpret_cast<const char*>(nmDesc.pData);
        UINT32 w = td.Width, h = td.Height;
        for (UINT32 i = 0; i < td.MipLevels; i++) {
            UINT32 pitch, sliceSize;
            if (nmBC) {
                UINT32 bw = DivUp(w, 4u);
                UINT32 bh = DivUp(h, 4u);
                pitch = bw * GetBytesPerBlock(td.Format);
                sliceSize = pitch * bh;
            } else {
                pitch = w * 4;
                sliceSize = pitch * h;
            }
            subData[i].pSysMem = pSrc;
            subData[i].SysMemPitch = pitch;
            subData[i].SysMemSlicePitch = 0;
            pSrc += sliceSize;
            w = (std::max)(1u, w / 2);
            h = (std::max)(1u, h / 2);
        }
        if (FAILED(m_device->CreateTexture2D(&td, subData.data(), m_normalMap.GetAddressOf())))
            return false;
        FreeTextureData(nmDesc);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = nmFmt;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = nmMips;
        srvDesc.Texture2D.MostDetailedMip = 0;
        if (FAILED(m_device->CreateShaderResourceView(m_normalMap.Get(), &srvDesc, m_normalMapView.GetAddressOf())))
            return false;
    }

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
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(m_device->CreateInputLayout(InputDesc, 4,
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

bool Dx11App::InitSkybox()
{
    const UINT latSteps = 16;
    const UINT lonSteps = 32;

    std::vector<SkyboxVertex> verts;
    std::vector<UINT16> indices;

    for (UINT lat = 0; lat <= latSteps; lat++) {
        float theta = lat * DirectX::XM_PI / latSteps;
        for (UINT lon = 0; lon <= lonSteps; lon++) {
            float phi = lon * DirectX::XM_2PI / lonSteps;
            SkyboxVertex v;
            v.x = sinf(theta) * cosf(phi);
            v.y = cosf(theta);
            v.z = sinf(theta) * sinf(phi);
            verts.push_back(v);
        }
    }

    for (UINT lat = 0; lat < latSteps; lat++) {
        for (UINT lon = 0; lon < lonSteps; lon++) {
            UINT16 a = (UINT16)(lat * (lonSteps + 1) + lon);
            UINT16 b = (UINT16)(a + lonSteps + 1);
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back((UINT16)(a + 1));
            indices.push_back((UINT16)(a + 1));
            indices.push_back(b);
            indices.push_back((UINT16)(b + 1));
        }
    }
    m_skyboxIndexCount = (UINT)indices.size();

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)(verts.size() * sizeof(SkyboxVertex));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = verts.data();

        if (FAILED(m_device->CreateBuffer(&desc, &data, m_skyboxVB.GetAddressOf())))
            return false;
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = (UINT)(indices.size() * sizeof(UINT16));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = indices.data();

        if (FAILED(m_device->CreateBuffer(&desc, &data, m_skyboxIB.GetAddressOf())))
            return false;
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(SkyboxGeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(m_device->CreateBuffer(&desc, nullptr, m_skyboxGeomBuffer.GetAddressOf())))
            return false;
        SetResourceName(m_skyboxGeomBuffer.Get(), "SkyboxGeomBuffer");
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);

    const std::wstring faceNames[6] = {
        dir + L"right.dds", dir + L"left.dds",
        dir + L"up.dds", dir + L"down.dds",
        dir + L"front.dds", dir + L"back.dds"
    };

    TextureDesc texDescs[6];
    bool ddsOk = true;
    for (int i = 0; i < 6 && ddsOk; i++)
        ddsOk = LoadDDS(faceNames[i].c_str(), texDescs[i], true);

    if (!ddsOk) {
        for (int i = 0; i < 6; i++) FreeTextureData(texDescs[i]);
        return false;
    }

    DXGI_FORMAT cubeFmt = texDescs[0].fmt;
    bool isBC = (cubeFmt == DXGI_FORMAT_BC1_UNORM || cubeFmt == DXGI_FORMAT_BC2_UNORM || cubeFmt == DXGI_FORMAT_BC3_UNORM);

    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Format = cubeFmt;
        td.ArraySize = 6;
        td.MipLevels = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        td.SampleDesc.Count = 1;
        td.Width = texDescs[0].width;
        td.Height = texDescs[0].height;

        D3D11_SUBRESOURCE_DATA subData[6] = {};
        for (int i = 0; i < 6; i++) {
            subData[i].pSysMem = texDescs[i].pData;
            if (isBC)
                subData[i].SysMemPitch = DivUp(td.Width, 4u) * GetBytesPerBlock(td.Format);
            else
                subData[i].SysMemPitch = td.Width * 4;
        }
        if (FAILED(m_device->CreateTexture2D(&td, subData, m_cubemapTexture.GetAddressOf()))) {
            for (int i = 0; i < 6; i++) FreeTextureData(texDescs[i]);
            return false;
        }
        for (int i = 0; i < 6; i++) FreeTextureData(texDescs[i]);
    }

    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = cubeFmt;
        srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        if (FAILED(m_device->CreateShaderResourceView(m_cubemapTexture.Get(), &srvDesc, m_cubemapView.GetAddressOf())))
            return false;
    }

    ID3DBlob* pVSCode = nullptr;
    if (!CompileShader(dir + L"skybox.vs", "vs", "vs_5_0", &pVSCode))
        return false;

    if (FAILED(m_device->CreateVertexShader(pVSCode->GetBufferPointer(),
        pVSCode->GetBufferSize(), nullptr, m_skyboxVS.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC SkyboxInputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(m_device->CreateInputLayout(SkyboxInputDesc, 1,
        pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(),
        m_skyboxInputLayout.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }
    pVSCode->Release();

    ID3DBlob* pPSCode = nullptr;
    if (!CompileShader(dir + L"skybox.ps", "ps", "ps_5_0", &pPSCode))
        return false;

    if (FAILED(m_device->CreatePixelShader(pPSCode->GetBufferPointer(),
        pPSCode->GetBufferSize(), nullptr, m_skyboxPS.GetAddressOf())))
    {
        pPSCode->Release();
        return false;
    }
    pPSCode->Release();

    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

        if (FAILED(m_device->CreateDepthStencilState(&dsDesc, m_skyboxDSS.GetAddressOf())))
            return false;
    }

    return true;
}

bool Dx11App::InitTransparent()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(PlaneVertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = PlaneVertices;
        if (FAILED(m_device->CreateBuffer(&desc, &data, m_transVB.GetAddressOf())))
            return false;
    }

    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(PlaneIndices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = PlaneIndices;
        if (FAILED(m_device->CreateBuffer(&desc, &data, m_transIB.GetAddressOf())))
            return false;
    }

    for (int i = 0; i < 2; i++) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(TransGeomBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(m_device->CreateBuffer(&desc, nullptr, m_transGeomBuffer[i].GetAddressOf())))
            return false;
    }

    ID3DBlob* pVSCode = nullptr;
    if (!CompileShader(dir + L"transparent.vs", "vs", "vs_5_0", &pVSCode))
        return false;

    if (FAILED(m_device->CreateVertexShader(pVSCode->GetBufferPointer(),
        pVSCode->GetBufferSize(), nullptr, m_transVS.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC TransInputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(m_device->CreateInputLayout(TransInputDesc, 1,
        pVSCode->GetBufferPointer(), pVSCode->GetBufferSize(),
        m_transInputLayout.GetAddressOf())))
    {
        pVSCode->Release();
        return false;
    }
    pVSCode->Release();

    ID3DBlob* pPSCode = nullptr;
    if (!CompileShader(dir + L"transparent.ps", "ps", "ps_5_0", &pPSCode))
        return false;

    if (FAILED(m_device->CreatePixelShader(pPSCode->GetBufferPointer(),
        pPSCode->GetBufferSize(), nullptr, m_transPS.GetAddressOf())))
    {
        pPSCode->Release();
        return false;
    }
    pPSCode->Release();

    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;
        dsDesc.StencilEnable = FALSE;
        if (FAILED(m_device->CreateDepthStencilState(&dsDesc, m_transDepthState.GetAddressOf())))
            return false;
    }

    {
        D3D11_BLEND_DESC desc = {};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED |
            D3D11_COLOR_WRITE_ENABLE_GREEN |
            D3D11_COLOR_WRITE_ENABLE_BLUE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        if (FAILED(m_device->CreateBlendState(&desc, m_transBlendState.GetAddressOf())))
            return false;
    }

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
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MinLOD = -FLT_MAX;
        desc.MaxLOD = FLT_MAX;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 16;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

        if (FAILED(m_device->CreateSamplerState(&desc, m_sampler.GetAddressOf())))
            return false;
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

    if (!InitGeometry())
        return false;

    if (!InitSkybox())
        return false;

    {
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;
        dsDesc.StencilEnable = FALSE;
        if (FAILED(m_device->CreateDepthStencilState(&dsDesc, m_opaqueDepthState.GetAddressOf())))
            return false;
    }

    if (!InitTransparent())
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
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
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

    float radius = 3.0f;
    float eyeX = radius * sinf(m_cameraYaw) * cosf(m_cameraPitch);
    float eyeY = radius * sinf(m_cameraPitch);
    float eyeZ = -radius * cosf(m_cameraYaw) * cosf(m_cameraPitch);
    DirectX::XMVECTOR eye = DirectX::XMVectorSet(eyeX, eyeY, eyeZ, 1.0f);
    DirectX::XMVECTOR at = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX v = DirectX::XMMatrixLookAtLH(eye, at, up);

    float n = 0.1f, f = 100.0f;
    float fov = DirectX::XM_PI / 3.0f;
    float aspect = (float)m_width / (float)m_height;
    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, f, n);
    DirectX::XMMATRIX vpMatrix = DirectX::XMMatrixMultiply(v, p);

    {
        GeomBuffer gb;
        gb.model = DirectX::XMMatrixRotationY(elapsed * DirectX::XM_PI * 0.15f);
        gb.shine = DirectX::XMFLOAT4(32.0f, 0, 0, 0);
        m_context->UpdateSubresource(m_geomBuffer.Get(), 0, nullptr, &gb, 0, 0);
    }

    {
        D3D11_MAPPED_SUBRESOURCE sub;
        if (SUCCEEDED(m_context->Map(m_sceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sub)))
        {
            SceneBuffer& sb = *reinterpret_cast<SceneBuffer*>(sub.pData);
            sb.vp = vpMatrix;
            sb.cameraPos = DirectX::XMFLOAT4(eyeX, eyeY, eyeZ, 1.0f);
            sb.lightCount = DirectX::XMINT4(2, 0, 0, 0);
            sb.lights[0].pos = DirectX::XMFLOAT4(0.6f, 0.6f, 0.6f, 0.0f);
            sb.lights[0].color = DirectX::XMFLOAT4(2.0f, 2.0f, 1.6f, 0.0f);
            sb.lights[1].pos = DirectX::XMFLOAT4(-0.6f, 0.4f, -0.4f, 0.0f);
            sb.lights[1].color = DirectX::XMFLOAT4(1.0f, 1.0f, 2.0f, 0.0f);
            sb.ambientColor = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
            m_context->Unmap(m_sceneBuffer.Get(), 0);
        }
    }

    {
        float halfH = n * tanf(fov / 2.0f);
        float halfW = halfH * aspect;
        float skyboxSize = sqrtf(n * n + halfH * halfH + halfW * halfW) * 1.01f;

        SkyboxGeomBuffer sgb;
        sgb.model = DirectX::XMMatrixIdentity();
        sgb.size = DirectX::XMFLOAT4(skyboxSize, 0, 0, 0);
        m_context->UpdateSubresource(m_skyboxGeomBuffer.Get(), 0, nullptr, &sgb, 0, 0);
    }

    m_context->ClearState();

    ID3D11RenderTargetView* views[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, views, m_dsv.Get());

    const float clearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);

    m_context->RSSetViewports(1, &m_viewport);
    m_context->RSSetState(m_rasterizerState.Get());

    ID3D11SamplerState* samplers[] = { m_sampler.Get() };

    // Opaque pass
    {
        m_context->OMSetDepthStencilState(m_opaqueDepthState.Get(), 0);
        m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        ID3D11Buffer* vbs[] = { m_vertexBuffer.Get() };
        UINT strides[] = { sizeof(TextureVertex) };
        UINT offsets[] = { 0 };
        m_context->IASetVertexBuffers(0, 1, vbs, strides, offsets);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        m_context->PSSetSamplers(0, 1, samplers);
        ID3D11ShaderResourceView* cubeSRVs[] = { m_textureView.Get(), m_normalMapView.Get() };
        m_context->PSSetShaderResources(0, 2, cubeSRVs);

        ID3D11Buffer* cubeCBs1[] = { m_geomBuffer.Get(), m_sceneBuffer.Get() };
        m_context->VSSetConstantBuffers(0, 2, cubeCBs1);
        m_context->PSSetConstantBuffers(0, 2, cubeCBs1);
        m_context->DrawIndexed(36, 0, 0);
    }

    // Skybox pass
    {
        m_context->OMSetDepthStencilState(m_skyboxDSS.Get(), 0);
        m_context->IASetIndexBuffer(m_skyboxIB.Get(), DXGI_FORMAT_R16_UINT, 0);
        ID3D11Buffer* vbs[] = { m_skyboxVB.Get() };
        UINT strides[] = { sizeof(SkyboxVertex) };
        UINT offsets[] = { 0 };
        m_context->IASetVertexBuffers(0, 1, vbs, strides, offsets);
        m_context->IASetInputLayout(m_skyboxInputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_skyboxVS.Get(), nullptr, 0);
        m_context->PSSetShader(m_skyboxPS.Get(), nullptr, 0);

        ID3D11Buffer* skyboxCBs[] = { m_sceneBuffer.Get(), m_skyboxGeomBuffer.Get() };
        m_context->VSSetConstantBuffers(0, 2, skyboxCBs);

        m_context->PSSetSamplers(0, 1, samplers);
        ID3D11ShaderResourceView* skyboxSRVs[] = { m_cubemapView.Get() };
        m_context->PSSetShaderResources(0, 1, skyboxSRVs);

        m_context->DrawIndexed(m_skyboxIndexCount, 0, 0);
    }

    m_swapChain->Present(1, 0);
}

void Dx11App::Cleanup()
{
    m_transBlendState.Reset();
    m_transDepthState.Reset();
    for (int i = 0; i < 2; i++) m_transGeomBuffer[i].Reset();
    m_transInputLayout.Reset();
    m_transPS.Reset();
    m_transVS.Reset();
    m_transIB.Reset();
    m_transVB.Reset();
    m_opaqueDepthState.Reset();
    m_geomBuffer2.Reset();
    m_skyboxDSS.Reset();
    m_cubemapView.Reset();
    m_cubemapTexture.Reset();
    m_skyboxInputLayout.Reset();
    m_skyboxPS.Reset();
    m_skyboxVS.Reset();
    m_skyboxGeomBuffer.Reset();
    m_skyboxIB.Reset();
    m_skyboxVB.Reset();
    m_textureView.Reset();
    m_texture.Reset();
    m_normalMapView.Reset();
    m_normalMap.Reset();
    m_sampler.Reset();
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
