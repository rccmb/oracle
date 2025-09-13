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

/**
 * @brief Creates Direct3D device and swap chain for rendering.
 * 
 * @param hWnd Handle to the window for rendering output.
 * 
 * @return True if creation succeeds, false otherwise.
 */
bool CreateDeviceD3D(HWND hWnd);

/**
 * @brief Creates a render target view from the swap chain's back buffer.
 * 
 * @param None.
 * 
 * @return None.
 */
void CreateRenderTarget();

/**
 * @brief Releases the render target view.
 * 
 * @param None.
 * 
 * @return None.
 */
void CleanupRenderTarget();

/**
 * @brief Cleans up Direct3D resources (render target, swap chain, device, and context).
 * 
 * @param None.
 * 
 * @return None.
 */
void CleanupDirect3D();