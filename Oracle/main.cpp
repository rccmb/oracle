#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

 #include "Utils.h"
 #include "Overlay.h"
 #include "Vision.h"

int main() {
	// Initialize overlay window.
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const LPCWSTR className = L"Oracle Overlay";
    HWND hwndOverlay = CreateOverlayWindow(hInstance, className, WndProc);

	// Get chessboard color coding.
    HWND hwndDesktop = GetDesktopWindow();
	std::pair<CLICK, CLICK> clicks = GetChessboardColorCoding(hwndDesktop);
    SetChessboardClicks(clicks);

    // Create ChessboardDetectionThread.
    CreateThread(NULL, 0, ChessboardDetectionThread, hwndOverlay, 0, NULL);

    // Message loop.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
