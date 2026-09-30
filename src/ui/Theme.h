#pragma once

#include "imgui.h"

// Oracle's visual language, shared by the menu and the overlay.
//
// A dark instrument panel with one accent. Colour beyond the accent is kept for
// meaning, an evaluation, a verdict, a warning, so that when something is red it
// is saying something. DESIGN.md at the repository root explains the choices.
namespace theme {

// Surfaces, darkest first. The menu floats over a chess site, usually a dark
// one, so it sits a step below the page rather than competing with it.
constexpr ImU32 kBg = IM_COL32(16, 18, 22, 255);           // Window.
constexpr ImU32 kSurface = IM_COL32(24, 27, 33, 255);      // Fields, cards, tracks.
constexpr ImU32 kSurfaceHover = IM_COL32(32, 36, 44, 255);
constexpr ImU32 kSurfaceActive = IM_COL32(40, 45, 55, 255);
constexpr ImU32 kLine = IM_COL32(38, 42, 51, 255);         // Hairlines and borders.
constexpr ImU32 kLineStrong = IM_COL32(58, 64, 77, 255);

// Text. Each step clears 4.5:1 against both kBg and kSurface.
constexpr ImU32 kText = IM_COL32(232, 234, 239, 255);
constexpr ImU32 kTextDim = IM_COL32(156, 163, 175, 255);
constexpr ImU32 kTextFaint = IM_COL32(128, 136, 149, 255);

// The accent: actions and the current state, nothing else. Blue because every
// other hue already means something on a chess board.
constexpr ImU32 kAccent = IM_COL32(37, 99, 235, 255);      // Filled controls, white text on it.
constexpr ImU32 kAccentHover = IM_COL32(45, 108, 242, 255);
constexpr ImU32 kAccentActive = IM_COL32(30, 84, 206, 255);
constexpr ImU32 kAccentText = IM_COL32(122, 170, 255, 255); // Accent as text or a mark on dark.

// States.
constexpr ImU32 kGood = IM_COL32(74, 190, 110, 255);
constexpr ImU32 kWarn = IM_COL32(242, 166, 48, 255);
constexpr ImU32 kBad = IM_COL32(240, 86, 80, 255);

// Evaluation colours, as the rank badges on the board use them. Chosen as the
// opaque colours they end up as on screen: the overlay window is colour keyed on
// black, so any alpha only drags a colour towards black, which is how a
// translucent yellow used to arrive as brown.
constexpr ImU32 kMateFor = IM_COL32(130, 79, 209, 255);    // A forced mate for us.
constexpr ImU32 kMateAgainst = IM_COL32(186, 41, 41, 255); // A forced mate against us.
constexpr ImU32 kWinning = IM_COL32(28, 128, 58, 255);   // Deep enough for white figures at 5:1.
constexpr ImU32 kLosing = IM_COL32(196, 53, 53, 255);
constexpr ImU32 kLevel = IM_COL32(87, 100, 120, 255);

// Verdicts on a move, in the colours chess sites use for them.
constexpr ImU32 kBlunder = IM_COL32(214, 48, 49, 255);
constexpr ImU32 kMistake = IM_COL32(236, 139, 32, 255);
constexpr ImU32 kInaccuracy = IM_COL32(240, 196, 25, 255);

// The eval bar's two sides.
constexpr ImU32 kWhiteSide = IM_COL32(236, 236, 232, 255);
constexpr ImU32 kBlackSide = IM_COL32(28, 30, 35, 255);

// Type sizes in pixels at 96 DPI. Everything is scaled by the display's DPI.
constexpr float kSizeBody = 14.0f;
constexpr float kSizeSmall = 12.5f;
constexpr float kSizeTitle = 15.0f;
constexpr float kSizeDisplay = 30.0f;

struct Fonts {
    ImFont* body = nullptr;    // Segoe UI, with Segoe Fluent Icons merged in.
    ImFont* strong = nullptr;  // Segoe UI Semibold, with the icons too.
    ImFont* mono = nullptr;    // Cascadia Mono, or Consolas. Paths and positions only.
    ImFont* pieces = nullptr;  // Segoe UI Symbol, for the chess glyphs.
};

const Fonts& GetFonts();

// Loads the fonts and applies the style to the current context.
void Apply();

// The display's scale over 96 DPI, for sizes drawn by hand.
float Scale();
inline float Px(float value) { return value * Scale(); }

// An ImU32 as an ImVec4, for the few calls that want one.
ImVec4 ToVec4(ImU32 color);

// Mixes two opaque colours, t = 0 giving a and 1 giving b.
ImU32 Mix(ImU32 a, ImU32 b, float t);

// The colour a suggestion's badge takes, from its score for the side that plays
// it. Shared by the board overlay and the move list so the two always agree.
ImU32 EvaluationColor(bool mate, int mateIn, int scoreCp);

// The colour for "Blunder", "Mistake" or "Inaccuracy".
ImU32 VerdictColor(const char* verdict);

// Chess annotation for a verdict: ??, ? or ?!.
const char* VerdictGlyph(const char* verdict);

// How a finished game reads from the player's side: "you win", "you lose" or
// "a draw". One phrase, so the board and the menu say the same thing.
const char* ResultForPlayer(bool mate, bool whiteWon, bool playingWhite);

} // namespace theme

// Icons from Segoe Fluent Icons, merged into the body and strong fonts, so an
// icon can sit inside any label. UTF-8, since the sources are compiled in the
// system code page.
#define ICON_CLOSE        "\xEE\xA2\xBB" // U+E8BB ChromeClose
#define ICON_MINIMIZE     "\xEE\xA4\xA1" // U+E921 ChromeMinimize
#define ICON_SEARCH       "\xEE\x9C\xA1" // U+E721 Search
#define ICON_REFRESH      "\xEE\x9C\xAC" // U+E72C Refresh
#define ICON_CHECK        "\xEE\x9C\xBE" // U+E73E CheckMark
#define ICON_DONE         "\xEE\xB1\xA1" // U+EC61 CompletedSolid
#define ICON_PENDING      "\xEE\xA8\xBA" // U+EA3A CircleRing
#define ICON_ERROR        "\xEE\xAE\x90" // U+EB90 StatusErrorFull
#define ICON_WARNING      "\xEE\x9E\xBA" // U+E7BA Warning
#define ICON_INFO         "\xEE\xA5\x86" // U+E946 Info
#define ICON_FOLDER       "\xEE\xA0\xB8" // U+E838 FolderOpen
#define ICON_CHEVRON_DOWN "\xEE\x9C\x8D" // U+E70D ChevronDown
#define ICON_CHEVRON_RIGHT "\xEE\x9D\xAC" // U+E76C ChevronRight
#define ICON_FLAG         "\xEE\x9F\x81" // U+E7C1 Flag
#define ICON_POWER        "\xEE\x9F\xA8" // U+E7E8 PowerButton
#define ICON_PLAY         "\xEE\x9D\xA8" // U+E768 Play
#define ICON_SETTINGS     "\xEE\x9C\x93" // U+E713 Setting
#define ICON_BOLT         "\xEE\xA5\x85" // U+E945 LightningBolt
#define ICON_EYE          "\xEE\xA2\x90" // U+E890 View
