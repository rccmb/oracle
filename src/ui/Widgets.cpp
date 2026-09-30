#include "ui/Widgets.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace theme;

namespace ui {
namespace {

// A colour with the current style alpha applied, so anything drawn by hand
// fades with the rest of a disabled block.
ImU32 Styled(ImU32 color) {
    return ImGui::GetColorU32(color);
}

float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

} // namespace

float Animate(ImGuiID id, float target, float seconds) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* value = storage->GetFloatRef(id, target);

    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    const float blend = 1.0f - std::exp(-deltaTime / std::max(seconds, 0.001f));
    *value += (target - *value) * blend;
    if (std::fabs(target - *value) < 0.001f) *value = target;
    return *value;
}

bool Button(const char* label, ButtonKind kind, const ImVec2& size) {
    int colors = 0;
    int vars = 0;

    switch (kind) {
    case ButtonKind::Primary:
        ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        colors = 4;
        break;
    case ButtonKind::Secondary:
        ImGui::PushStyleColor(ImGuiCol_Button, kSurface);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceActive);
        ImGui::PushStyleColor(ImGuiCol_Border, kLineStrong);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        colors = 4;
        vars = 1;
        break;
    case ButtonKind::Ghost:
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceActive);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        colors = 4;
        break;
    }

    ImGui::PushFont(GetFonts().strong, 0.0f);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopFont();

    // Hover lightens the edge rather than the fill, which keeps the label's
    // contrast where it is while still answering the pointer.
    if (kind != ButtonKind::Ghost && ImGui::IsItemHovered() && !ImGui::IsItemActive()) {
        const ImU32 edge = kind == ButtonKind::Primary ? kAccentText : IM_COL32(88, 97, 115, 255);
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                            Styled(edge), ImGui::GetStyle().FrameRounding, 0, 1.0f);
    }

    ImGui::PopStyleVar(vars);
    ImGui::PopStyleColor(colors);
    return pressed;
}

