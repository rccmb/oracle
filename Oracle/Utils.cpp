#include "Utils.h"

cv::Mat hwnd2mat(HWND hwnd) {
    HDC hwindowDC, hwindowCompatibleDC;

    int height, width;
    HBITMAP hbwindow;
    cv::Mat src;

    hwindowDC = GetDC(hwnd);
    hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);

    RECT windowsize;
    GetClientRect(hwnd, &windowsize);

    height = windowsize.bottom;
    width = windowsize.right;

    src.create(height, width, CV_8UC4);

    hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
    SelectObject(hwindowCompatibleDC, hbwindow);
    BitBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, SRCCOPY);
    GetBitmapBits(hbwindow, src.total() * src.elemSize(), src.data);

    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);

    return src;
}