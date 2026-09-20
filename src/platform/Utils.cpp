#include "platform/Utils.h"

void InitializeDisplayMetrics() {
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2. Resolved at runtime rather than
    // compiled against, so the project still builds on a Windows SDK that predates
    // the API. Falls back to the older system-wide call when it is unavailable.
    using SetContextFn = BOOL(WINAPI*)(HANDLE);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto setContext = (SetContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setContext && setContext((HANDLE)-4)) {
            // Per-monitor awareness is active.
        }
        else {
            SetProcessDPIAware();
        }
    }

    // Recorded after the awareness call: these metrics are reported in virtualised
    // units until the process opts in.
    g_virtualScreen.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualScreen.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_virtualScreen.right = g_virtualScreen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_virtualScreen.bottom = g_virtualScreen.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

std::filesystem::path ExecutableDirectory() {
    wchar_t moduleName[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, moduleName, MAX_PATH)) return {};
    return std::filesystem::path(moduleName).parent_path();
}

cv::Mat CaptureScreenRegion(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) return {};

    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return {};

    HDC memoryDC = CreateCompatibleDC(screenDC);
    if (!memoryDC) {
        ReleaseDC(nullptr, screenDC);
        return {};
    }

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(BITMAPINFO));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // Top-down, so row 0 is the top of the region.
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memoryDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap) {
        DeleteDC(memoryDC);
        ReleaseDC(nullptr, screenDC);
        return {};
    }

    HGDIOBJ previous = SelectObject(memoryDC, bitmap);
    BitBlt(memoryDC, 0, 0, width, height, screenDC, x, y, SRCCOPY);

    // cvtColor allocates its own destination, so the result stays valid once the
    // DIB section is released. No extra clone is needed.
    cv::Mat bgra(height, width, CV_8UC4, bits);
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);

    SelectObject(memoryDC, previous);
    DeleteObject(bitmap);
    DeleteDC(memoryDC);
    ReleaseDC(nullptr, screenDC);

    return bgr;
}

cv::Mat CaptureVirtualScreen() {
    return CaptureScreenRegion(
        g_virtualScreen.left,
        g_virtualScreen.top,
        g_virtualScreen.right - g_virtualScreen.left,
        g_virtualScreen.bottom - g_virtualScreen.top);
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
    if (bgr.empty()) return bgr;

    cv::Mat mask = cv::Mat::zeros(bgr.size(), CV_8U);

    auto addBoardColorRange = [&](const cv::Vec3b& ref) {
        cv::Scalar lo(std::max(0, ref[0] - g_analysisTolerance), std::max(0, ref[1] - g_analysisTolerance), std::max(0, ref[2] - g_analysisTolerance));
        cv::Scalar hi(std::min(255, ref[0] + g_analysisTolerance), std::min(255, ref[1] + g_analysisTolerance), std::min(255, ref[2] + g_analysisTolerance));
        cv::Mat m; 
        cv::inRange(bgr, lo, hi, m); 
        cv::bitwise_or(mask, m, mask);
    };

    // Find all pixels that belong to the empty board background.
    addBoardColorRange(g_refBoardColor1Color);
    addBoardColorRange(g_refBoardColor2Color);

    // A highlighted square is still an empty square. Without this the tint a
    // site paints over the last move survives the mask, and the piece standing
    // on it is matched against a background the references never saw.
    {
        std::lock_guard<std::mutex> lock(g_highlightMutex);
        for (const cv::Vec3b& highlight : g_highlightColors) addBoardColorRange(highlight);
    }

    // Replace the board background with a perfectly uniform color.
    // This leaves the pieces completely untouched, preserving all their gradients and texture.
    cv::Mat filtered = bgr.clone();
    cv::Scalar uniformBg(g_refBoardColor1Color[0], g_refBoardColor1Color[1], g_refBoardColor1Color[2]);
    filtered.setTo(uniformBg, mask);

    cv::cvtColor(filtered, bgr, cv::COLOR_BGR2GRAY);
    return bgr;
}