bool IconButton(const char* id, const char* icon, const char* tooltip, bool danger) {
    const float side = Px(28.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##icon", ImVec2(side, side));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImGui::PopID();

    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + side, origin.y + side);
    if (hovered || held) {
        // A dangerous button turns solid red, as the close button of a window
        // does. A red mixed towards this dark panel would come out maroon.
        const ImU32 fill = danger ? (held ? IM_COL32(168, 34, 24, 255) : IM_COL32(196, 43, 28, 255))
                                  : (held ? kSurfaceActive : kSurfaceHover);
        list->AddRectFilled(origin, end, Styled(fill), Px(6.0f));
    }

    const ImU32 ink = hovered ? (danger ? IM_COL32(255, 255, 255, 255) : kText) : kTextDim;
    DrawCenteredText(list, GetFonts().body, Px(12.0f),
                     ImVec2(origin.x + side * 0.5f, origin.y + side * 0.5f), Styled(ink), icon);

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

bool Toggle(const char* label, bool* value) {
    const float trackWidth = Px(34.0f);
    const float trackHeight = Px(20.0f);
    const float rowHeight = std::max(trackHeight, ImGui::GetTextLineHeight()) + Px(4.0f);
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    const bool pressed = ImGui::InvisibleButton("##toggle", ImVec2(width, rowHeight));
    if (pressed) *value = !*value;
    const bool hovered = ImGui::IsItemHovered();
    const float on = Animate(ImGui::GetID("##knob"), *value ? 1.0f : 0.0f, 0.07f);
    ImGui::PopID();

    ImDrawList* list = ImGui::GetWindowDrawList();

    const float textHeight = ImGui::GetTextLineHeight();
    list->AddText(ImVec2(origin.x, origin.y + (rowHeight - textHeight) * 0.5f), Styled(kText), label);

    const ImVec2 trackMin(origin.x + width - trackWidth, origin.y + (rowHeight - trackHeight) * 0.5f);
    const ImVec2 trackMax(trackMin.x + trackWidth, trackMin.y + trackHeight);
    const ImU32 off = hovered ? kLineStrong : kSurfaceActive;
    const ImU32 onColor = hovered ? kAccentHover : kAccent;
    list->AddRectFilled(trackMin, trackMax, Styled(Mix(off, onColor, on)), trackHeight * 0.5f);

    const float knobRadius = trackHeight * 0.5f - Px(3.0f);
    const float knobX = trackMin.x + trackHeight * 0.5f + (trackWidth - trackHeight) * EaseOut(on);
    list->AddCircleFilled(ImVec2(knobX, trackMin.y + trackHeight * 0.5f), knobRadius,
                          Styled(IM_COL32(245, 247, 250, 255)), 24);
    return pressed;
}

namespace {

// The shared body of both sliders, working on a fraction of the range.
bool SliderTrack(const char* label, const char* valueText, float* fraction, float step) {
    const float width = ImGui::GetContentRegionAvail().x;

    // Label and value on one line.
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const float valueWidth = ImGui::CalcTextSize(valueText).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - valueWidth));
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(valueText);
    ImGui::PopStyleColor();

    // The track, tight under its label.
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(4.0f));
    const float knobRadius = Px(7.0f);
    const float rowHeight = Px(20.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    ImGui::InvisibleButton("##track", ImVec2(width, rowHeight));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool focused = ImGui::IsItemFocused();
    ImGui::PopID();

    const float left = origin.x + knobRadius;
    const float right = origin.x + width - knobRadius;

    bool changed = false;
    float wanted = *fraction;
    if (held && right > left) {
        wanted = (ImGui::GetIO().MousePos.x - left) / (right - left);
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        wanted += (ImGui::GetIO().MouseWheel > 0.0f ? step : -step);
    }
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) wanted -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) wanted += step;
    }
    wanted = std::clamp(wanted, 0.0f, 1.0f);
    if (wanted != *fraction) {
        *fraction = wanted;
        changed = true;
    }

    ImDrawList* list = ImGui::GetWindowDrawList();
    const float centerY = origin.y + rowHeight * 0.5f;
    const float trackHalf = Px(2.0f);
    const float knobX = left + (right - left) * *fraction;

    list->AddRectFilled(ImVec2(left, centerY - trackHalf), ImVec2(right, centerY + trackHalf),
                        Styled(kSurfaceActive), trackHalf);
    list->AddRectFilled(ImVec2(left, centerY - trackHalf), ImVec2(knobX, centerY + trackHalf),
                        Styled(hovered || held ? kAccentHover : kAccent), trackHalf);

    if (hovered || held) {
        list->AddCircleFilled(ImVec2(knobX, centerY), knobRadius + Px(4.0f),
                              Styled(Mix(kBg, kAccent, 0.30f)), 24);
    }
    list->AddCircleFilled(ImVec2(knobX, centerY), knobRadius, Styled(IM_COL32(245, 247, 250, 255)), 24);
    return changed;
}

} // namespace

bool Slider(const char* label, int* value, int min, int max, const char* format) {
    char text[32];
    std::snprintf(text, sizeof(text), format, *value);

    const float range = (float)std::max(1, max - min);
    float fraction = (float)(*value - min) / range;
    if (!SliderTrack(label, text, &fraction, 1.0f / range)) return false;

    const int next = std::clamp(min + (int)std::lround(fraction * range), min, max);
    if (next == *value) return false;
    *value = next;
    return true;
}

bool Slider(const char* label, float* value, float min, float max, const char* format) {
    char text[32];
    std::snprintf(text, sizeof(text), format, *value);

    const float range = std::max(0.0001f, max - min);
    float fraction = (*value - min) / range;
    if (!SliderTrack(label, text, &fraction, 0.01f)) return false;

    // Held to two decimals, which is as fine as any of these are read.
    const float next = std::clamp(std::round((min + fraction * range) * 100.0f) / 100.0f, min, max);
    if (next == *value) return false;
    *value = next;
    return true;
}

