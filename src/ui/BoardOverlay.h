#pragma once

#include "imgui.h"

/**
 * @brief Draws everything Oracle puts over the board itself: the board outline,
 *        the calibration guides, the last move's verdict, and the numbered
 *        suggestions with their arrows.
 *
 * Reads the shared analysis state, taking the analysis lock only to copy it.
 * Kept apart from the render loop so it can be drawn somewhere other than the
 * live overlay, such as the offscreen preview in tools/UiPreview.
 *
 * @param drawList Where to draw, normally ImGui's background draw list, so the
 *                 menu always sits above it.
 */
void DrawBoardOverlay(ImDrawList* drawList);
