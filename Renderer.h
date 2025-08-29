#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

class Renderer
{
public:
	Renderer();
	~Renderer();

	HRESULT Init(HWND hWnd);
	void Update();
	void Render();
	void Cleanup();

private:
	HRESULT InitDevice(HWND hWnd);
	HRESULT InitShaders();
	HRESULT InitBuffers();

	ID3D11Device* m_pd3dDevice;
	ID3D11DeviceContext* m_pd3dImmediateContext;
	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pRenderTargetView;
	ID3D11Texture2D* m_pDepthStencil;
	ID3D11DepthStencilView* m_pDepthStencilView;
	ID3D11DepthStencilState* m_pDepthStencilState;
	ID3D11RasterizerState* m_pRasterizerState;
	ID3D11BlendState* m_pBlendState;
	ID3D11InputLayout* m_pVertexLayout;
	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
	ID3D11Buffer* m_pConstantBuffer;

	ID3D11ShaderResourceView* m_pTextureRV;
	ID3D11SamplerState* m_pSamplerLinear;

	DirectX::XMMATRIX m_World;
	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;
};

