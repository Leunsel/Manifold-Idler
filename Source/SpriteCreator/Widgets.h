#pragma once

#include "imgui.h"

namespace Idler::SpriteCreator
{
    ImU32 Color(ImVec4 color);
    bool IconButton(const char* id, const char* icon, float side, ImVec4 hoverColor, bool active = false, bool enabled = true);
    void Tooltip(const char* text);
    void BoldText(ImDrawList* drawList, ImVec2 at, ImVec4 color, const char* text);
    float BoldWidth(const char* text);
    void BeginCard(const char* id, const char* icon, const char* title, ImVec2 size, const char* note = nullptr, ImVec4 noteColor = ImVec4(0.50f, 0.52f, 0.56f, 1.00f));
    void EndCard();
    void MutedText(const char* text);
}
