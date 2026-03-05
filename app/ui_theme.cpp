#include "ui_theme.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <algorithm>

namespace voidcare::app::theme {

namespace {

constexpr ImVec4 kBackground = ImVec4(0.043f, 0.043f, 0.063f, 1.0f);      // #0B0B10
constexpr ImVec4 kSurface = ImVec4(0.071f, 0.071f, 0.102f, 1.0f);         // #12121A
constexpr ImVec4 kSurface2 = ImVec4(0.086f, 0.086f, 0.149f, 1.0f);        // #161626
constexpr ImVec4 kBorder = ImVec4(0.141f, 0.141f, 0.227f, 0.55f);         // #24243A
constexpr ImVec4 kText = ImVec4(0.910f, 0.910f, 0.941f, 1.0f);            // #E8E8F0
constexpr ImVec4 kMuted = ImVec4(0.651f, 0.651f, 0.753f, 1.0f);           // #A6A6C0
constexpr ImVec4 kAccent = ImVec4(0.486f, 0.361f, 1.000f, 1.0f);          // #7C5CFF

thread_local int g_panelDepth = 0;

ImU32 toColorU32(const ImVec4& color, const float alphaMul = 1.0f) {
    return ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w * alphaMul));
}

void drawCardShadow(const ImRect& rect, const float rounding, const float strength) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i < 4; ++i) {
        const float expand = 4.0f + static_cast<float>(i) * 3.0f;
        const float alpha = (0.10f - (static_cast<float>(i) * 0.018f)) * strength;
        drawList->AddRectFilled(rect.Min - ImVec2(expand, expand),
                                rect.Max + ImVec2(expand, expand),
                                toColorU32(ImVec4(0.0f, 0.0f, 0.0f, std::max(alpha, 0.015f))),
                                rounding + expand);
    }
}

}  // namespace

ThemeMetrics metrics(const float dpiScale) {
    ThemeMetrics m;
    m.spacing8 *= dpiScale;
    m.spacing12 *= dpiScale;
    m.spacing16 *= dpiScale;
    m.spacing24 *= dpiScale;
    m.cardRadius *= dpiScale;
    m.controlHeight *= dpiScale;
    return m;
}

void ApplyVoidCareStyle(const float dpiScale) {
    applyVoidCareStyle(dpiScale);
}

void applyVoidCareStyle(const float dpiScale) {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);

    style.WindowRounding = 16.0f;
    style.ChildRounding = 16.0f;
    style.FrameRounding = 14.0f;
    style.GrabRounding = 14.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.TabRounding = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(12.0f, 12.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 9.0f);
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kMuted;
    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ChildBg] = kSurface;
    colors[ImGuiCol_PopupBg] = kSurface;
    colors[ImGuiCol_Border] = kBorder;
    colors[ImGuiCol_FrameBg] = kSurface2;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.130f, 0.130f, 0.215f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.180f, 0.180f, 0.290f, 1.0f);
    colors[ImGuiCol_TitleBg] = kSurface;
    colors[ImGuiCol_TitleBgActive] = kSurface;
    colors[ImGuiCol_CheckMark] = kAccent;
    colors[ImGuiCol_SliderGrab] = kAccent;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.52f, 1.0f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.120f, 0.120f, 0.220f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.155f, 0.155f, 0.285f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.200f, 0.180f, 0.350f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.120f, 0.120f, 0.220f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.160f, 0.160f, 0.300f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.200f, 0.180f, 0.350f, 1.0f);
    colors[ImGuiCol_Separator] = kBorder;
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.35f, 0.35f, 0.56f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered] = kAccent;
    colors[ImGuiCol_ResizeGripActive] = kAccent;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.35f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.25f, 0.48f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = kAccent;
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.10f, 0.18f, 1.0f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.09f, 0.09f, 0.16f, 0.92f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.11f, 0.11f, 0.19f, 0.92f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.25f);

    style.ScaleAllSizes(dpiScale);
    ImGui::GetStyle() = style;
}

void drawBackdrop(const ImVec2& pos, const ImVec2& size) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 end = pos + size;

    drawList->AddRectFilledMultiColor(pos,
                                      end,
                                      toColorU32(ImVec4(0.030f, 0.030f, 0.050f, 1.0f)),
                                      toColorU32(ImVec4(0.045f, 0.045f, 0.070f, 1.0f)),
                                      toColorU32(ImVec4(0.055f, 0.055f, 0.090f, 1.0f)),
                                      toColorU32(ImVec4(0.030f, 0.030f, 0.050f, 1.0f)));

    const float vignette = std::min(size.x, size.y) * 0.14f;
    drawList->AddRectFilled(pos, ImVec2(end.x, pos.y + vignette), toColorU32(ImVec4(0, 0, 0, 0.35f)));
    drawList->AddRectFilled(ImVec2(pos.x, end.y - vignette), end, toColorU32(ImVec4(0, 0, 0, 0.42f)));
    drawList->AddRectFilled(pos, ImVec2(pos.x + vignette, end.y), toColorU32(ImVec4(0, 0, 0, 0.25f)));
    drawList->AddRectFilled(ImVec2(end.x - vignette, pos.y), end, toColorU32(ImVec4(0, 0, 0, 0.25f)));
}

