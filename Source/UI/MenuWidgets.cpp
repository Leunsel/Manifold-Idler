#include "UI/MenuWidgets.h"

#include "Core/Errors.h"
#include "Generated/IconsFontAwesome5.h"
#include "UI/Covers.h"
#include "UI/UiScale.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace Idler::UI
{
    namespace
    {
        constexpr ImVec4 kAccent = ManifoldTheme::Accent;
        constexpr ImU32 kAccentU32 = IM_COL32(46, 219, 158, 255);
        constexpr ImU32 kHoverU32 = IM_COL32(255, 255, 255, 9);

        constexpr float kCardPadding = 12.0f;
        constexpr float kInfoLabelWidth = 96.0f;
        constexpr float kTwoColumnMinWidth = 600.0f;

        float g_RowInset = 0.0f;
        float g_ListRowInset = 0.0f;

        constexpr float kRowRounding = 5.0f;

        bool PlainSelectable(bool selected, ImGuiSelectableFlags flags, ImVec2 size)
        {
            const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Header, clear);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, clear);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, clear);
            const bool clicked = ImGui::Selectable("##row", selected, flags, size);
            ImGui::PopStyleColor(3);
            return clicked;
        }

        void DrawRowBackground(ImDrawList* drawList, ImVec2 min, ImVec2 max, bool selected, bool hot, bool active)
        {
            const float hover = Fade(ImGui::GetID("##hover"), hot);
            const float rest = selected ? ImGui::GetStyleColorVec4(ImGuiCol_Header).w : 0.0f;
            const float lit = ImGui::GetStyleColorVec4(active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered).w;
            const float alpha = rest + (lit - rest) * hover;
            if (alpha > 0.0f)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, alpha)), Px(kRowRounding));
        }

        void RowLabel(const char* label, const char* tooltip, const char* warning, bool framed)
        {
            if (framed)
                ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Label);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ItemTooltip(tooltip);

            if (warning && warning[0])
            {
                ImGui::SameLine(0.0f, Px(6.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
                ImGui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE);
                ImGui::PopStyleColor();
                ItemTooltip(warning);
            }
        }
    }

    void DrawWindowBackground(float rounding)
    {
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 max(pos.x + size.x, pos.y + size.y);
        const float split = pos.x + Px(kSidebarWidth);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, ImVec2(split, max.y), ImGui::GetColorU32(ManifoldTheme::Sidebar), rounding, ImDrawFlags_RoundCornersLeft);
        drawList->AddRectFilled(ImVec2(split, pos.y), max, ImGui::GetColorU32(ManifoldTheme::Page), rounding, ImDrawFlags_RoundCornersRight);
        drawList->AddRect(pos, max, ImGui::GetColorU32(ManifoldTheme::Border), rounding);
    }

    bool SidebarItem(const char* icon, const char* label, bool current)
    {
        const float width = ImGui::GetContentRegionAvail().x;
        const float itemHeight = Px(34.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushID(label);
        const bool clicked = ImGui::InvisibleButton("##page", ImVec2(width, itemHeight));
        ImGui::PopID();

        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (hovered)
            drawList->AddRectFilled(ImVec2(pos.x + Px(8.0f), pos.y), ImVec2(pos.x + width - Px(8.0f), pos.y + itemHeight), kHoverU32, Px(4.0f));
        const float textY = pos.y + (itemHeight - ImGui::GetTextLineHeight()) * 0.5f;
        const ImU32 iconColor = current ? kAccentU32 : hovered ? IM_COL32(196, 199, 205, 255) : IM_COL32(112, 116, 124, 255);
        const ImU32 textColor = current ? IM_COL32(236, 238, 241, 255) : hovered ? IM_COL32(196, 199, 205, 255) : IM_COL32(136, 140, 148, 255);
        drawList->AddText(ImVec2(pos.x + Px(20.0f), textY), iconColor, icon);
        drawList->AddText(ImVec2(pos.x + Px(46.0f), textY), textColor, label);
        return clicked;
    }

    void PageHeader(const char* icon, const char* title, std::string_view status, ImVec4 statusColor, float reserve)
    {
        const float headerHeight = Px(34.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 max(pos.x + width, pos.y + headerHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, max, ImGui::GetColorU32(ManifoldTheme::Card), Px(6.0f));
        drawList->AddRect(pos, max, ImGui::GetColorU32(ManifoldTheme::CardBorder), Px(6.0f));

        const float textY = std::round(pos.y + (headerHeight - ImGui::GetTextLineHeight()) * 0.5f);
        float x = pos.x + Px(14.0f);
        drawList->AddText(ImVec2(x, textY), kAccentU32, icon);
        x += ImGui::CalcTextSize(icon).x + Px(8.0f);
        drawList->AddText(ImVec2(x, textY), ImGui::GetColorU32(ManifoldTheme::Label), title);
        x += ImGui::CalcTextSize(title).x;

        if (!status.empty())
        {
            const float right = max.x - Px(14.0f) - reserve;
            const float statusX = std::max(x + Px(16.0f), right - ImGui::CalcTextSize(status.data(), status.data() + status.size()).x);
            drawList->PushClipRect(ImVec2(x + Px(16.0f), pos.y), ImVec2(right, max.y), true);
            drawList->AddText(ImVec2(statusX, textY), ImGui::GetColorU32(statusColor), status.data(), status.data() + status.size());
            drawList->PopClipRect();
        }

        ImGui::Dummy(ImVec2(width, headerHeight));
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    void BeginPageBody()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##page_body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
    }

    void EndPageBody()
    {
        ImGui::EndChild();
    }

    void BeginCard(const char* title)
    {
        ImGui::PushID(title);
        const float height = ImGui::GetStateStorage()->GetFloat(ImGui::GetID("##card_height"), 0.0f);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        if (height > 0.0f)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height), ImGui::GetColorU32(ManifoldTheme::Card), Px(6.0f));
            drawList->AddRect(start, ImVec2(start.x + width, start.y + height), ImGui::GetColorU32(ManifoldTheme::CardBorder), Px(6.0f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Px(8.0f, 9.0f));
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(width, 1.0f));
        ImGui::Indent(Px(kCardPadding));
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        g_RowInset = Px(kCardPadding);
    }

    void EndCard()
    {
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::Unindent(Px(kCardPadding));
        ImGui::EndGroup();
        ImGui::GetStateStorage()->SetFloat(ImGui::GetID("##card_height"), ImGui::GetItemRectSize().y);
        ImGui::PopStyleVar();
        ImGui::PopID();
        g_RowInset = 0.0f;
        ImGui::Dummy(ImVec2(0.0f, Px(3.0f)));
    }

    void BeginCardList(const char* id, float reserve)
    {
        BeginRow(id);
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float height = std::max(ImGui::GetFrameHeight() * 2.0f, ImGui::GetContentRegionAvail().y - reserve - spacing - 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##list", ImVec2(RowWidth(), height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleColor();
        SetScrollSurface(ManifoldTheme::Card);
        g_ListRowInset = std::exchange(g_RowInset, 0.0f);
    }

    void EndCardList()
    {
        ImGui::EndChild();
        g_RowInset = g_ListRowInset;
        ImGui::PopID();
    }

    namespace
    {
        std::vector<std::pair<ImGuiID, ImVec4>> g_scrollSurfaces;

        ImVec4 ScrollSurfaceOf(const ImGuiWindow* window)
        {
            for (const auto& [id, color] : g_scrollSurfaces)
            {
                if (id == window->ID)
                    return color;
            }
            if (window->RootWindow && (window->RootWindow->Flags & ImGuiWindowFlags_Popup) != 0)
                return ImGui::GetStyle().Colors[ImGuiCol_PopupBg];
            return ManifoldTheme::Card;
        }
    }

    void SetScrollSurface(ImVec4 color)
    {
        g_scrollSurfaces.emplace_back(ImGui::GetCurrentWindow()->ID, color);
    }

    void DrawScrollShadows()
    {
        constexpr float kScrollShadowDepth = 18.0f;
        const ImGuiContext& context = *ImGui::GetCurrentContext();
        const float depth = Px(kScrollShadowDepth);
        for (ImGuiWindow* window : context.Windows)
        {
            if (!window->Active || window->Hidden || window->SkipItems || (window->Flags & ImGuiWindowFlags_Tooltip) != 0)
                continue;
            if ((window->Flags & ImGuiWindowFlags_NoScrollbar) != 0 && (window->Flags & ImGuiWindowFlags_NoScrollWithMouse) != 0)
                continue;
            if (window->ScrollMax.y < 1.0f)
                continue;
            const ImRect& clip = window->InnerClipRect;
            if (clip.GetWidth() <= 0.0f || clip.GetHeight() <= depth)
                continue;

            const ImVec4 surface = ScrollSurfaceOf(window);
            const ImU32 none = ImGui::ColorConvertFloat4ToU32(ManifoldTheme::WithAlpha(surface, 0.0f));
            const float above = std::clamp(window->Scroll.y / depth, 0.0f, 1.0f);
            const float below = std::clamp((window->ScrollMax.y - window->Scroll.y) / depth, 0.0f, 1.0f);
            ImDrawList* drawList = window->DrawList;
            drawList->PushClipRect(clip.Min, clip.Max, false);
            if (above > 0.01f)
            {
                const ImU32 edge = ImGui::ColorConvertFloat4ToU32(ManifoldTheme::WithAlpha(surface, surface.w * above));
                drawList->AddRectFilledMultiColor(clip.Min, ImVec2(clip.Max.x, clip.Min.y + depth), edge, edge, none, none);
            }
            if (below > 0.01f)
            {
                const ImU32 edge = ImGui::ColorConvertFloat4ToU32(ManifoldTheme::WithAlpha(surface, surface.w * below));
                drawList->AddRectFilledMultiColor(ImVec2(clip.Min.x, clip.Max.y - depth), clip.Max, none, none, edge, edge);
            }
            drawList->PopClipRect();
        }
        g_scrollSurfaces.clear();
    }

    float CardRowsHeight(int rows)
    {
        return static_cast<float>(rows) * (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y);
    }

    void CardColumns(const char* id, const std::function<void()>& left, const std::function<void()>& right, float leftShare, float rightLimit)
    {
        const float space = ImGui::GetContentRegionAvail().x;
        if (space >= Px(kTwoColumnMinWidth) &&
            ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        {
            float rightWidth = std::floor((space - ImGui::GetStyle().CellPadding.x * 2.0f) * (1.0f - leftShare));
            if (rightLimit > 0.0f)
                rightWidth = std::min(rightWidth, rightLimit);
            ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthFixed, rightWidth);
            ImGui::TableNextColumn();
            left();
            ImGui::TableNextColumn();
            right();
            ImGui::EndTable();
        }
        else
        {
            left();
            right();
        }
    }

    float RowWidth()
    {
        return std::max(Px(60.0f), ImGui::GetContentRegionAvail().x - g_RowInset);
    }

    void BeginRow(const char* id)
    {
        ImGui::PushID(id);
    }

    void ControlRow(const char* label, const char* tooltip, float width, const std::function<void()>& control, const char* warning)
    {
        BeginRow(label);
        const float start = ImGui::GetCursorPosX();
        const float rowWidth = RowWidth();
        RowLabel(label, tooltip, warning, true);

        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), start + rowWidth - width));
        ImGui::SetNextItemWidth(width);
        control();
        ImGui::PopID();
    }

    bool ToggleRow(const char* label, const char* tooltip, bool& value)
    {
        bool changed = false;
        ControlRow(label, tooltip, ToggleSwitchWidth(), [&]() { changed = ToggleSwitch("##toggle", value); });
        return changed;
    }

    void InfoRow(const char* label, std::string_view value, ImVec4 color, ImTextureID image)
    {
        BeginRow(label);
        const float start = ImGui::GetCursorPosX();
        const float end = ImGui::GetCursorScreenPos().x + RowWidth();
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine();
        ImGui::SetCursorPosX(start + Px(kInfoLabelWidth));
        if (image)
        {
            const float size = ListIconSize();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float top = std::round(at.y + (ImGui::GetTextLineHeight() - size) * 0.5f);
            ImGui::GetWindowDrawList()->AddImage(image, ImVec2(at.x, top), ImVec2(at.x + size, top + size));
            ImGui::Dummy(ImVec2(size, ImGui::GetTextLineHeight()));
            ImGui::SameLine(0.0f, Px(6.0f));
        }
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::PushClipRect(pos, ImVec2(end, pos.y + ImGui::GetTextLineHeight()), true);
        DrawCopyableText(value, color, std::max(1.0f, end - pos.x));
        ImGui::PopClipRect();
        if (pos.x + ImGui::CalcTextSize(value.data(), value.data() + value.size()).x > end && ImGui::IsItemHovered())
            ImGui::SetTooltip("%.*s\nClick to copy.", static_cast<int>(value.size()), value.data());
        ImGui::PopID();
    }

    float ListIconSize()
    {
        return std::round(Px(16.0f));
    }

    bool ListRow(const char* id, bool selected, const char* icon, ImVec4 iconColor, std::string_view name, ImVec4 nameColor,
        const std::vector<RowTag>& tags, ImTextureID image)
    {
        BeginRow(id);
        const float rowWidth = RowWidth();
        const float rowHeight = ImGui::GetFrameHeight();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool clicked = PlainSelectable(selected, ImGuiSelectableFlags_None, ImVec2(rowWidth, rowHeight));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawRowBackground(drawList, pos, ImVec2(pos.x + rowWidth, pos.y + rowHeight), selected, ImGui::IsItemHovered(), ImGui::IsItemActive());
        const float textY = pos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
        if (image)
        {
            const float size = ListIconSize();
            const ImVec2 at(pos.x + Px(6.0f), std::round(pos.y + (rowHeight - size) * 0.5f));
            drawList->AddImage(image, at, ImVec2(at.x + size, at.y + size));
        }
        else
        {
            drawList->AddText(ImVec2(pos.x + Px(6.0f), textY), ImGui::GetColorU32(iconColor), icon);
        }

        float right = pos.x + rowWidth - Px(6.0f);
        for (auto tag = tags.rbegin(); tag != tags.rend(); ++tag)
        {
            right -= ImGui::CalcTextSize(tag->Text.c_str()).x;
            drawList->AddText(ImVec2(right, textY), ImGui::GetColorU32(tag->Color), tag->Text.c_str());
            right -= Px(8.0f);
        }
        const float nameX = pos.x + Px(28.0f);
        drawList->PushClipRect(ImVec2(nameX, pos.y), ImVec2(std::max(nameX, right), pos.y + rowHeight), true);
        DrawCutText(drawList, ImVec2(nameX, textY), right, nameColor, name);
        drawList->PopClipRect();
        ImGui::PopID();
        return clicked;
    }

    float GameRowHeight()
    {
        return std::round(Px(40.0f));
    }

    namespace
    {
        void DrawCornerShade(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImVec2 size, bool right, bool bottom, float rounding)
        {
            const ImVec2 from(right ? std::max(min.x, max.x - size.x) : min.x, bottom ? std::max(min.y, max.y - size.y) : min.y);
            const ImVec2 to(right ? max.x : std::min(max.x, min.x + size.x), bottom ? max.y : std::min(max.y, min.y + size.y));
            if (to.x <= from.x || to.y <= from.y)
                return;

            constexpr float kDarkest = 150.0f;
            const ImDrawFlags corner = right ? (bottom ? ImDrawFlags_RoundCornersBottomRight : ImDrawFlags_RoundCornersTopRight) : (bottom ? ImDrawFlags_RoundCornersBottomLeft : ImDrawFlags_RoundCornersTopLeft);
            const int first = drawList->VtxBuffer.Size;
            drawList->AddRectFilled(from, to, IM_COL32(8, 10, 12, 255), rounding, corner);
            for (int index = first; index < drawList->VtxBuffer.Size; ++index)
            {
                ImDrawVert& point = drawList->VtxBuffer[index];
                const float across = std::clamp((point.pos.x - from.x) / (to.x - from.x), 0.0f, 1.0f);
                const float down = std::clamp((point.pos.y - from.y) / (to.y - from.y), 0.0f, 1.0f);
                const float closeness = (right ? across : 1.0f - across) * (bottom ? down : 1.0f - down);
                const float edge = static_cast<float>((point.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) / 255.0f;
                point.col = (point.col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(kDarkest * closeness * edge) << IM_COL32_A_SHIFT);
            }
        }

        void DrawCover(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImTextureID cover, float shown = 1.0f)
        {
            const float rounding = Px(3.0f);
            if (cover)
            {
                const float cut = (1.0f - shown) * 0.5f;
                drawList->AddImageRounded(cover, min, max, ImVec2(0.0f, cut), ImVec2(1.0f, 1.0f - cut), IM_COL32_WHITE, rounding);
                return;
            }
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Sidebar), rounding);
            const ImVec2 icon = ImGui::CalcTextSize(ICON_FA_GAMEPAD);
            drawList->AddText(ImVec2(std::round((min.x + max.x - icon.x) * 0.5f), std::round((min.y + max.y - icon.y) * 0.5f)),
                ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Muted, 0.6f)), ICON_FA_GAMEPAD);
        }
    }

    float Fade(ImGuiID key, bool on, float speed)
    {
        const float target = on ? 1.0f : 0.0f;
        ImGuiStorage* storage = ImGui::GetStateStorage();
        float value = storage->GetFloat(key, 0.0f);
        value = IdleAnimation::Enabled() ? value + (target - value) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed) : target;
        if (std::fabs(target - value) < 0.004f)
            value = target;
        storage->SetFloat(key, value);
        return value;
    }

    void DrawSheen(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, float center, float band, ImVec4 color)
    {
        const float top = min.y + 1.0f;
        const float bottom = max.y - 1.0f;
        const float ends = std::ceil(rounding);

        const ImU32 clear = ImGui::GetColorU32(ManifoldTheme::WithAlpha(color, 0.0f));
        const ImU32 bright = ImGui::GetColorU32(color);
        drawList->PushClipRect(ImVec2(min.x + ends, top), ImVec2(max.x - ends, bottom), true);
        drawList->AddRectFilledMultiColor(ImVec2(center - band, top), ImVec2(center, bottom), clear, bright, bright, clear);
        drawList->AddRectFilledMultiColor(ImVec2(center, top), ImVec2(center + band, bottom), bright, clear, clear, bright);
        drawList->PopClipRect();

        const int columns = static_cast<int>(ends);
        for (int column = 1; column < columns; ++column)
        {
            const float fromCenter = std::max(0.0f, rounding - static_cast<float>(column) - 0.5f);
            const float inset = rounding - std::sqrt(std::max(0.0f, rounding * rounding - fromCenter * fromCenter));
            for (const float x : { min.x + static_cast<float>(column), max.x - static_cast<float>(column) - 1.0f })
            {
                const float light = 1.0f - std::fabs(x + 0.5f - center) / band;
                if (light > 0.0f)
                    drawList->AddRectFilled(ImVec2(x, std::round(top + inset)), ImVec2(x + 1.0f, std::round(bottom - inset)), ImGui::GetColorU32(ManifoldTheme::WithAlpha(color, color.w * light)));
            }
        }
    }

    void DrawIdleHighlight(ImDrawList* drawList, ImVec2 min, ImVec2 max, float strength, std::uint32_t seed, ImVec4 color)
    {
        constexpr double period = 6.0;
        const double moment = seed != 0 ? IdleAnimation::Moment(seed) : 0.0;
        const double pace = 1.0 + 0.2 * (moment - 0.5);
        const double time = IdleAnimation::Enabled() ? ImGui::GetTime() * pace + moment * period : 0.0;
        const auto accent = [&](float alpha) { return ImGui::GetColorU32(ManifoldTheme::WithAlpha(color, alpha * strength)); };
        const float breath = 0.5f + 0.5f * static_cast<float>(std::sin(time * 6.28318530 / 3.4));
        const float rounding = Px(kRowRounding);

        for (int ring = 3; ring >= 1; --ring)
        {
            const float grow = Px(1.5f) * static_cast<float>(ring);
            drawList->AddRectFilled(ImVec2(min.x - grow, min.y - grow), ImVec2(max.x + grow, max.y + grow), accent(0.012f + 0.012f * breath), rounding + grow);
        }
        drawList->AddRectFilled(min, max, accent(0.06f + 0.06f * breath), rounding);
        drawList->AddRect(min, max, accent(0.22f + 0.26f * breath), rounding);

        constexpr double passes = 1.3;
        const double phase = std::fmod(time, period);
        if (phase < passes)
        {
            const float band = (max.x - min.x) * 0.35f;
            const float center = min.x - band + (max.x - min.x + band * 2.0f) * static_cast<float>(phase / passes);
            DrawSheen(drawList, min, max, rounding, center, band, ManifoldTheme::WithAlpha(color, 0.16f * strength));
        }
    }

    bool GameRow(const char* id, bool selected, ImTextureID cover, std::string_view name, ImVec4 nameColor, std::string_view detail, ImVec4 detailColor,
        const std::vector<RowTag>& tags, const GameRowOptions& options)
    {
        const RowAction* action = options.Action;
        const float progress = options.Progress;
        BeginRow(id);
        const float rowWidth = RowWidth();
        const float rowHeight = GameRowHeight();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool clicked = PlainSelectable(selected, action ? ImGuiSelectableFlags_AllowOverlap : ImGuiSelectableFlags_None, ImVec2(rowWidth, rowHeight));
        const bool rowActive = ImGui::IsItemActive();
        const ImGuiLastItemData rowItem = ImGui::GetCurrentContext()->LastItemData;

        if (!ImGui::IsRectVisible(pos, ImVec2(pos.x + rowWidth, pos.y + rowHeight)))
        {
            ImGui::PopID();
            return clicked;
        }

        const ImVec2 rowEnd(pos.x + rowWidth, pos.y + rowHeight);
        const bool hot = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, rowEnd);
        const float actionSize = std::round(Px(26.0f));
        const float reveal = action ? Fade(ImGui::GetID("##reveal"), hot) : 0.0f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawRowBackground(drawList, pos, rowEnd, selected, hot, rowActive);
        const float glow = Fade(ImGui::GetID("##glow"), options.Glow, 3.0f);
        if (glow > 0.0f)
        {
            DrawIdleHighlight(drawList, pos, rowEnd, glow * 0.7f, options.GlowSeed);
            IdleAnimation::RequestFrames();
        }
        const float coverHeight = rowHeight - Px(6.0f);
        const float coverWidth = std::round(coverHeight * Covers::kAspect);
        const ImVec2 coverMin(pos.x + Px(3.0f), pos.y + Px(3.0f));
        const ImVec2 coverMax(coverMin.x + coverWidth, coverMin.y + coverHeight);
        DrawCover(drawList, coverMin, coverMax, cover);

        const float line = ImGui::GetTextLineHeight();
        float right = pos.x + rowWidth - Px(8.0f) - (actionSize + Px(4.0f)) * reveal;
        const float tagY = std::round(pos.y + (rowHeight - line) * 0.5f);
        for (auto tag = tags.rbegin(); tag != tags.rend(); ++tag)
        {
            if (tag->Animated)
            {
                right -= IdleAnimation::Width();
                const ImVec2 at(right, std::round(pos.y + (rowHeight - IdleAnimation::Height()) * 0.5f));
                IdleAnimation::Draw(drawList, at, tag->Mood, tag->Seed);
                if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(at, ImVec2(at.x + IdleAnimation::Width(), at.y + IdleAnimation::Height())))
                {
                    if (tag->Mood == IdleAnimation::Mood::Starting)
                        ImGui::SetTooltip("%s", tag->Text.c_str());
                    else
                        ImGui::SetTooltip("%s\n%s", tag->Text.c_str(), IdleAnimation::NameOf(tag->Seed).c_str());
                }
            }
            else
            {
                right -= ImGui::CalcTextSize(tag->Text.c_str()).x;
                drawList->AddText(ImVec2(right, tagY), ImGui::GetColorU32(tag->Color), tag->Text.c_str());
            }
            right -= Px(10.0f);
        }

        const float textX = coverMax.x + Px(10.0f);
        const float top = std::round(pos.y + (rowHeight - line * 2.0f - Px(3.0f)) * 0.5f);
        drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), pos.y + rowHeight), true);
        DrawCutText(drawList, ImVec2(textX, top), right, nameColor, name);
        DrawCutText(drawList, ImVec2(textX, top + line + Px(3.0f)), right, detailColor, detail);
        drawList->PopClipRect();

        if (progress >= 0.0f)
        {
            const float barY = pos.y + rowHeight - Px(2.0f);
            const float barRight = pos.x + rowWidth - Px(8.0f);
            drawList->AddRectFilled(ImVec2(textX, barY), ImVec2(barRight, barY + Px(2.0f)), IM_COL32(255, 255, 255, 14), Px(1.0f));
            drawList->AddRectFilled(ImVec2(textX, barY), ImVec2(textX + (barRight - textX) * std::min(progress, 1.0f), barY + Px(2.0f)),
                ImGui::GetColorU32(kAccent), Px(1.0f));
        }

        if (action && reveal > 0.01f)
        {
            const ImVec2 at(std::round(pos.x + rowWidth - Px(7.0f) - actionSize + Px(6.0f) * (1.0f - reveal)), std::round(pos.y + (rowHeight - actionSize) * 0.5f));
            const ImVec2 end(at.x + actionSize, at.y + actionSize);
            const ImRect box(at, end);
            const ImGuiID actionId = ImGui::GetID("##action");
            bool hovered = false;
            const bool pressed = ImGui::ItemAdd(box, actionId, nullptr, ImGuiItemFlags_NoNav) && ImGui::ButtonBehavior(box, actionId, &hovered, nullptr);
            const ImVec4 tint = action->Danger ? ManifoldTheme::Error : ManifoldTheme::Accent;
            drawList->AddRectFilled(at, end, ImGui::GetColorU32(ManifoldTheme::WithAlpha(tint, (hovered ? 0.36f : 0.16f) * reveal)), Px(5.0f));
            drawList->AddRect(at, end, ImGui::GetColorU32(ManifoldTheme::WithAlpha(tint, (hovered ? 0.7f : 0.35f) * reveal)), Px(5.0f));
            const ImVec2 icon = ImGui::CalcTextSize(action->Icon);
            drawList->AddText(ImVec2(std::round(at.x + (actionSize - icon.x) * 0.5f), std::round(at.y + (actionSize - icon.y) * 0.5f)),
                ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, reveal)), action->Icon);
            ItemTooltip(action->Tooltip);
            if (pressed && options.ActionClicked)
                *options.ActionClicked = true;

            ImGui::GetCurrentContext()->LastItemData = rowItem;
        }
        ImGui::PopID();
        return clicked;
    }

    namespace
    {
        void PushDangerColors()
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ManifoldTheme::WithAlpha(ManifoldTheme::Error, 0.16f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ManifoldTheme::WithAlpha(ManifoldTheme::Error, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ManifoldTheme::WithAlpha(ManifoldTheme::Error, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Error);
        }

        void PopDangerColors()
        {
            ImGui::PopStyleColor(4);
        }

        void PushPositiveColors()
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.16f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.30f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
        }
    }

    bool IconButton(const char* label, float side, bool danger)
    {
        if (danger)
            PushDangerColors();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        const bool clicked = ImGui::Button(label, ImVec2(side, side));
        ImGui::PopStyleVar();
        if (danger)
            PopDangerColors();
        return clicked;
    }

    bool PositiveIconButton(const char* label, float side)
    {
        PushPositiveColors();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        const bool clicked = ImGui::Button(label, ImVec2(side, side));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        return clicked;
    }

    bool PositiveButton(const char* label, ImVec2 size)
    {
        PushPositiveColors();
        const bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    namespace
    {
        std::string g_actionText;
        float g_actionTextShown = 0.0f;
        ImVec4 g_actionTextColor;
    }

    bool ActionButton(const char* icon, std::string_view text, bool danger)
    {
        ImGui::PushID(icon);
        const std::string label = std::string(icon) + "##button";
        const bool clicked = IconButton(label.c_str(), ImGui::GetFrameHeight(), danger);
        const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
        const bool pointed = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (!disabled && ImGui::GetIO().NavVisible && ImGui::IsItemFocused());
        const float shown = Fade(ImGui::GetID("##says"), pointed);
        if (shown > g_actionTextShown)
        {
            g_actionText.assign(text);
            g_actionTextShown = shown;
            g_actionTextColor = disabled ? ManifoldTheme::Muted : danger ? ManifoldTheme::Error : ManifoldTheme::Label;
        }
        ImGui::PopID();
        ImGui::SameLine();
        return clicked;
    }

    void EndActionButtons()
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float room = RowWidth();
        const float height = ImGui::GetFrameHeight();
        ImGui::Dummy(ImVec2(0.0f, height));
        if (g_actionTextShown > 0.01f && room > 0.0f)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(pos, ImVec2(pos.x + room, pos.y + height), true);
            drawList->AddText(ImVec2(std::round(pos.x + Px(2.0f)), std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)),
                ImGui::GetColorU32(ManifoldTheme::WithAlpha(g_actionTextColor, g_actionTextShown)), g_actionText.c_str());
            drawList->PopClipRect();
        }
        g_actionTextShown = 0.0f;
    }

    bool DangerButton(const char* label, ImVec2 size)
    {
        PushDangerColors();
        const bool clicked = ImGui::Button(label, size);
        PopDangerColors();
        return clicked;
    }

    bool DangerMenuItem(const char* label)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Error);
        const bool clicked = ImGui::MenuItem(label);
        ImGui::PopStyleColor();
        return clicked;
    }

    bool BeginDialog(const char* id, const char* icon, std::string_view title, ImVec2 size, std::string_view note, bool* closed, bool outsideCloses)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float margin = Px(24.0f);
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(size.x, viewport->Size.x - margin * 2.0f), std::min(size.y, viewport->Size.y - margin * 2.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(16.0f, 14.0f));
        const bool open = ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        if (!open)
            return false;

        const float button = ImGui::GetFrameHeight();
        const float top = ImGui::GetCursorPosY();
        ImGui::TextColored(ManifoldTheme::Accent, "%s", icon);
        ImGui::SameLine(0.0f, Px(8.0f));
        ImGui::TextUnformatted(title.data(), title.data() + title.size());
        if (!note.empty())
        {
            ImGui::SameLine(0.0f, Px(10.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(note.data(), note.data() + note.size());
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - button, top));
        const bool clickedBeside = outsideCloses && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetCurrentContext()->HoveredWindow == nullptr;
        const bool out = IconLink("##close_dialog", ICON_FA_TIMES, ImVec2(button, button), nullptr, ManifoldTheme::Error) ||
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive()) || clickedBeside;
        if (out)
        {
            if (closed)
                *closed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        return true;
    }

    void CoverStripRow(const char* id, const std::vector<ImTextureID>& covers, int more)
    {
        BeginRow(id);
        const float gap = Px(4.0f);
        const float space = RowWidth();
        const float count = static_cast<float>(std::max<std::size_t>(1, covers.size()));
        const float width = std::floor((space - gap * (count - 1.0f)) / count);
        const float height = std::round(width / Covers::kAspect);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(space, height));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (std::size_t index = 0; index < covers.size(); ++index)
        {
            const ImVec2 min(pos.x + (width + gap) * static_cast<float>(index), pos.y);
            const ImVec2 max(min.x + width, min.y + height);
            DrawCover(drawList, min, max, covers[index]);
            if (more > 0 && index + 1 == covers.size())
            {
                drawList->AddRectFilled(min, max, IM_COL32(8, 10, 12, 170), Px(3.0f));
                const std::string text = "+" + std::to_string(more);
                const ImVec2 size = ImGui::CalcTextSize(text.c_str());
                drawList->AddText(ImVec2(std::round(min.x + (width - size.x) * 0.5f), std::round(min.y + (height - size.y) * 0.5f)), ImGui::GetColorU32(ManifoldTheme::Text), text.c_str());
            }
        }
        ImGui::PopID();
    }

    float CoverTileHeight(float width, bool detailed)
    {
        const float height = std::round((width - Px(6.0f)) / Covers::kAspect) + ImGui::GetTextLineHeight() + Px(11.0f);
        return detailed ? height + ImGui::GetTextLineHeight() + Px(6.0f) : height;
    }

    bool CoverTile(const char* id, bool selected, ImTextureID cover, std::string_view name, float width, const CoverTileOptions& options)
    {
        ImGui::PushID(id);
        const float height = CoverTileHeight(width, options.Detailed);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 end(pos.x + width, pos.y + height);
        ImGui::SetNextItemAllowOverlap();
        const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(width, height));
        const ImGuiLastItemData tileItem = ImGui::GetCurrentContext()->LastItemData;
        if (!ImGui::IsRectVisible(pos, end))
        {
            ImGui::PopID();
            return clicked;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool hot = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, end);
        const float rounding = Px(4.0f);
        if (selected)
            drawList->AddRectFilled(pos, end, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.16f)), rounding);
        else if (hot)
            drawList->AddRectFilled(pos, end, IM_COL32(255, 255, 255, 9), rounding);
        const float glow = Fade(ImGui::GetID("##glow"), options.Glow, 3.0f);
        if (glow > 0.0f)
        {
            DrawIdleHighlight(drawList, pos, end, glow * 0.7f, options.Seed);
            IdleAnimation::RequestFrames();
        }

        const ImVec2 coverMin(pos.x + Px(3.0f), pos.y + Px(3.0f));
        const float coverWidth = width - Px(6.0f);
        const ImVec2 coverMax(coverMin.x + coverWidth, coverMin.y + std::round(coverWidth / Covers::kAspect));
        DrawCover(drawList, coverMin, coverMax, cover);

        const auto mark = [&](const char* icon, bool top, ImVec4 color)
            {
                const ImVec2 size = ImGui::CalcTextSize(icon);
                const float margin = Px(3.0f);
                const ImVec2 at(coverMin.x + margin, top ? coverMin.y + margin : coverMax.y - size.y - margin);
                DrawCornerShade(drawList, coverMin, coverMax, ImVec2(size.x * 2.2f, size.y * 2.2f), false, !top, Px(3.0f));
                drawList->AddText(at, ImGui::GetColorU32(color), icon);
            };
        if (options.MarkTop)
            mark(options.MarkTop, true, ManifoldTheme::Accent);
        if (options.MarkBottom)
            mark(options.MarkBottom, false, ManifoldTheme::Label);

        if (options.ShowCreature)
            DrawCornerShade(drawList, coverMin, coverMax, ImVec2(IdleAnimation::Width() * 1.7f, IdleAnimation::Height() * 1.7f), true, true, Px(3.0f));
        if (selected)
            drawList->AddRect(coverMin, coverMax, ImGui::GetColorU32(ManifoldTheme::Accent), Px(3.0f), 0, Px(1.5f));
        if (options.ShowCreature)
        {
            const float margin = Px(2.0f);
            IdleAnimation::Draw(drawList, ImVec2(coverMax.x - IdleAnimation::Width() - margin, coverMax.y - IdleAnimation::Height() - margin), options.Mood, options.Seed);
        }

        DrawCutText(drawList, ImVec2(coverMin.x, coverMax.y + Px(3.0f)), end.x - Px(4.0f), selected ? ManifoldTheme::Text : ManifoldTheme::Label, name);
        if (options.Detailed)
        {
            DrawCutText(drawList, ImVec2(coverMin.x, coverMax.y + Px(4.0f) + ImGui::GetTextLineHeight()), end.x - Px(4.0f), options.DetailColor, options.Detail);
            if (options.Progress >= 0.0f)
            {
                const float barTop = end.y - Px(5.0f);
                const float barHeight = std::round(Px(2.0f));
                drawList->AddRectFilled(ImVec2(coverMin.x, barTop), ImVec2(coverMax.x, barTop + barHeight), IM_COL32(255, 255, 255, 18));
                drawList->AddRectFilled(ImVec2(coverMin.x, barTop), ImVec2(coverMin.x + coverWidth * std::min(1.0f, options.Progress), barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Accent));
            }
        }

        const float reveal = options.Action ? Fade(ImGui::GetID("##reveal"), hot) : 0.0f;
        if (options.Action && reveal > 0.01f)
        {
            const float side = std::round(Px(22.0f));
            const ImVec2 at(coverMax.x - side - Px(3.0f), coverMin.y + Px(3.0f));
            const ImVec2 stop(at.x + side, at.y + side);
            const ImRect box(at, stop);
            const ImGuiID actionId = ImGui::GetID("##action");
            bool hovered = false;
            const bool pressed = ImGui::ItemAdd(box, actionId, nullptr, ImGuiItemFlags_NoNav) && ImGui::ButtonBehavior(box, actionId, &hovered, nullptr);
            drawList->AddRectFilled(at, stop, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Sidebar, 0.9f * reveal)), Px(5.0f));
            const ImVec4 tint = options.Action->Danger ? ManifoldTheme::Error : ManifoldTheme::Accent;
            drawList->AddRectFilled(at, stop, ImGui::GetColorU32(ManifoldTheme::WithAlpha(tint, (hovered ? 0.36f : 0.16f) * reveal)), Px(5.0f));
            drawList->AddRect(at, stop, ImGui::GetColorU32(ManifoldTheme::WithAlpha(tint, (hovered ? 0.7f : 0.35f) * reveal)), Px(5.0f));
            const ImVec2 icon = ImGui::CalcTextSize(options.Action->Icon);
            drawList->AddText(ImVec2(std::round(at.x + (side - icon.x) * 0.5f), std::round(at.y + (side - icon.y) * 0.5f)),
                ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, reveal)), options.Action->Icon);
            ItemTooltip(options.Action->Tooltip);
            if (pressed && options.ActionClicked)
                *options.ActionClicked = true;
            ImGui::GetCurrentContext()->LastItemData = tileItem;
        }
        ImGui::PopID();
        return clicked;
    }

    void CoverRow(const char* id, ImTextureID cover, float maxHeight)
    {
        BeginRow(id);
        const float width = RowWidth();
        const float fullHeight = std::round(width / Covers::kAspect);
        const float height = maxHeight > 0.0f ? std::min(fullHeight, std::round(maxHeight)) : fullHeight;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));
        DrawCover(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + width, pos.y + height), cover, height / fullHeight);
        ImGui::PopID();
    }

    float ToggleSwitchWidth()
    {
        return std::round(std::round(ImGui::GetFrameHeight() * 0.72f) * 1.85f);
    }

    bool ToggleSwitch(const char* id, bool& value)
    {
        const float frameHeight = ImGui::GetFrameHeight();
        const float height = std::round(frameHeight * 0.72f);
        const float width = ToggleSwitchWidth();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, ImVec2(width, frameHeight));

        bool changed = false;
        if (ImGui::IsItemClicked())
        {
            value = !value;
            changed = true;
        }

        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID key = ImGui::GetItemID();
        float t = storage->GetFloat(key, value ? 1.0f : 0.0f);
        t += ((value ? 1.0f : 0.0f) - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 16.0f);
        storage->SetFloat(key, t);

        const bool hovered = ImGui::IsItemHovered();
        const ImVec4 off = hovered ? ImVec4(0.26f, 0.27f, 0.31f, 1.0f) : ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
        const ImVec4 on = hovered ? ManifoldTheme::AccentHovered : kAccent;
        const ImVec4 track(off.x + (on.x - off.x) * t, off.y + (on.y - off.y) * t, off.z + (on.z - off.z) * t, 1.0f);

        const ImVec2 min(pos.x, pos.y + (frameHeight - height) * 0.5f);
        const ImVec2 max(min.x + width, min.y + height);
        const float radius = height * 0.5f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(track), radius);
        const float knob = radius - Px(2.5f);
        const ImVec2 center(min.x + radius + t * (width - 2.0f * radius), min.y + radius);
        drawList->AddCircleFilled(center, knob, ImGui::GetColorU32(ImVec4(0.97f, 0.98f, 0.98f, 1.0f)), 20);
        return changed;
    }

    void DrawCutText(ImDrawList* drawList, ImVec2 at, float right, ImVec4 color, std::string_view text)
    {
        const char* textEnd = text.data() + text.size();
        const ImVec2 size = ImGui::CalcTextSize(text.data(), textEnd);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::RenderTextEllipsis(drawList, at, ImVec2(right, at.y + size.y), right, text.data(), textEnd, &size);
        ImGui::PopStyleColor();
    }

    void CoverPicture(ImTextureID cover, ImVec2 size)
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(size);
        DrawCover(ImGui::GetWindowDrawList(), min, ImVec2(min.x + size.x, min.y + size.y), cover);
    }

    bool DrawCopyableText(std::string_view value, ImVec4 color, float maxWidth)
    {
        if (value.empty())
        {
            ImGui::TextDisabled("-");
            return false;
        }

        const std::string text(value);
        const ImVec2 size = ImGui::CalcTextSize(text.c_str());
        if (maxWidth > 0.0f && size.x > maxWidth)
        {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(maxWidth, size.y));
            DrawCutText(ImGui::GetWindowDrawList(), at, at.x + maxWidth, color, text);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopStyleColor();
        }

        const bool hovered = ImGui::IsItemHovered();
        if (hovered)
            ImGui::SetTooltip("Click to copy.");
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ImGui::SetClipboardText(text.c_str());
            return true;
        }

        return false;
    }

    bool IconLink(const char* id, const char* icon, ImVec2 size, const char* tooltip, ImVec4 hoverColor, ImVec4 restColor)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (hovered)
            drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), kHoverU32, Px(4.0f));
        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        drawList->AddText(ImVec2(std::round(pos.x + (size.x - iconSize.x) * 0.5f), std::round(pos.y + (size.y - iconSize.y) * 0.5f)),
            ImGui::GetColorU32(hovered ? hoverColor : restColor), icon);
        ItemTooltip(tooltip);
        return clicked;
    }

    bool BeginContextMenu(const char* id, std::string_view title)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
        const bool open = ImGui::BeginPopupContextItem(id);
        ImGui::PopStyleVar();
        if (open && !title.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(title.data(), title.data() + title.size());
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
        return open;
    }

    void ShowTooltip(const std::function<void()>& content)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(10.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Px(8.0f, 4.0f));
        ImGui::BeginTooltip();
        content();
        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
    }

    void ActionTooltip(const char* text)
    {
        if (!text || !text[0] || !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            return;

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const bool leftwards = min.x > viewport->Pos.x + viewport->Size.x * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(leftwards ? max.x : min.x, min.y - Px(6.0f)), ImGuiCond_Always, ImVec2(leftwards ? 1.0f : 0.0f, 1.0f));
        ItemTooltip(text);
    }

    void ItemTooltip(const char* text)
    {
        if (!text || !text[0] || !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            return;

        ShowTooltip([text]()
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
                ImGui::TextUnformatted(text);
                ImGui::PopTextWrapPos();
            });
    }

    bool ErrorTooltip(std::string_view message)
    {
        const ErrorInfo* info = ErrorOf(message);
        if (!info || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            return false;

        static ErrorCode copiedCode{};
        static double copiedAt = -10.0;
        const std::string name = ErrorName(info->Code);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (clicked)
        {
            ImGui::SetClipboardText(name.c_str());
            copiedCode = info->Code;
            copiedAt = ImGui::GetTime();
        }
        const bool copied = copiedCode == info->Code && ImGui::GetTime() - copiedAt < 2.0;
        if (!copied && !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            return clicked;

        const std::string heading = name + "  " + std::string(ErrorArea(info->Code));
        const std::string footer = copied ? "Copied " + name + "." : "Click to copy " + name + ".";
        ShowTooltip([&]()
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
                ImGui::TextUnformatted(heading.c_str());
                ImGui::PopStyleColor();
                ImGui::TextUnformatted(info->Cause.data(), info->Cause.data() + info->Cause.size());
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Label);
                ImGui::TextUnformatted(info->Help.data(), info->Help.data() + info->Help.size());
                ImGui::PopStyleColor();
                ImGui::TextDisabled("%s", footer.c_str());
                ImGui::PopTextWrapPos();
            });
        return clicked;
    }
}
