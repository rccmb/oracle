#pragma once

#include <windows.h>
#include <windowsx.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Struct definition to hold click information.
// Each CLICK contains the x and y coordinates of the click and the grayscale value of the pixel at that location.
// Used for defining the dimensions of the chessboard and board theme.
struct CLICK {
    int x;
    int y;
    uchar grayscaleValue;
};

// Struct definition to hold sample information.
// Samples are rectangles defined by their top-left corner (x, y) and their width and height.
// Used for defining areas where the piece COLOR will be analyzed.
struct SAMPLE {
    int x;
    int y;
    int width;
    int height;
};

// TODO: Documentation.
struct StockfishMove {
    std::string uci;   // Move in UCI format.
    int scoreCp;       // Score in centipawns.
    bool mate;         // True if this is a mate score.
    int mateIn;        // Number of moves to mate.
};