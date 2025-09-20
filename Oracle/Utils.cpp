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

    cv::Mat bgr;
    cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    cv::Mat result = bgr.clone();

    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);

    return result;
}

std::string PieceToUnicode(char piece) {
    switch (piece) {
        // THEY NEED TO BE SWITCHED SINCE THE FOREGROUND IS WHITE.
        case 'k': return u8"\u2654"; // ♔
        case 'q': return u8"\u2655"; // ♕
        case 'r': return u8"\u2656"; // ♖
        case 'b': return u8"\u2657"; // ♗
        case 'n': return u8"\u2658"; // ♘
        case 'p': return u8"\u2659"; // ♙

        case 'K': return u8"\u265A"; // ♚
        case 'Q': return u8"\u265B"; // ♛
        case 'R': return u8"\u265C"; // ♜
        case 'B': return u8"\u265D"; // ♝
        case 'N': return u8"\u265E"; // ♞
        case 'P': return u8"\u265F"; // ♟

        default: return " ";
    }
}

cv::Mat ApplyPaletteMasking(cv::Mat bgr) {
    cv::Mat mask = cv::Mat::zeros(bgr.size(), CV_8U);

    auto addColorRange = [&](const cv::Vec3b& ref) {
        if (ref == cv::Vec3b(0, 0, 0)) return;
        cv::Scalar lo(std::max(0, ref[0] - g_analysisTolerance), std::max(0, ref[1] - g_analysisTolerance), std::max(0, ref[2] - g_analysisTolerance));
        cv::Scalar hi(std::min(255, ref[0] + g_analysisTolerance), std::min(255, ref[1] + g_analysisTolerance), std::min(255, ref[2] + g_analysisTolerance));
        cv::Mat m; cv::inRange(bgr, lo, hi, m); cv::bitwise_or(mask, m, mask);
    };

    addColorRange(g_refBlackPieceColor);
    addColorRange(g_refWhitePieceColor);
    addColorRange(g_refBoardColor1Color);
    addColorRange(g_refBoardColor2Color);

    cv::Mat filtered;
    bgr.copyTo(filtered, mask);
    cv::cvtColor(filtered, bgr, cv::COLOR_BGR2GRAY);

    return bgr;
}