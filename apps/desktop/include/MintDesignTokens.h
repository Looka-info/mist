#pragma once

#if defined(_WIN32)
#include "imgui.h"
#include <cstdint>

namespace mist::desktop::tokens {

enum class ThemeMode : uint8_t {
    Light = 0,
    Dark  = 1
};

struct MintPalette {
    // Brand Base Palette
    static constexpr ImVec4 PixelMint     { 0.722f, 0.922f, 0.835f, 1.00f }; // #B8EBD5
    static constexpr ImVec4 FreshMint     { 0.557f, 0.847f, 0.729f, 1.00f }; // #8ED8BA
    static constexpr ImVec4 ForestGreen   { 0.090f, 0.294f, 0.220f, 1.00f }; // #174B38
    static constexpr ImVec4 DeepEvergreen { 0.051f, 0.188f, 0.141f, 1.00f }; // #0D3024
    static constexpr ImVec4 CloudBg       { 0.949f, 0.973f, 0.957f, 1.00f }; // #F2F8F4
    static constexpr ImVec4 SoftSage      { 0.867f, 0.937f, 0.894f, 1.00f }; // #DDEFE4
};

struct ThemeColors {
    ImVec4 background;
    ImVec4 surface;
    ImVec4 surfaceVariant;
    ImVec4 primary;
    ImVec4 onPrimary;
    ImVec4 primaryContainer;
    ImVec4 onPrimaryContainer;
    ImVec4 secondary;
    ImVec4 onSecondary;
    ImVec4 outline;
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 textDisabled;
    ImVec4 success;
    ImVec4 successContainer;
    ImVec4 onSuccessContainer;
    ImVec4 error;
    ImVec4 errorContainer;
    ImVec4 onErrorContainer;
    ImVec4 cardBg;
    ImVec4 cardBorder;
    ImVec4 inputBg;
    ImVec4 inputBorder;
    ImVec4 controlBg;
    ImVec4 controlHovered;
    ImVec4 controlActive;
};

// Material Design 3 Shape Hierarchy
struct MintShapes {
    static constexpr float ExtraLarge = 28.0f; // Dialogs, hero sheets
    static constexpr float Large      = 24.0f; // Cards, large surface containers
    static constexpr float Medium     = 16.0f; // Buttons, input fields, controls
    static constexpr float Small      = 12.0f; // Chips, badges, small items
    static constexpr float Pill       = 9999.0f; // Fully-rounded pill buttons/badges
};

// 4px Base Spacing Grid
struct MintSpacing {
    static constexpr float S4  = 4.0f;
    static constexpr float S8  = 8.0f;
    static constexpr float S12 = 12.0f;
    static constexpr float S16 = 16.0f;
    static constexpr float S20 = 20.0f;
    static constexpr float S24 = 24.0f;
    static constexpr float S32 = 32.0f;
    static constexpr float S48 = 48.0f;
};

// Semantic Tokens - Light Theme
inline const ThemeColors LightTheme = {
    /* background         */ ImVec4(0.949f, 0.973f, 0.957f, 1.00f), // #F2F8F4 Cloud
    /* surface            */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF Pure White
    /* surfaceVariant     */ ImVec4(0.910f, 0.949f, 0.925f, 1.00f), // #E8F2EC Soft Mint White
    /* primary            */ ImVec4(0.090f, 0.294f, 0.220f, 1.00f), // #174B38 Forest Green
    /* onPrimary          */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF White
    /* primaryContainer   */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4 Soft Sage
    /* onPrimaryContainer */ ImVec4(0.071f, 0.239f, 0.173f, 1.00f), // #123D2C Dark Forest
    /* secondary          */ ImVec4(0.322f, 0.447f, 0.388f, 1.00f), // #527263 Sage Gray
    /* onSecondary        */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF White
    /* outline            */ ImVec4(0.722f, 0.788f, 0.749f, 1.00f), // #B8C9BF Muted Mint
    /* textPrimary        */ ImVec4(0.051f, 0.188f, 0.141f, 1.00f), // #0D3024 Deep Evergreen
    /* textSecondary      */ ImVec4(0.322f, 0.447f, 0.388f, 1.00f), // #527263
    /* textDisabled       */ ImVec4(0.550f, 0.630f, 0.580f, 1.00f), // Soft Slate
    /* success            */ ImVec4(0.090f, 0.294f, 0.220f, 1.00f), // #174B38
    /* successContainer   */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4
    /* onSuccessContainer */ ImVec4(0.071f, 0.239f, 0.173f, 1.00f), // #123D2C
    /* error              */ ImVec4(0.730f, 0.180f, 0.180f, 1.00f), // #BA1A1A Red
    /* errorContainer     */ ImVec4(0.990f, 0.880f, 0.880f, 1.00f), // Soft Pink
    /* onErrorContainer   */ ImVec4(0.550f, 0.050f, 0.050f, 1.00f), // Deep Red
    /* cardBg             */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF
    /* cardBorder         */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4
    /* inputBg            */ ImVec4(0.910f, 0.949f, 0.925f, 0.75f), // Soft Mint Input
    /* inputBorder        */ ImVec4(0.722f, 0.788f, 0.749f, 1.00f), // Outline
    /* controlBg          */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // Primary Container
    /* controlHovered     */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // Pixel Mint Hover
    /* controlActive      */ ImVec4(0.557f, 0.847f, 0.729f, 1.00f)  // Fresh Mint Active
};

// Semantic Tokens - Dark Theme
inline const ThemeColors DarkTheme = {
    /* background         */ ImVec4(0.063f, 0.102f, 0.082f, 1.00f), // #101A15 Near Black Mint
    /* surface            */ ImVec4(0.094f, 0.137f, 0.110f, 1.00f), // #18231C Dark Charcoal Surface
    /* surfaceVariant     */ ImVec4(0.125f, 0.180f, 0.145f, 1.00f), // #202E25 Charcoal Variant
    /* primary            */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // #B8EBD5 Pixel Mint
    /* onPrimary          */ ImVec4(0.063f, 0.239f, 0.173f, 1.00f), // #103D2C Deep Forest
    /* primaryContainer   */ ImVec4(0.141f, 0.298f, 0.224f, 1.00f), // #244C39 Pine
    /* onPrimaryContainer */ ImVec4(0.867f, 0.961f, 0.910f, 1.00f), // #DDF5E8 Mint Light
    /* secondary          */ ImVec4(0.690f, 0.780f, 0.725f, 1.00f), // #B0C7B9 Pale Sage
    /* onSecondary        */ ImVec4(0.094f, 0.137f, 0.110f, 1.00f), // Dark Charcoal
    /* outline            */ ImVec4(0.325f, 0.404f, 0.357f, 1.00f), // #53675B Dark Sage Outline
    /* textPrimary        */ ImVec4(0.949f, 0.973f, 0.957f, 1.00f), // #F2F8F4 Light Cloud
    /* textSecondary      */ ImVec4(0.690f, 0.780f, 0.725f, 1.00f), // #B0C7B9
    /* textDisabled       */ ImVec4(0.400f, 0.480f, 0.430f, 1.00f), // Muted Dark
    /* success            */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // #B8EBD5
    /* successContainer   */ ImVec4(0.141f, 0.298f, 0.224f, 1.00f), // #244C39
    /* onSuccessContainer */ ImVec4(0.867f, 0.961f, 0.910f, 1.00f), // #DDF5E8
    /* error              */ ImVec4(1.000f, 0.450f, 0.450f, 1.00f), // Light Red
    /* errorContainer     */ ImVec4(0.450f, 0.120f, 0.120f, 1.00f), // Dark Red
    /* onErrorContainer   */ ImVec4(1.000f, 0.850f, 0.850f, 1.00f), // Pale Pink
    /* cardBg             */ ImVec4(0.094f, 0.137f, 0.110f, 1.00f), // #18231C
    /* cardBorder         */ ImVec4(0.200f, 0.280f, 0.230f, 1.00f), // Border
    /* inputBg            */ ImVec4(0.125f, 0.180f, 0.145f, 0.85f), // #202E25
    /* inputBorder        */ ImVec4(0.325f, 0.404f, 0.357f, 1.00f), // #53675B
    /* controlBg          */ ImVec4(0.141f, 0.298f, 0.224f, 1.00f), // #244C39
    /* controlHovered     */ ImVec4(0.200f, 0.380f, 0.290f, 1.00f), // Lighter Green
    /* controlActive      */ ImVec4(0.260f, 0.460f, 0.360f, 1.00f)  // Active Green
};

inline const ThemeColors& getActiveTheme(ThemeMode mode) {
    return (mode == ThemeMode::Light) ? LightTheme : DarkTheme;
}

inline void applyTheme(ThemeMode mode) {
    ImGuiStyle& style = ImGui::GetStyle();
    const ThemeColors& c = getActiveTheme(mode);
    ImVec4* colors = style.Colors;

    // Material 3 Rounded Metrics
    style.WindowRounding    = MintShapes::Large;      // 24px
    style.ChildRounding     = MintShapes::Large;      // 24px
    style.FrameRounding     = MintShapes::Medium;     // 16px
    style.PopupRounding     = MintShapes::ExtraLarge; // 28px
    style.ScrollbarRounding = MintShapes::Small;      // 12px
    style.GrabRounding      = MintShapes::Medium;     // 16px
    style.TabRounding       = MintShapes::Medium;     // 16px

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(MintSpacing::S16, MintSpacing::S16);
    style.FramePadding      = ImVec2(MintSpacing::S12, MintSpacing::S8);
    style.ItemSpacing       = ImVec2(MintSpacing::S12, MintSpacing::S8);
    style.ItemInnerSpacing  = ImVec2(MintSpacing::S8,  MintSpacing::S4);
    style.ScrollbarSize     = MintSpacing::S12;

    colors[ImGuiCol_Text]                  = c.textPrimary;
    colors[ImGuiCol_TextDisabled]          = c.textDisabled;
    colors[ImGuiCol_WindowBg]              = c.background;
    colors[ImGuiCol_ChildBg]               = c.cardBg;
    colors[ImGuiCol_PopupBg]               = c.surface;
    colors[ImGuiCol_Border]                = c.cardBorder;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg]               = c.inputBg;
    colors[ImGuiCol_FrameBgHovered]        = c.controlHovered;
    colors[ImGuiCol_FrameBgActive]         = c.controlActive;
    colors[ImGuiCol_TitleBg]               = c.surface;
    colors[ImGuiCol_TitleBgActive]         = c.surface;
    colors[ImGuiCol_TitleBgCollapsed]      = c.surface;
    colors[ImGuiCol_MenuBarBg]             = c.surface;
    colors[ImGuiCol_ScrollbarBg]           = c.surfaceVariant;
    colors[ImGuiCol_ScrollbarGrab]         = c.secondary;
    colors[ImGuiCol_ScrollbarGrabHovered]  = c.primary;
    colors[ImGuiCol_ScrollbarGrabActive]   = (mode == ThemeMode::Light) ? MintPalette::ForestGreen : MintPalette::PixelMint;
    colors[ImGuiCol_CheckMark]             = c.primary;
    colors[ImGuiCol_SliderGrab]            = c.primary;
    colors[ImGuiCol_SliderGrabActive]      = (mode == ThemeMode::Light) ? MintPalette::ForestGreen : MintPalette::FreshMint;
    colors[ImGuiCol_Button]                = c.controlBg;
    colors[ImGuiCol_ButtonHovered]         = c.controlHovered;
    colors[ImGuiCol_ButtonActive]          = c.controlActive;
    colors[ImGuiCol_Header]                = c.surfaceVariant;
    colors[ImGuiCol_HeaderHovered]         = c.controlHovered;
    colors[ImGuiCol_HeaderActive]          = c.controlActive;
    colors[ImGuiCol_Separator]             = c.outline;
    colors[ImGuiCol_SeparatorHovered]      = c.primary;
    colors[ImGuiCol_SeparatorActive]       = c.primary;
    colors[ImGuiCol_ResizeGrip]            = c.controlBg;
    colors[ImGuiCol_ResizeGripHovered]     = c.controlHovered;
    colors[ImGuiCol_ResizeGripActive]      = c.controlActive;
    colors[ImGuiCol_Tab]                   = c.surfaceVariant;
    colors[ImGuiCol_TabHovered]            = c.controlHovered;
    colors[ImGuiCol_TabActive]             = c.primaryContainer;
    colors[ImGuiCol_TabUnfocused]          = c.surface;
    colors[ImGuiCol_TabUnfocusedActive]    = c.surfaceVariant;
    colors[ImGuiCol_TableHeaderBg]         = c.surfaceVariant;
    colors[ImGuiCol_TableBorderStrong]     = c.outline;
    colors[ImGuiCol_TableBorderLight]      = c.cardBorder;
    colors[ImGuiCol_TableRowBg]            = ImVec4(c.surface.x, c.surface.y, c.surface.z, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(c.surfaceVariant.x, c.surfaceVariant.y, c.surfaceVariant.z, 0.35f);
    colors[ImGuiCol_TextSelectedBg]        = c.primaryContainer;
    colors[ImGuiCol_NavHighlight]          = c.primary;
}

} // namespace mist::desktop::tokens

#endif // _WIN32
