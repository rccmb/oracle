#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern ID3D11RenderTargetView* g_mainRenderTargetView;
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;

bool CreateDeviceD3D(HWND hWnd);

void CreateRenderTarget();

void CleanupRenderTarget();

void CleanupDirect3D();