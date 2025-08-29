#include "Renderer.h"
#include <d3dcompiler.h>
#include "WICTextureLoader.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};

const char* g_szVertexShader = R"(
cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(input.Pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    output.Color = input.Color;
    output.Tex = input.Tex;
    return output;
}
)";

const char* g_szPixelShader = R"(
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_Target
{
    return txDiffuse.Sample(samLinear, input.Tex) * input.Color;
}
)";

struct SimpleVertex {
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
	XMFLOAT2 TexCoord;
};

SimpleVertex vertices[] = {
    { XMFLOAT3(0.0f,  0.5f,  0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.5f, 0.0f) }, // Top
    { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }, // Front Right
    { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) }, // Back Right
    { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }, // Back Left
    { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }, // Front Left
};

WORD indices[] = {
    0, 2, 1,
    0, 3, 2,
    0, 4, 3,
    0, 1, 4,
    1, 2, 3,
    1, 3, 4
};

Renderer::Renderer()
    : m_pd3dDevice(nullptr), m_pd3dImmediateContext(nullptr), m_pSwapChain(nullptr),
    m_pRenderTargetView(nullptr), m_pDepthStencil(nullptr), m_pDepthStencilView(nullptr), m_pDepthStencilState(nullptr), m_pRasterizerState(nullptr), m_pVertexLayout(nullptr), m_pVertexShader(nullptr),
    m_pPixelShader(nullptr), m_pVertexBuffer(nullptr), m_pIndexBuffer(nullptr), m_pConstantBuffer(nullptr),
    m_pTextureRV(nullptr), m_pSamplerLinear(nullptr)
{
    m_World = XMMatrixIdentity();
    m_View = XMMatrixIdentity();
    m_Projection = XMMatrixIdentity();
}

Renderer::~Renderer() {
    Cleanup();
}

HRESULT Renderer::Init(HWND hWnd) {
    HRESULT hr = InitDevice(hWnd);
    if (FAILED(hr)) return hr;

    hr = InitShaders();
    if (FAILED(hr)) return hr;

    hr = InitBuffers();
    if (FAILED(hr)) return hr;

    // Load the Texture
    hr = CreateWICTextureFromFile(m_pd3dDevice, L"Nareko.png", nullptr, &m_pTextureRV);
    if (FAILED(hr))
        return hr;

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_pd3dDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
    if (FAILED(hr))
        return hr;

	return S_OK;
}

HRESULT Renderer::InitDevice(HWND hWnd) {
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, driverTypes[driverTypeIndex], nullptr, createDeviceFlags,
            featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd,
            &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dImmediateContext);
        if (SUCCEEDED(hr)) break;
    }
    if (FAILED(hr)) return hr;

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return hr;

    hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    if (pBackBuffer) pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = m_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &m_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = m_pd3dDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    m_pd3dImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pd3dImmediateContext->RSSetViewports(1, &vp);

    // Create the depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = false;
    m_pd3dDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStencilState);
    m_pd3dImmediateContext->OMSetDepthStencilState(m_pDepthStencilState, 1);

    // Create the rasterizer state
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = false;
    rsDesc.DepthBias = 0;
    rsDesc.SlopeScaledDepthBias = 0.0f;
    rsDesc.DepthBiasClamp = 0.0f;
    rsDesc.DepthClipEnable = true;
    rsDesc.ScissorEnable = false;
    rsDesc.MultisampleEnable = false;
    rsDesc.AntialiasedLineEnable = false;
    m_pd3dDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerState);
    m_pd3dImmediateContext->RSSetState(m_pRasterizerState);

    // Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 0.3f, -1.5f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_View = XMMatrixLookAtLH(Eye, At, Up);

    // Initialize the projection matrix
	m_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);

    return S_OK;
}

HRESULT Renderer::InitShaders() {
    HRESULT hr = S_OK;

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;

    hr = D3DCompile(g_szVertexShader, strlen(g_szVertexShader), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &pVSBlob, nullptr);
    if (FAILED(hr)) return hr;

    hr = m_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return hr; }

    hr = D3DCompile(g_szPixelShader, strlen(g_szPixelShader), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pPSBlob, nullptr);
    if (FAILED(hr)) { pVSBlob->Release(); return hr; }

    hr = m_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return hr; }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    UINT numElements = ARRAYSIZE(layout);
    hr = m_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pVertexLayout);

    pVSBlob->Release();
    if (pPSBlob) pPSBlob->Release();

    return hr;
}

HRESULT Renderer::InitBuffers() {
	HRESULT hr = S_OK;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * ARRAYSIZE(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = m_pd3dDevice->CreateBuffer(&bd, &InitData, &m_pVertexBuffer);
    if (FAILED(hr)) return hr;

    bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    InitData.pSysMem = indices;
    hr = m_pd3dDevice->CreateBuffer(&bd, &InitData, &m_pIndexBuffer);
    if (FAILED(hr)) return hr;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pConstantBuffer);
    if (FAILED(hr)) return hr;

    return hr;
}

void Renderer::Update() {
    // Rotate the cube
	static float t = 0.0f;
    t += (float)XM_PI * 0.00005f;
	m_World = XMMatrixRotationY(t);
}

void Renderer::Render() {
    if (!m_pd3dImmediateContext) return;

    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView, ClearColor);
    m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(m_World);
	cb.mView = XMMatrixTranspose(m_View);
	cb.mProjection = XMMatrixTranspose(m_Projection);
	m_pd3dImmediateContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    m_pd3dImmediateContext->IASetInputLayout(m_pVertexLayout);
    m_pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pd3dImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pd3dImmediateContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
    m_pd3dImmediateContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    m_pd3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

    m_pd3dImmediateContext->DrawIndexed(ARRAYSIZE(indices), 0, 0);

    m_pSwapChain->Present(0, 0);
}

void Renderer::Cleanup() {
    if (m_pd3dImmediateContext) m_pd3dImmediateContext->ClearState();

    if (m_pSamplerLinear) m_pSamplerLinear->Release();
    if (m_pTextureRV) m_pTextureRV->Release();
    if (m_pConstantBuffer) m_pConstantBuffer->Release();
    if (m_pIndexBuffer) m_pIndexBuffer->Release();
    if (m_pVertexBuffer) m_pVertexBuffer->Release();
    if (m_pVertexLayout) m_pVertexLayout->Release();
    if (m_pVertexShader) m_pVertexShader->Release();
    if (m_pPixelShader) m_pPixelShader->Release();
    if (m_pRasterizerState) m_pRasterizerState->Release();
    if (m_pDepthStencilState) m_pDepthStencilState->Release();
    if (m_pDepthStencilView) m_pDepthStencilView->Release();
    if (m_pDepthStencil) m_pDepthStencil->Release();
    if (m_pRenderTargetView) m_pRenderTargetView->Release();
    if (m_pSwapChain) m_pSwapChain->Release();
    if (m_pd3dImmediateContext) m_pd3dImmediateContext->Release();
    if (m_pd3dDevice) m_pd3dDevice->Release();
}