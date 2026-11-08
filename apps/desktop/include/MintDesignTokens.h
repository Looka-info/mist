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
    ImVec4 surfaceContainerLow;
    ImVec4 surfaceContainer;
    ImVec4 surfaceContainerHigh;
    ImVec4 surfaceContainerHighest;
    ImVec4 primary;
    ImVec4 onPrimary;
    ImVec4 primaryContainer;
    ImVec4 onPrimaryContainer;
    ImVec4 secondary;
    ImVec4 onSecondary;
    ImVec4 secondaryContainer;
    ImVec4 onSecondaryContainer;
    ImVec4 outline;
    ImVec4 outlineVariant;
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

// Material Design 3 Shape Hierarchy (Derived from reference design)
struct MintShapes {
    static constexpr float ExtraLarge = 28.0f; // Modal dialogs, popup sheets (36px in modal)
    static constexpr float Large      = 24.0f; // Cards, large surface containers
    static constexpr float Medium     = 16.0f; // Buttons, input fields, controls
    static constexpr float Small      = 12.0f; // Chips, badges, small items
    static constexpr float Pill       = 9999.0f; // Fully-rounded pill buttons/badges (100px)
};

// Typography Scale (Derived from reference design: Google Sans / Segoe UI)
struct MintTypography {
    static constexpr float Display  = 32.0f; // Hero Title
    static constexpr float Headline = 24.0f; // Page Titles
    static constexpr float Title    = 20.0f; // Card headers, section titles
    static constexpr float BodyBold = 16.0f; // Emphasized text
    static constexpr float Body     = 15.0f; // Default standard text
    static constexpr float Label    = 12.0f; // Overlines, badges, tags
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

// Semantic Tokens - Light Theme (Mint & White Material)
inline const ThemeColors LightTheme = {
    /* background              */ ImVec4(0.949f, 0.973f, 0.957f, 1.00f), // #F2F8F4 Cloud Mint Canvas
    /* surface                 */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF Pure Crisp White
    /* surfaceContainerLow     */ ImVec4(0.965f, 0.980f, 0.969f, 1.00f), // #F6FAF7 Low Surface
    /* surfaceContainer        */ ImVec4(0.933f, 0.961f, 0.945f, 1.00f), // #EEF5F1 Standard Surface Container
    /* surfaceContainerHigh    */ ImVec4(0.898f, 0.941f, 0.914f, 1.00f), // #E5F0E9 High Surface Container
    /* surfaceContainerHighest */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4 Highest Surface / Soft Sage
    /* primary                 */ ImVec4(0.090f, 0.294f, 0.220f, 1.00f), // #174B38 Forest Green
    /* onPrimary               */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF White
    /* primaryContainer        */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4 Soft Sage Container
    /* onPrimaryContainer      */ ImVec4(0.051f, 0.188f, 0.141f, 1.00f), // #0D3024 Deep Evergreen Text
    /* secondary               */ ImVec4(0.322f, 0.447f, 0.388f, 1.00f), // #527263 Sage Gray
    /* onSecondary             */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF White
    /* secondaryContainer      */ ImVec4(0.886f, 0.925f, 0.898f, 1.00f), // #E2ECE5
    /* onSecondaryContainer    */ ImVec4(0.118f, 0.208f, 0.169f, 1.00f), // #1E352B
    /* outline                 */ ImVec4(0.722f, 0.788f, 0.749f, 1.00f), // #B8C9BF Muted Mint
    /* outlineVariant          */ ImVec4(0.835f, 0.886f, 0.851f, 1.00f), // #D5E2D9 Subtle Divider
    /* textPrimary             */ ImVec4(0.051f, 0.188f, 0.141f, 1.00f), // #0D3024 Deep Evergreen
    /* textSecondary           */ ImVec4(0.322f, 0.447f, 0.388f, 1.00f), // #527263 Sage
    /* textDisabled            */ ImVec4(0.541f, 0.620f, 0.576f, 1.00f), // #8A9E93
    /* success                 */ ImVec4(0.090f, 0.294f, 0.220f, 1.00f), // #174B38
    /* successContainer        */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // #DDEFE4
    /* onSuccessContainer      */ ImVec4(0.051f, 0.188f, 0.141f, 1.00f), // #0D3024
    /* error                   */ ImVec4(0.730f, 0.180f, 0.180f, 1.00f), // #BA1A1A Red
    /* errorContainer          */ ImVec4(0.996f, 0.886f, 0.886f, 1.00f), // #FEE2E2 Soft Pink
    /* onErrorContainer        */ ImVec4(0.600f, 0.106f, 0.106f, 1.00f), // #991B1B Deep Red
    /* cardBg                  */ ImVec4(1.000f, 1.000f, 1.000f, 1.00f), // #FFFFFF Pure White
    /* cardBorder              */ ImVec4(0.835f, 0.886f, 0.851f, 1.00f), // #D5E2D9
    /* inputBg                 */ ImVec4(0.933f, 0.961f, 0.945f, 0.85f), // Soft Mint Input
    /* inputBorder             */ ImVec4(0.722f, 0.788f, 0.749f, 1.00f), // Outline
    /* controlBg               */ ImVec4(0.867f, 0.937f, 0.894f, 1.00f), // Soft Sage Fill
    /* controlHovered          */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // Pixel Mint Hover
    /* controlActive           */ ImVec4(0.557f, 0.847f, 0.729f, 1.00f)  // Fresh Mint Active
};

// Semantic Tokens - Dark Theme (Deep Charcoal Mint with Material You Surface Containers)
inline const ThemeColors DarkTheme = {
    /* background              */ ImVec4(0.055f, 0.082f, 0.067f, 1.00f), // #0E1511 Deep Dark Canvas
    /* surface                 */ ImVec4(0.082f, 0.125f, 0.102f, 1.00f), // #15201A Elevated Card Surface
    /* surfaceContainerLow     */ ImVec4(0.067f, 0.102f, 0.082f, 1.00f), // #111A15 Low Surface
    /* surfaceContainer        */ ImVec4(0.094f, 0.145f, 0.118f, 1.00f), // #18251E Container Surface
    /* surfaceContainerHigh    */ ImVec4(0.125f, 0.192f, 0.157f, 1.00f), // #203128 High Surface
    /* surfaceContainerHighest */ ImVec4(0.157f, 0.243f, 0.200f, 1.00f), // #283E33 Highest Surface
    /* primary                 */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // #B8EBD5 Pixel Mint
    /* onPrimary               */ ImVec4(0.051f, 0.188f, 0.141f, 1.00f), // #0D3024 Deep Evergreen Text
    /* primaryContainer        */ ImVec4(0.133f, 0.282f, 0.212f, 1.00f), // #224836 Pine Container
    /* onPrimaryContainer      */ ImVec4(0.867f, 0.961f, 0.910f, 1.00f), // #DDF5E8 Mint Light Text
    /* secondary               */ ImVec4(0.690f, 0.780f, 0.725f, 1.00f), // #B0C7B9 Pale Sage
    /* onSecondary             */ ImVec4(0.082f, 0.125f, 0.102f, 1.00f), // Dark Charcoal
    /* secondaryContainer      */ ImVec4(0.161f, 0.231f, 0.192f, 1.00f), // #293B31
    /* onSecondaryContainer    */ ImVec4(0.878f, 0.933f, 0.898f, 1.00f), // #E0EEE5
    /* outline                 */ ImVec4(0.251f, 0.349f, 0.294f, 1.00f), // #40594B Restrained Sage
    /* outlineVariant          */ ImVec4(0.169f, 0.239f, 0.204f, 1.00f), // #2B3D34 Subtle Inner Line
    /* textPrimary             */ ImVec4(0.949f, 0.973f, 0.957f, 1.00f), // #F2F8F4 Light Cloud
    /* textSecondary           */ ImVec4(0.690f, 0.780f, 0.725f, 1.00f), // #B0C7B9
    /* textDisabled            */ ImVec4(0.365f, 0.459f, 0.408f, 1.00f), // #5D7568
    /* success                 */ ImVec4(0.722f, 0.922f, 0.835f, 1.00f), // #B8EBD5
    /* successContainer        */ ImVec4(0.133f, 0.282f, 0.212f, 1.00f), // #224836
    /* onSuccessContainer      */ ImVec4(0.867f, 0.961f, 0.910f, 1.00f), // #DDF5E8
    /* error                   */ ImVec4(1.000f, 0.450f, 0.450f, 1.00f), // Light Red
    /* errorContainer          */ ImVec4(0.380f, 0.118f, 0.118f, 1.00f), // Deep Crimson
    /* onErrorContainer        */ ImVec4(1.000f, 0.850f, 0.850f, 1.00f), // Pale Pink
    /* cardBg                  */ ImVec4(0.082f, 0.125f, 0.102f, 1.00f), // #15201A Elevated Surface
    /* cardBorder              */ ImVec4(0.251f, 0.349f, 0.294f, 1.00f), // #40594B
    /* inputBg                 */ ImVec4(0.125f, 0.192f, 0.157f, 0.85f), // #203128 High Surface
    /* inputBorder             */ ImVec4(0.251f, 0.349f, 0.294f, 1.00f), // Outline
    /* controlBg               */ ImVec4(0.133f, 0.282f, 0.212f, 1.00f), // #224836
    /* controlHovered          */ ImVec4(0.180f, 0.360f, 0.270f, 1.00f), // Lighter Green
    /* controlActive           */ ImVec4(0.240f, 0.450f, 0.340f, 1.00f)  // Active Green
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
    colors[ImGuiCol_ScrollbarBg]           = c.surfaceContainerLow;
    colors[ImGuiCol_ScrollbarGrab]         = c.secondary;
    colors[ImGuiCol_ScrollbarGrabHovered]  = c.primary;
    colors[ImGuiCol_ScrollbarGrabActive]   = (mode == ThemeMode::Light) ? MintPalette::ForestGreen : MintPalette::PixelMint;
    colors[ImGuiCol_CheckMark]             = c.primary;
    colors[ImGuiCol_SliderGrab]            = c.primary;
    colors[ImGuiCol_SliderGrabActive]      = (mode == ThemeMode::Light) ? MintPalette::ForestGreen : MintPalette::FreshMint;
    colors[ImGuiCol_Button]                = c.controlBg;
    colors[ImGuiCol_ButtonHovered]         = c.controlHovered;
    colors[ImGuiCol_ButtonActive]          = c.controlActive;
    colors[ImGuiCol_Header]                = c.surfaceContainerHigh;
    colors[ImGuiCol_HeaderHovered]         = c.controlHovered;
    colors[ImGuiCol_HeaderActive]          = c.controlActive;
    colors[ImGuiCol_Separator]             = c.outlineVariant;
    colors[ImGuiCol_SeparatorHovered]      = c.primary;
    colors[ImGuiCol_SeparatorActive]       = c.primary;
    colors[ImGuiCol_ResizeGrip]            = c.controlBg;
    colors[ImGuiCol_ResizeGripHovered]     = c.controlHovered;
    colors[ImGuiCol_ResizeGripActive]      = c.controlActive;
    colors[ImGuiCol_Tab]                   = c.surfaceContainerHigh;
    colors[ImGuiCol_TabHovered]            = c.controlHovered;
    colors[ImGuiCol_TabActive]             = c.primaryContainer;
    colors[ImGuiCol_TabUnfocused]          = c.surface;
    colors[ImGuiCol_TabUnfocusedActive]    = c.surfaceContainerHigh;
    colors[ImGuiCol_TableHeaderBg]         = c.surfaceContainerHigh;
    colors[ImGuiCol_TableBorderStrong]     = c.outline;
    colors[ImGuiCol_TableBorderLight]      = c.outlineVariant;
    colors[ImGuiCol_TableRowBg]            = ImVec4(c.surface.x, c.surface.y, c.surface.z, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(c.surfaceContainerLow.x, c.surfaceContainerLow.y, c.surfaceContainerLow.z, 0.45f);
    colors[ImGuiCol_TextSelectedBg]        = c.primaryContainer;
    colors[ImGuiCol_NavHighlight]          = c.primary;
}

} // namespace mist::desktop::tokens

#endif // _WIN32
