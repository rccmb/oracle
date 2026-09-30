#pragma once

#include "imgui.h"

#include "ui/Theme.h"

// The handful of controls the menu is built from, drawn in Oracle's own style
// rather than ImGui's defaults. Each has hover, pressed and disabled states;
// disabled comes from ImGui::BeginDisabled like any stock widget.
namespace ui {

// A value that eases towards a target instead of jumping to it, remembered per
// ID within the current window. Frame-rate independent.
float Animate(ImGuiID id, float target, float seconds = 0.12f);

enum class ButtonKind {
    Primary,    // Filled with the accent. One per view, for the thing to do next.
    Secondary,  // Outlined.
    Ghost,      // Text only until hovered.
};

bool Button(const char* label, ButtonKind kind = ButtonKind::Secondary, const ImVec2& size = ImVec2(0, 0));

// A square button holding one icon, with a tooltip. `danger` turns the hover red.
bool IconButton(const char* id, const char* icon, const char* tooltip, bool danger = false);

// A settings row: the label on the left, a switch on the right. The whole row
// is the hit target.
bool Toggle(const char* label, bool* value);

// A settings row: the label and the value on one line, a thin slider below.
// Dragging, clicking, the mouse wheel and the arrow keys all move it.
bool Slider(const char* label, int* value, int min, int max, const char* format = "%d");
bool Slider(const char* label, float* value, float min, float max, const char* format = "%.2f");

// One of a few options, as a pill with a sliding selection.
bool Segmented(const char* id, int* selected, const char* const* labels, int count, float width = 0.0f);

// The menu's tabs: labels with an underline that slides to the one selected.
bool Tabs(const char* id, int* selected, const char* const* labels, int count);

// A heading over a group of settings.
void SectionLabel(const char* text);

// Small, quiet, wrapped text for explanations.
void Caption(const char* text);

// Text in a colour, wrapped to the available width.
void WrappedText(ImU32 color, const char* text);

// A dot marking a state, pulsing while something is under way.
void StatusDot(ImU32 color, bool pulse = false);

// A small turning arc, for work in progress.
void Spinner(float radius, ImU32 color);

// A raised panel, sized to what is put in it.
bool BeginCard(const char* id, ImU32 background = theme::kSurface, ImU32 border = theme::kLine);
void EndCard();

// A tinted strip with an icon and a message. Returns true when its action,
// if it has one, is pressed.
bool Banner(const char* id, const char* icon, ImU32 tone, const char* text, const char* action = nullptr);

// A rank badge as the overlay draws it on the board: a solid disc for where a
// move lands, a ring for where it starts. Shared so the menu's list and the
// board always look alike.
void DrawRankDisc(ImDrawList* list, ImVec2 center, float radius, ImU32 fill, bool ring, const char* text);

// The ??, ? or ?! a verdict wears, as a chip in the verdict's colour. The same
// chip on the board and in the menu.
float VerdictChipWidth(const char* verdict, float fontSize);
void DrawVerdictChip(ImDrawList* list, ImVec2 min, float height, float fontSize, const char* verdict);

// Text drawn so its visual middle, not its line box, sits on `center`.
void DrawCenteredText(ImDrawList* list, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text);

} // namespace ui
