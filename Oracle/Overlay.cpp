#include "Overlay.h"

std::vector<RECT> g_candidateRects; // Chessboard candidate rectangles.
RECT g_bestRect = { 0, 0, 0, 0 }; // Best chessboard candidate rectangle.

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Clean previous.
        HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, clearBrush);
        DeleteObject(clearBrush);

		// [DEBUG] Draw candidates.
        /*
        HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
        for (const auto& r : g_candidateRects) {
            FrameRect(hdc, &r, redBrush);
        }
        DeleteObject(redBrush);
        */

        // [DEBUG] Draw chessboard.
        /*
        if ((g_bestRect.right - g_bestRect.left) > 0 && (g_bestRect.bottom - g_bestRect.top) > 0) {
            HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
            FrameRect(hdc, &g_bestRect, greenBrush);
            DeleteObject(greenBrush);
        }
        */

        EndPaint(hwnd, &ps);
    } break;

    case WM_CHESSBOARD_CANDIDATES: {
        InvalidateRect(hwnd, NULL, TRUE);
    } break;

    case WM_CHESSBOARD_FOUND: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int w = LOWORD(wParam);
        int h = HIWORD(wParam);

        g_bestRect = { x, y, x + w, y + h };
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

HWND CreateOverlayWindow(HINSTANCE hInstance, const LPCWSTR className, WNDPROC wndProc) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
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

void ClearCandidateRectangles() {
    g_candidateRects.clear();
}

void AddCandidateRectangle(RECT rect) {
    g_candidateRects.push_back(rect);
}

void ClearBestRectangle() {
    g_bestRect = { 0, 0, 0, 0 };
}

void SetBestRectangle(RECT rect) {
    g_bestRect = rect;
}