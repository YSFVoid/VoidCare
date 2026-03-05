#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>

#include <functional>

namespace voidcare::app::theme {

struct ThemeMetrics {
    float spacing8 = 8.0f;
    float spacing12 = 12.0f;
    float spacing16 = 16.0f;
    float spacing24 = 24.0f;
    float cardRadius = 16.0f;
    float controlHeight = 35.0f;
};

enum class ChipVariant {
    Neutral,
    Accent,
    Success,
    Warning,
    Danger,
};

void ApplyVoidCareStyle(float dpiScale);
void applyVoidCareStyle(float dpiScale);
void drawBackdrop(const ImVec2& pos, const ImVec2& size);

bool BeginPanel(const char* id,
                const char* title,
                const ImVec2& size = ImVec2(0.0f, 0.0f),
                const char* subtitle = nullptr,
                const std::function<void()>& rightHeaderWidget = {});
void EndPanel();

bool BeginSettingRows(const char* id);
void SettingRow(const char* label, const char* description, const std::function<void()>& drawControlFn);
void EndSettingRows();

void StatusChip(const char* text, ChipVariant variant = ChipVariant::Neutral);

bool sidebarItem(const char* icon, const char* id, bool selected, const ImVec2& size);
bool togglePill(const char* id, bool* value, const ImVec2& size);
bool chipCombo(const char* id, int* currentIndex, const char* const* items, int itemCount, const ImVec2& size);
bool sliderBar(const char* id, float* value, float minValue, float maxValue, const char* suffix);

bool beginCard(const char* id, const ImVec2& size = ImVec2(0.0f, 0.0f));
void cardHeader(const char* title, const char* subtitle = nullptr);
void endCard();

ThemeMetrics metrics(float dpiScale);

}  // namespace voidcare::app::theme