bool BeginPanel(const char* id,
                const char* title,
                const ImVec2& size,
                const char* subtitle,
                const std::function<void()>& rightHeaderWidget) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(kSurface.x, kSurface.y, kSurface.z, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(kBorder.x, kBorder.y, kBorder.z, 0.80f));
    const bool begun = ImGui::BeginChild("##panel", size, true, ImGuiWindowFlags_NoScrollbar);
    if (!begun) {
        return false;
    }

    ++g_panelDepth;

    const ImRect rect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize());
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    drawCardShadow(rect, 16.0f, hovered ? 1.35f : 1.0f);
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        rect.Min,
        rect.Max,
        toColorU32(hovered ? ImVec4(0.13f, 0.13f, 0.22f, 0.96f) : ImVec4(0.10f, 0.10f, 0.17f, 0.93f)),
        toColorU32(hovered ? ImVec4(0.08f, 0.08f, 0.14f, 0.96f) : ImVec4(0.07f, 0.07f, 0.12f, 0.93f)),
        toColorU32(hovered ? ImVec4(0.08f, 0.08f, 0.14f, 0.98f) : ImVec4(0.07f, 0.07f, 0.12f, 0.97f)),
        toColorU32(hovered ? ImVec4(0.13f, 0.13f, 0.22f, 0.98f) : ImVec4(0.10f, 0.10f, 0.17f, 0.97f)));
    ImGui::GetWindowDrawList()->AddRect(
        rect.Min,
        rect.Max,
        toColorU32(ImVec4(kBorder.x, kBorder.y, kBorder.z, hovered ? 0.95f : 0.72f)),
        16.0f,
        0,
        hovered ? 1.3f : 1.0f);

    if (title != nullptr && title[0] != '\0') {
        ImGui::TextColored(kText, "%s", title);
        if (subtitle != nullptr && subtitle[0] != '\0') {
            ImGui::TextColored(kMuted, "%s", subtitle);
        }
        if (rightHeaderWidget) {
            const float targetX = ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x - 190.0f;
            const float baseY = ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing();
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), targetX));
            ImGui::SetCursorPosY(baseY);
            rightHeaderWidget();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    return true;
}

