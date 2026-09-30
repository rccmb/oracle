#include "ui/Theme.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>

namespace theme {
namespace {

Fonts g_fonts;
float g_scale = 1.0f;

// A font from the Windows font directory, or empty when it is not installed.
// Segoe Fluent Icons only ships with Windows 11 and Cascadia Mono only with
// recent builds, so every font here has a fallback.
std::string FirstInstalledFont(std::initializer_list<const wchar_t*> files) {
    wchar_t windows[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    const std::filesystem::path fonts =
        std::filesystem::path(length ? windows : L"C:\\Windows") / L"Fonts";

    for (const wchar_t* file : files) {
        std::error_code ec;
        const std::filesystem::path path = fonts / file;
        if (std::filesystem::exists(path, ec)) return path.u8string();
    }
    return {};
}

// The display's scale over 96 DPI. The process is per-monitor aware, so the
// system DPI is the primary display's, which is where the menu opens.
float SystemScale() {
    using DpiForSystemFn = UINT(WINAPI*)();
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (auto dpiForSystem = (DpiForSystemFn)GetProcAddress(user32, "GetDpiForSystem")) {
            const UINT dpi = dpiForSystem();
            if (dpi > 0) return dpi / 96.0f;
        }
    }

    HDC screen = GetDC(nullptr);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) ReleaseDC(nullptr, screen);
    return dpi > 0 ? dpi / 96.0f : 1.0f;
}

// Adds Segoe Fluent Icons into the font added just before, so an icon can be
// written inside any label.
void MergeIcons(float size) {
    // The private use area, where the icon font keeps its glyphs. Limiting the
    // merge to it stops the icon font supplying anything the text font lacks.
    static const ImWchar kIconRanges[] = { 0xE700, 0xF8FF, 0 };

    const std::string icons = FirstInstalledFont({ L"SegoeIcons.ttf", L"segmdl2.ttf" });
    if (icons.empty()) return;

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphRanges = kIconRanges;
    config.GlyphMinAdvanceX = size;              // Icons line up in a column.
    config.GlyphOffset = ImVec2(0.0f, size * 0.20f); // Onto the text's baseline.
    ImGui::GetIO().Fonts->AddFontFromFileTTF(icons.c_str(), size, &config);
}

ImFont* AddTextFont(std::initializer_list<const wchar_t*> files, float size, bool withIcons) {
    const std::string path = FirstInstalledFont(files);
    if (path.empty()) return nullptr;

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size);
    if (font && withIcons) MergeIcons(size);
    return font;
}

void SetColor(ImGuiCol slot, ImU32 color) {
    ImGui::GetStyle().Colors[slot] = ToVec4(color);
}

} // namespace

const Fonts& GetFonts() {
    return g_fonts;
}

float Scale() {
    return g_scale;
}

ImVec4 ToVec4(ImU32 color) {
    return ImGui::ColorConvertU32ToFloat4(color);
}

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto channel = [&](int shift) {
        const float from = (float)((a >> shift) & 0xFF);
        const float to = (float)((b >> shift) & 0xFF);
        return (ImU32)std::lround(from + (to - from) * t) << shift;
    };
    return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) |
           channel(IM_COL32_B_SHIFT) | channel(IM_COL32_A_SHIFT);
}

ImU32 EvaluationColor(bool mate, int mateIn, int scoreCp) {
    if (mate) return mateIn > 0 ? kMateFor : kMateAgainst;
    if (scoreCp > 50) return kWinning;
    if (scoreCp < -50) return kLosing;
    return kLevel;
}

ImU32 VerdictColor(const char* verdict) {
    if (std::strcmp(verdict, "Blunder") == 0) return kBlunder;
    if (std::strcmp(verdict, "Mistake") == 0) return kMistake;
    return kInaccuracy;
}

const char* VerdictGlyph(const char* verdict) {
    if (std::strcmp(verdict, "Blunder") == 0) return "??";
    if (std::strcmp(verdict, "Mistake") == 0) return "?";
    return "?!";
}

const char* ResultForPlayer(bool mate, bool whiteWon, bool playingWhite) {
    if (!mate) return "a draw";
    return whiteWon == playingWhite ? "you win" : "you lose";
}