bool Segmented(const char* id, int* selected, const char* const* labels, int count, float width) {
    if (count <= 0) return false;

    const float height = Px(28.0f);
    if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
    const float segment = width / count;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::PushID(id);
    ImDrawList* list = ImGui::GetWindowDrawList();

    list->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Styled(kSurface), Px(7.0f));
    list->AddRect(origin, ImVec2(origin.x + width, origin.y + height), Styled(kLine), Px(7.0f));

    const float at = Animate(ImGui::GetID("##selection"), (float)*selected, 0.08f);
    const float inset = Px(3.0f);
    const ImVec2 pillMin(origin.x + segment * at + inset, origin.y + inset);
    const ImVec2 pillMax(pillMin.x + segment - inset * 2.0f, origin.y + height - inset);
    list->AddRectFilled(pillMin, pillMax, Styled(kSurfaceActive), Px(5.0f));
    list->AddRect(pillMin, pillMax, Styled(kLineStrong), Px(5.0f));

    bool changed = false;
    for (int i = 0; i < count; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + segment * i, origin.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##segment", ImVec2(segment, height)) && *selected != i) {
            *selected = i;
            changed = true;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        const ImU32 ink = (i == *selected || hovered) ? kText : kTextDim;
        DrawCenteredText(list, GetFonts().strong, ImGui::GetFontSize() * 0.93f,
                         ImVec2(origin.x + segment * (i + 0.5f), origin.y + height * 0.5f), Styled(ink), labels[i]);
    }

    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
    ImGui::Dummy(ImVec2(width, 0.0f));
    return changed;
}

bool Tabs(const char* id, int* selected, const char* const* labels, int count) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = Px(30.0f);
    const float gap = Px(20.0f);
    ImFont* font = GetFonts().strong;
    const float size = ImGui::GetFontSize();

    ImGui::PushID(id);
    ImDrawList* list = ImGui::GetWindowDrawList();

    bool changed = false;
    float x = origin.x;
    float selectedX = origin.x;
    float selectedWidth = 0.0f;

    for (int i = 0; i < count; ++i) {
        const float labelWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, labels[i]).x;

        ImGui::SetCursorScreenPos(ImVec2(x, origin.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##tab", ImVec2(labelWidth, height)) && *selected != i) {
            *selected = i;
            changed = true;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        const ImU32 ink = (i == *selected) ? kText : (hovered ? kTextDim : kTextFaint);
        list->AddText(font, size, ImVec2(x, origin.y + (height - size * 1.35f) * 0.5f), Styled(ink), labels[i]);

        if (i == *selected) {
            selectedX = x;
            selectedWidth = labelWidth;
        }
        x += labelWidth + gap;
    }

    // The underline slides rather than jumps, so switching tabs reads as one
    // indicator moving, not two appearing and disappearing.
    const float lineX = Animate(ImGui::GetID("##lineX"), selectedX - origin.x, 0.07f);
    const float lineWidth = Animate(ImGui::GetID("##lineW"), selectedWidth, 0.07f);
    const float baseline = origin.y + height;

    list->AddLine(ImVec2(origin.x, baseline - 0.5f), ImVec2(origin.x + width, baseline - 0.5f), Styled(kLine), 1.0f);
    list->AddRectFilled(ImVec2(origin.x + lineX, baseline - Px(2.0f)),
                        ImVec2(origin.x + lineX + lineWidth, baseline), Styled(kAccentText), Px(1.0f));

    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, baseline));
    ImGui::Dummy(ImVec2(width, 0.0f));
    return changed;
}

void SectionLabel(const char* text) {
    ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));
    ImGui::PushFont(GetFonts().strong, kSizeSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void Caption(const char* text) {
    ImGui::PushFont(nullptr, kSizeSmall);
    WrappedText(kTextFaint, text);
    ImGui::PopFont();
}

void WrappedText(ImU32 color, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void StatusDot(ImU32 color, bool pulse) {
    const float radius = Px(4.0f);
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2.0f, lineHeight));

    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 center(origin.x + radius, origin.y + lineHeight * 0.5f);

    // Breathes while something is under way. Size rather than a fade: an amber
    // fading into this dark panel passes through brown on its way out.
    float shown = radius;
    if (pulse) {
        const float phase = (float)ImGui::GetTime() * (2.0f * 3.14159265f / 1.4f);
        shown = radius * (1.0f + 0.25f * std::sin(phase));
    }
    list->AddCircleFilled(center, shown, Styled(color), 16);
}