void EndPanel() {
    if (g_panelDepth > 0) {
        --g_panelDepth;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

bool BeginSettingRows(const char* id) {
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_PadOuterX |
                                  ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable(id, 2, flags)) {
        return false;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
}

void SettingRow(const char* label, const char* description, const std::function<void()>& drawControlFn) {
    if (ImGui::GetCurrentTable() == nullptr) {
        return;
    }

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(kText, "%s", label != nullptr ? label : "");
    if (description != nullptr && description[0] != '\0') {
        ImGui::TextColored(kMuted, "%s", description);
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(label != nullptr ? label : "setting_row");
    if (drawControlFn) {
        drawControlFn();
    }
    ImGui::PopID();
}

void EndSettingRows() {
    if (ImGui::GetCurrentTable() != nullptr) {
        ImGui::EndTable();
    }
}

void StatusChip(const char* text, const ChipVariant variant) {
    ImVec4 bg = ImVec4(0.21f, 0.22f, 0.30f, 0.95f);
    ImVec4 fg = ImVec4(0.92f, 0.92f, 0.96f, 1.0f);
    switch (variant) {
    case ChipVariant::Accent:
        bg = ImVec4(0.30f, 0.22f, 0.66f, 0.96f);
        fg = ImVec4(0.97f, 0.95f, 1.0f, 1.0f);
        break;
    case ChipVariant::Success:
        bg = ImVec4(0.13f, 0.35f, 0.26f, 0.95f);
        fg = ImVec4(0.91f, 1.0f, 0.93f, 1.0f);
        break;
    case ChipVariant::Warning:
        bg = ImVec4(0.43f, 0.31f, 0.12f, 0.95f);
        fg = ImVec4(1.0f, 0.96f, 0.87f, 1.0f);
        break;
    case ChipVariant::Danger:
        bg = ImVec4(0.42f, 0.18f, 0.23f, 0.95f);
        fg = ImVec4(1.0f, 0.88f, 0.88f, 1.0f);
        break;
    case ChipVariant::Neutral:
        break;
    }

    const char* safeText = text != nullptr ? text : "";
    const ImVec2 textSize = ImGui::CalcTextSize(safeText);
    const ImVec2 padding(10.0f, 5.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
    const ImRect rect(pos, pos + size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rect.Min, rect.Max, toColorU32(bg), size.y * 0.5f);
    drawList->AddRect(rect.Min, rect.Max, toColorU32(ImVec4(bg.x, bg.y, bg.z, 0.92f)), size.y * 0.5f);
    drawList->AddText(ImVec2(rect.Min.x + padding.x, rect.Min.y + padding.y), toColorU32(fg), safeText);
    ImGui::Dummy(size);
}

bool sidebarItem(const char* icon, const char* id, const bool selected, const ImVec2& size) {
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##sidebar_item", size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImRect rect(pos, pos + size);

    if (selected) {
        drawList->AddRectFilled(rect.Min, rect.Max, toColorU32(ImVec4(0.16f, 0.17f, 0.30f, 0.98f)), 14.0f);
        drawList->AddRectFilled(ImVec2(rect.Min.x + 2.0f, rect.Min.y + 9.0f),
                                ImVec2(rect.Min.x + 6.0f, rect.Max.y - 9.0f),
                                toColorU32(kAccent),
                                4.0f);
        drawList->AddRect(rect.Min, rect.Max, toColorU32(ImVec4(0.42f, 0.36f, 0.86f, 0.85f)), 14.0f, 0, 1.2f);
    } else if (hovered) {
        drawList->AddRectFilled(rect.Min - ImVec2(0.0f, 1.5f),
                                rect.Max - ImVec2(0.0f, 1.5f),
                                toColorU32(ImVec4(0.13f, 0.14f, 0.23f, 0.92f)),
                                14.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + (size.x * 0.5f) - (ImGui::CalcTextSize(icon).x * 0.5f),
                                     pos.y + (size.y * 0.5f) - (ImGui::GetFontSize() * 0.5f)));
    ImGui::TextUnformatted(icon);
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 8.0f));
    ImGui::PopID();
    return pressed;
}

bool togglePill(const char* id, bool* value, const ImVec2& size) {
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##toggle_pill", size);
    if (pressed && value != nullptr) {
        *value = !*value;
    }

    const bool enabled = value != nullptr && *value;
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImRect rect(pos, pos + size);
    const ImU32 trackColor = enabled ? toColorU32(kAccent) : toColorU32(ImVec4(0.18f, 0.18f, 0.26f, 1.0f));
    drawList->AddRectFilled(rect.Min, rect.Max, trackColor, size.y * 0.5f);
    drawList->AddRect(rect.Min, rect.Max, toColorU32(kBorder), size.y * 0.5f);

    const float knobRadius = (size.y * 0.5f) - 4.0f;
    const float knobX = enabled ? rect.Max.x - knobRadius - 4.0f : rect.Min.x + knobRadius + 4.0f;
    const ImU32 knobColor = hovered ? toColorU32(ImVec4(1, 1, 1, 0.95f)) : toColorU32(ImVec4(0.95f, 0.95f, 1.0f, 0.90f));
    drawList->AddCircleFilled(ImVec2(knobX, rect.Min.y + size.y * 0.5f), knobRadius, knobColor, 24);

    ImGui::PopID();
    return pressed;
}

bool chipCombo(const char* id,
               int* currentIndex,
               const char* const* items,
               const int itemCount,
               const ImVec2& size) {
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(size.x);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, size.y * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.11f, 0.11f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.16f, 0.28f, 1.0f));
    const bool changed = ImGui::Combo("##chip_combo", currentIndex, items, itemCount);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

bool sliderBar(const char* id,
               float* value,
               const float minValue,
               const float maxValue,
               const char* suffix) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.16f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.58f, 0.50f, 1.0f, 1.0f));
    const bool changed = ImGui::SliderFloat("##slider", value, minValue, maxValue, suffix);
    ImGui::PopStyleColor(5);
    ImGui::PopID();
    return changed;
}

bool beginCard(const char* id, const ImVec2& size) {
    return BeginPanel(id, nullptr, size);
}

void cardHeader(const char* title, const char* subtitle) {
    ImGui::TextColored(kText, "%s", title);
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::TextColored(kMuted, "%s", subtitle);
    }
    ImGui::Spacing();
}

void endCard() {
    EndPanel();
}

}  // namespace voidcare::app::theme
