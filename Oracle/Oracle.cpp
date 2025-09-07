#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

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

int main() {
    HWND hwndDesktop = GetDesktopWindow();

    /* Setup, testing with 5 screenshots. */
    for (int i = 0; i < 5; i++) {
        cv::Mat screenshot = hwnd2mat(hwndDesktop);

        if (screenshot.empty()) {
            std::cerr << "Failed to capture screen!" << std::endl;
            return -1;
        }

        std::string filename = "screenshot_" + std::to_string(i) + ".png";
        if (!cv::imwrite(filename, screenshot)) {
            std::cerr << "Failed to write image: " << filename << std::endl;
        }
        else {
            std::cout << "Saved: " << filename << std::endl;
        }

        Sleep(1000);
    }

    return 0;
}
