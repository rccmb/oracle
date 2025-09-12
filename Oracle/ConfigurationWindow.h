#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

void ShowDebugROIWindow(int imageWidth, int imageHeight);

HWND CreateImGuiWindow(HINSTANCE hInstance, const LPCWSTR className);