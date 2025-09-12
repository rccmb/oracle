#include "Overlay.h"

RECT g_boardRect = { 0, 0, 0, 0 }; // Chessboard rectangle.

std::vector<SAMPLE> g_debugSamples;

// ImGui parameters for debugging ROI selection.
int g_debugROI_x = 0;
int g_debugROI_y = 0;
int g_debugPatchSize = 6;
int g_debugOffset = 0;

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Clean previous.
        HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, clearBrush);
        DeleteObject(clearBrush);

        if ((g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
            HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
            FrameRect(hdc, &g_boardRect, greenBrush);
            DeleteObject(greenBrush);
        }

		// Draw debug points.
        if (!g_debugSamples.empty()) {
            HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
            for (SAMPLE s : g_debugSamples) {
                std::cout << "[DEBUG] Drawing sample at (" << s.x << ", " << s.y << ") size (" << s.width << "x" << s.height << ")\n";
                RECT r = { s.x, s.y, s.x + s.width, s.y + s.height };
                FillRect(hdc, &r, redBrush);
            }
            DeleteObject(redBrush);
        }
        
        EndPaint(hwnd, &ps);
    } break;

    case WM_CHESSBOARD_DETECTED: {
        InvalidateRect(hwnd, NULL, TRUE);
    } break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

HWND CreateOverlayWindow(HINSTANCE hInstance, const LPCWSTR className) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        className,
        L"Oracle Overlay",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(hwnd, SW_SHOW);

    return hwnd;
}

void AddDebugSample(SAMPLE sample) {
    g_debugSamples.push_back(sample);
}

void ClearboardRectangle() {
    g_boardRect = { 0, 0, 0, 0 };
}

void SetboardRectangle(RECT rect) {
    g_boardRect = rect;
}