void Apply() {
    ImGuiIO& io = ImGui::GetIO();
    g_scale = SystemScale();

    /* FONTS. */
    // Sizes passed here are only defaults. ImGui 1.92 rasterises each font at
    // whatever size it is drawn, so one file serves every size below crisply.
    g_fonts.body = AddTextFont({ L"segoeui.ttf" }, kSizeBody, true);
    if (!g_fonts.body) g_fonts.body = io.Fonts->AddFontDefault();

    g_fonts.strong = AddTextFont({ L"seguisb.ttf", L"segoeuib.ttf" }, kSizeBody, true);
    if (!g_fonts.strong) g_fonts.strong = g_fonts.body;

    g_fonts.mono = AddTextFont({ L"CascadiaMono.ttf", L"consola.ttf" }, kSizeSmall, false);
    if (!g_fonts.mono) g_fonts.mono = g_fonts.body;

    g_fonts.pieces = AddTextFont({ L"seguisym.ttf" }, 20.0f, false);
    if (!g_fonts.pieces) g_fonts.pieces = g_fonts.body;

    io.FontDefault = g_fonts.body;

    /* METRICS. */
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();

    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.WindowRounding = 12.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.ChildRounding = 8.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 8.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.FrameRounding = 6.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.DisabledAlpha = 0.42f;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    style.ScaleAllSizes(g_scale);
    style.FontSizeBase = kSizeBody;
    style.FontScaleDpi = g_scale;

    /* COLOURS. */
    for (int slot = 0; slot < ImGuiCol_COUNT; ++slot) style.Colors[slot] = ImVec4(0, 0, 0, 0);

    SetColor(ImGuiCol_Text, kText);
    SetColor(ImGuiCol_TextDisabled, kTextFaint);
    SetColor(ImGuiCol_WindowBg, kBg);
    SetColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    SetColor(ImGuiCol_PopupBg, IM_COL32(22, 25, 30, 255));
    SetColor(ImGuiCol_Border, kLine);
    SetColor(ImGuiCol_FrameBg, kSurface);
    SetColor(ImGuiCol_FrameBgHovered, kSurfaceHover);
    SetColor(ImGuiCol_FrameBgActive, kSurfaceActive);
    SetColor(ImGuiCol_TitleBg, kBg);
    SetColor(ImGuiCol_TitleBgActive, kBg);
    SetColor(ImGuiCol_TitleBgCollapsed, kBg);
    SetColor(ImGuiCol_MenuBarBg, kBg);
    SetColor(ImGuiCol_ScrollbarGrab, kLineStrong);
    SetColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(74, 81, 96, 255));
    SetColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(90, 98, 115, 255));
    SetColor(ImGuiCol_CheckMark, kAccentText);
    SetColor(ImGuiCol_SliderGrab, kAccent);
    SetColor(ImGuiCol_SliderGrabActive, kAccentHover);
    SetColor(ImGuiCol_Button, kSurface);
    SetColor(ImGuiCol_ButtonHovered, kSurfaceHover);
    SetColor(ImGuiCol_ButtonActive, kSurfaceActive);
    SetColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
    SetColor(ImGuiCol_HeaderHovered, kSurfaceHover);
    SetColor(ImGuiCol_HeaderActive, kSurfaceActive);
    SetColor(ImGuiCol_Separator, kLine);
    SetColor(ImGuiCol_SeparatorHovered, kLineStrong);
    SetColor(ImGuiCol_SeparatorActive, kAccent);
    SetColor(ImGuiCol_InputTextCursor, kAccentText);
    SetColor(ImGuiCol_Tab, kSurface);
    SetColor(ImGuiCol_TabHovered, kSurfaceHover);
    SetColor(ImGuiCol_TabSelected, kSurfaceActive);
    SetColor(ImGuiCol_TabSelectedOverline, kAccent);
    SetColor(ImGuiCol_TabDimmed, kSurface);
    SetColor(ImGuiCol_TabDimmedSelected, kSurfaceActive);
    SetColor(ImGuiCol_PlotLines, kAccentText);
    SetColor(ImGuiCol_PlotLinesHovered, kText);
    SetColor(ImGuiCol_PlotHistogram, kAccent);
    SetColor(ImGuiCol_PlotHistogramHovered, kAccentHover);
    SetColor(ImGuiCol_TableHeaderBg, kSurface);
    SetColor(ImGuiCol_TableBorderStrong, kLine);
    SetColor(ImGuiCol_TableBorderLight, kLine);
    SetColor(ImGuiCol_TableRowBgAlt, IM_COL32(255, 255, 255, 6));
    SetColor(ImGuiCol_TextLink, kAccentText);
    SetColor(ImGuiCol_TextSelectedBg, IM_COL32(37, 99, 235, 110));
    SetColor(ImGuiCol_TreeLines, kLine);
    SetColor(ImGuiCol_DragDropTarget, kAccentText);
    SetColor(ImGuiCol_NavCursor, kAccentText);
    SetColor(ImGuiCol_NavWindowingHighlight, IM_COL32(255, 255, 255, 180));
    SetColor(ImGuiCol_NavWindowingDimBg, IM_COL32(8, 9, 11, 200));
    SetColor(ImGuiCol_ModalWindowDimBg, IM_COL32(8, 9, 11, 200));
}

} // namespace theme
