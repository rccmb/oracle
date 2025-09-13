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

cv::Mat CropHWND2MAT(HWND hwnd, int x, int y, int width, int height) {
    cv::Mat full = HWND2MAT(hwnd);
    if (full.empty()) return cv::Mat();

    x = std::clamp(x, 0, full.cols - 1);
    y = std::clamp(y, 0, full.rows - 1);
    width = std::min(width, full.cols - x);
    height = std::min(height, full.rows - y);

    cv::Rect roi(x, y, width, height);
    return full(roi).clone();
}