void Spinner(float radius, ImU32 color) {
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2.0f, lineHeight));

    const ImVec2 center(origin.x + radius, origin.y + lineHeight * 0.5f);
    const float start = (float)ImGui::GetTime() * 6.0f;
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->PathArcTo(center, radius - Px(1.0f), start, start + IM_PI * 1.4f, 20);
    list->PathStroke(Styled(color), ImDrawFlags_None, Px(2.0f));
}

bool BeginCard(const char* id, ImU32 background, ImU32 border) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Px(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(12.0f), Px(10.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Px(8.0f), Px(6.0f)));

    const bool open = ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(2);

    // Spacing inside the card follows the card, not the window around it.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Px(8.0f), Px(6.0f)));
    return open;
}

void EndCard() {
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

bool Banner(const char* id, const char* icon, ImU32 tone, const char* text, const char* action) {
    bool pressed = false;

    // The tone at full strength, on the edge and the icon, over a neutral card.
    // Tinting the card instead is the obvious move and the wrong one here:
    // amber, orange and yellow mixed towards a dark panel all come out brown.
    BeginCard(id, kSurface, tone);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, tone);
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, Px(8.0f));

        const float actionWidth = action
            ? ImGui::CalcTextSize(action).x + ImGui::GetStyle().FramePadding.x * 2.0f + Px(8.0f)
            : 0.0f;
        ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - actionWidth);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();

        if (action) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - actionWidth + Px(8.0f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(4.0f));
            pressed = Button(action, ButtonKind::Secondary);
        }
    }
    EndCard();
    return pressed;
}

void DrawCenteredText(ImDrawList* list, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text) {
    // A line box includes room above and below the letters, so centring the box
    // leaves figures sitting high. This centres on the cap height instead.
    ImFontBaked* baked = font->GetFontBaked(size);
    const float width = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
    const float capHeight = size * 0.70f;
    const float top = center.y + capHeight * 0.5f - baked->Ascent;
    list->AddText(font, size, ImVec2(std::round(center.x - width * 0.5f), std::round(top)), color, text);
}

void DrawRankDisc(ImDrawList* list, ImVec2 center, float radius, ImU32 fill, bool ring, const char* text) {
    if (ring) {
        list->AddCircleFilled(center, radius, IM_COL32(14, 14, 16, 255), 20);
        list->AddCircle(center, radius - 0.5f, fill, 20, std::max(1.2f, radius * 0.16f));
    }
    else {
        // A hairline of ink around the disc keeps it apart from a square of a
        // similar tone, and shows which of two crossing marks is on top.
        list->AddCircleFilled(center, radius + 1.0f, IM_COL32(14, 14, 16, 255), 24);
        list->AddCircleFilled(center, radius, fill, 20);
    }
    DrawCenteredText(list, GetFonts().strong, radius * 1.30f, center, IM_COL32(255, 255, 255, 255), text);
}

float VerdictChipWidth(const char* verdict, float fontSize) {
    return std::round(GetFonts().strong->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, VerdictGlyph(verdict)).x + fontSize);
}

void DrawVerdictChip(ImDrawList* list, ImVec2 min, float height, float fontSize, const char* verdict) {
    const float width = VerdictChipWidth(verdict, fontSize);
    // White reads on the red; the orange and the yellow need dark ink.
    const ImU32 ink = std::strcmp(verdict, "Blunder") == 0 ? IM_COL32(255, 255, 255, 255) : IM_COL32(14, 14, 16, 255);
    list->AddRectFilled(min, ImVec2(min.x + width, min.y + height), VerdictColor(verdict), height * 0.5f);
    DrawCenteredText(list, GetFonts().strong, fontSize, ImVec2(min.x + width * 0.5f, min.y + height * 0.5f),
                     ink, VerdictGlyph(verdict));
}

} // namespace ui
