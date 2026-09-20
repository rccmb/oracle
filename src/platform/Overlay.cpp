#include "platform/Overlay.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
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
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        className,
        L"Oracle Overlay",
        WS_POPUP,
        g_virtualScreen.left, g_virtualScreen.top,
        g_virtualScreen.right - g_virtualScreen.left,
        g_virtualScreen.bottom - g_virtualScreen.top,
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // Take this window out of screen capture.
    //
    // Capture blits from the screen device context, which composites every
    // window including this one, so Oracle was reading its own drawing back as
    // if it were the board: an arrowhead over a square hid the piece standing
    // there, that square then read as empty, and the suggestion that drew the
    // arrow destroyed the evidence for itself. Measured at 100% of the overlay's
    // pixels landing in the capture before this call and none after it.
    //
    // WDA_EXCLUDEFROMCAPTURE needs Windows 10 2004. Resolved at runtime so the
    // build does not depend on a particular SDK, and reported rather than
    // assumed, because without it the overlay has to stay clear of the pieces.
    using SetAffinityFn = BOOL(WINAPI*)(HWND, DWORD);
    g_overlayHiddenFromCapture = false;
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto setAffinity = (SetAffinityFn)GetProcAddress(user32, "SetWindowDisplayAffinity");
        if (setAffinity && setAffinity(hwnd, 0x00000011 /* WDA_EXCLUDEFROMCAPTURE */)) {
            g_overlayHiddenFromCapture = true;
        }
    }

    if (!g_overlayHiddenFromCapture) {
        std::cerr << "[WARN] The overlay could not be excluded from screen capture. "
                     "Detection may read Oracle's own drawing as part of the board.\n";
    }

    ShowWindow(hwnd, SW_SHOW);

    return hwnd;
}