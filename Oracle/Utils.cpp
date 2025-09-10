#include "Utils.h"

cv::Mat HWND2MAT(HWND hwnd) {
    HDC hwindowDC = GetDC(hwnd);
    HDC hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);

    RECT windowsize;
    GetClientRect(hwnd, &windowsize);
    int width = windowsize.right;
    int height = windowsize.bottom;

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(BITMAPINFO));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;  
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;    
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbwindow = CreateDIBSection(hwindowCompatibleDC, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    SelectObject(hwindowCompatibleDC, hbwindow);

    BitBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, SRCCOPY);

    cv::Mat src(height, width, CV_8UC4, pBits);

    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);

    cv::Mat result = gray.clone(); 

    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);

    return result; 
}

std::pair<CLICK, CLICK> GetChessboardColorCoding(HWND hwnd) {
    std::cout << "Waiting for clicks..." << std::endl;

    CLICK first{ -1, -1, 0 };
    CLICK second{ -1, -1, 0 };

    // LMOUSE + CTRL.
    while (true) {
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {

            cv::Mat screenshot = HWND2MAT(GetDesktopWindow());

            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);

            int x = p.x;
            int y = p.y;

            cv::imwrite("debug_screenshot_click.jpg", screenshot);

            // Should be top-left cell.
            if (first.x == -1) {
                first.x = x;
                first.y = y;
                first.grayscaleValue = screenshot.at<uchar>(y, x);
                std::cout << "[DEBUG] First click at (" << first.x << ", " << first.y << ") with grayscale value: " << (int)first.grayscaleValue << std::endl;
                Sleep(200);
            }

            // Should be top-right cell.
            else if (second.x == -1) {
                second.x = x;
                second.y = y;
                second.grayscaleValue = screenshot.at<uchar>(y, x);
                std::cout << "[DEBUG] Second click at (" << second.x << ", " << second.y << ") with grayscale value: " << (int)second.grayscaleValue << std::endl;
                break;
            }
        }
    }

    return { first, second};
}