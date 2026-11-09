#pragma once

#if defined(_WIN32)
#include "imgui.h"
#include <cstdint>

namespace mist::desktop::tokens {

enum class ThemeMode : uint8_t {
    Light = 0,
    Dark  = 1
};

// Exact color tokens from Mist.html design specification
struct MintPalette {
    // Light Theme: :root
    // --bg:#f0faf6; --sf:#ffffff; --ct:#bff0dd; --ct2:#e3f6ee; --mint:#46deb5; --onm:#032b20;
    // --acc:#0f8466; --on:#ffffff; --onct:#06382b; --ink:#10231d; --mute:#4f655d; --line:#d4e7df;
    // --ok:#0f8466; --warn:#8a5a00; --bad:#b3261e;
    // --s1:#c2efdc; --s2:#dcf6ec; --s3:#a3e8cb; --s4:#d0f2e4;

    static constexpr ImVec4 LightBg       { 0.941f, 0.980f, 0.965f, 1.00f }; // #f0faf6
    static constexpr ImVec4 LightSurface  { 1.000f, 1.000f, 1.000f, 1.00f }; // #ffffff
    static constexpr ImVec4 LightCt       { 0.749f, 0.941f, 0.867f, 1.00f }; // #bff0dd (Active Container / Pill)
    static constexpr ImVec4 LightCt2      { 0.890f, 0.965f, 0.933f, 1.00f }; // #e3f6ee (Soft Container / Tile)
    static constexpr ImVec4 LightMint     { 0.275f, 0.871f, 0.710f, 1.00f }; // #46deb5 (Vibrant Mint)
    static constexpr ImVec4 LightOnMint   { 0.012f, 0.169f, 0.125f, 1.00f }; // #032b20 (Dark Forest Text)
    static constexpr ImVec4 LightAcc      { 0.059f, 0.518f, 0.400f, 1.00f }; // #0f8466 (Forest Accent Green)
    static constexpr ImVec4 LightOnCt     { 0.024f, 0.220f, 0.169f, 1.00f }; // #06382b (Text on Active Container)
    static constexpr ImVec4 LightInk      { 0.063f, 0.137f, 0.114f, 1.00f }; // #10231d (Primary Text)
    static constexpr ImVec4 LightMute     { 0.310f, 0.396f, 0.365f, 1.00f }; // #4f655d (Muted Secondary Text)
    static constexpr ImVec4 LightLine     { 0.831f, 0.906f, 0.875f, 1.00f }; // #d4e7df (Border Outline)
    static constexpr ImVec4 LightOk       { 0.059f, 0.518f, 0.400f, 1.00f }; // #0f8466
    static constexpr ImVec4 LightWarn     { 0.541f, 0.353f, 0.000f, 1.00f }; // #8a5a00
    static constexpr ImVec4 LightBad      { 0.702f, 0.149f, 0.118f, 1.00f }; // #b3261e
    static constexpr ImVec4 LightS1       { 0.761f, 0.937f, 0.863f, 1.00f }; // #c2efdc
    static constexpr ImVec4 LightS2       { 0.863f, 0.965f, 0.925f, 1.00f }; // #dcf6ec
    static constexpr ImVec4 LightS3       { 0.639f, 0.910f, 0.796f, 1.00f }; // #a3e8cb
    static constexpr ImVec4 LightS4       { 0.816f, 0.949f, 0.894f, 1.00f }; // #d0f2e4

    // Dark Theme: :root[data-theme="dark"]
    // --bg:#0c1915; --sf:#12231e; --mint:#6fdcb6; --onm:#00382a; --ct:#1d3d33; --ct2:#183128;
    // --acc:#6fdcb6; --on:#00382a; --onct:#c9f4e3; --ink:#dcefe8; --mute:#92b2a6; --line:#25423a;
    // --ok:#6fdcb6; --warn:#e5ad45; --bad:#f2867d;
    // --s1:#1f6b57; --s2:#14463a; --s3:#2b9a7c; --s4:#0f3a30;

    static constexpr ImVec4 DarkBg        { 0.047f, 0.098f, 0.082f, 1.00f }; // #0c1915
    static constexpr ImVec4 DarkSurface   { 0.071f, 0.137f, 0.118f, 1.00f }; // #12231e
    static constexpr ImVec4 DarkCt        { 0.114f, 0.239f, 0.200f, 1.00f }; // #1d3d33
    static constexpr ImVec4 DarkCt2       { 0.094f, 0.192f, 0.157f, 1.00f }; // #183128
    static constexpr ImVec4 DarkMint      { 0.435f, 0.863f, 0.714f, 1.00f }; // #6fdcb6
    static constexpr ImVec4 DarkOnMint    { 0.000f, 0.220f, 0.165f, 1.00f }; // #00382a
    static constexpr ImVec4 DarkAcc       { 0.435f, 0.863f, 0.714f, 1.00f }; // #6fdcb6
    static constexpr ImVec4 DarkOnCt      { 0.788f, 0.957f, 0.890f, 1.00f }; // #c9f4e3
    static constexpr ImVec4 DarkInk       { 0.863f, 0.937f, 0.910f, 1.00f }; // #dcefe8
    static constexpr ImVec4 DarkMute      { 0.573f, 0.698f, 0.651f, 1.00f }; // #92b2a6
    static constexpr ImVec4 DarkLine      { 0.145f, 0.259f, 0.227f, 1.00f }; // #25423a
    static constexpr ImVec4 DarkOk        { 0.435f, 0.863f, 0.714f, 1.00f }; // #6fdcb6
    static constexpr ImVec4 DarkWarn      { 0.898f, 0.678f, 0.271f, 1.00f }; // #e5ad45
    static constexpr ImVec4 DarkBad       { 0.949f, 0.525f, 0.490f, 1.00f }; // #f2867d
    static constexpr ImVec4 DarkS1        { 0.122f, 0.420f, 0.341f, 1.00f }; // #1f6b57
    static constexpr ImVec4 DarkS2        { 0.078f, 0.275f, 0.227f, 1.00f }; // #14463a
    static constexpr ImVec4 DarkS3        { 0.169f, 0.604f, 0.486f, 1.00f }; // #2b9a7c
    static constexpr ImVec4 DarkS4        { 0.059f, 0.227f, 0.188f, 1.00f }; // #0f3a30
};

struct ThemeColors {
    ImVec4 bg;            // --bg
    ImVec4 surface;       // --sf
    ImVec4 ct;            // --ct (Active container / pill selection)
    ImVec4 ct2;           // --ct2 (Soft container / tile background)
    ImVec4 mint;          // --mint (Vibrant brand mint)
    ImVec4 onMint;        // --onm (Dark text on mint)
    ImVec4 acc;           // --acc (Accent green)
    ImVec4 on;            // --on (Pure white / dark on accent)
    ImVec4 onCt;          // --onct (Text on active container)
    ImVec4 ink;           // --ink (Primary typography)
    ImVec4 mute;          // --mute (Muted secondary typography)
    ImVec4 line;          // --line (Border lines and dividers)
    ImVec4 ok;            // --ok
    ImVec4 warn;          // --warn
    ImVec4 bad;           // --bad
    ImVec4 s1;            // Ambient shapes
    ImVec4 s2;
    ImVec4 s3;
    ImVec4 s4;

    // Backward-compatible aliases for existing calls
    ImVec4 background;
    ImVec4 primary;
    ImVec4 onPrimary;
    ImVec4 primaryContainer;
    ImVec4 onPrimaryContainer;
    ImVec4 secondary;
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 textDisabled;
    ImVec4 cardBg;
    ImVec4 cardBorder;
    ImVec4 controlBg;
    ImVec4 controlHovered;
    ImVec4 controlActive;
    ImVec4 error;
    ImVec4 errorContainer;
    ImVec4 onErrorContainer;
    ImVec4 success;
    ImVec4 successContainer;
    ImVec4 onSuccessContainer;
    ImVec4 surfaceContainerHigh;
    ImVec4 surfaceContainerLow;
};

// Material 3 & Mint Shape Metrics
struct MintShapes {
    static constexpr float Dialog = 28.0f; // dialog border-radius: 28px
    static constexpr float Card   = 28.0f; // .card border-radius: 28px
    static constexpr float Tile   = 24.0f; // .tile border-radius: 24px
    static constexpr float Control= 16.0f; // .ic border-radius: 16px, inputs: 12-16px
    static constexpr float Small  = 8.0f;  // .chip border-radius: 8px
    static constexpr float Pill   = 99.0f; // .btn, .rail a, .search border-radius: 99px
};

// Typography Scale (Comfortable large modern scale)
struct MintTypography {
    static constexpr float Display  = 40.0f; // Page titles (h1)
    static constexpr float Headline = 28.0f; // Major metrics & key stats
    static constexpr float Title    = 24.0f; // Card headings (h2)
    static constexpr float Section  = 21.0f; // Section titles
    static constexpr float BodyBold = 18.5f; // Button text, emphasized items
    static constexpr float Body     = 17.5f; // Standard text, text fields
    static constexpr float Label    = 15.0f; // Chips, descriptions, sublabels
    static constexpr float Badge    = 13.5f; // Badges
    static constexpr float Code     = 42.0f; // Pairing code display
};

// Spacing Grid
struct MintSpacing {
    static constexpr float S4  = 4.0f;
    static constexpr float S6  = 6.0f;
    static constexpr float S8  = 8.0f;
    static constexpr float S10 = 10.0f;
    static constexpr float S12 = 12.0f;
    static constexpr float S14 = 14.0f;
    static constexpr float S16 = 16.0f;
    static constexpr float S20 = 20.0f;
    static constexpr float S22 = 22.0f;
    static constexpr float S24 = 24.0f;
    static constexpr float S28 = 28.0f;
    static constexpr float S32 = 32.0f;
};

// Theme Definitions
inline const ThemeColors LightTheme = {
    MintPalette::LightBg,       // bg
    MintPalette::LightSurface,  // surface
    MintPalette::LightCt,       // ct
    MintPalette::LightCt2,      // ct2
    MintPalette::LightMint,     // mint
    MintPalette::LightOnMint,   // onMint
    MintPalette::LightAcc,      // acc
    ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // on
    MintPalette::LightOnCt,     // onCt
    MintPalette::LightInk,      // ink
    MintPalette::LightMute,     // mute
    MintPalette::LightLine,     // line
    MintPalette::LightOk,       // ok
    MintPalette::LightWarn,     // warn
    MintPalette::LightBad,      // bad
    MintPalette::LightS1,       // s1
    MintPalette::LightS2,       // s2
    MintPalette::LightS3,       // s3
    MintPalette::LightS4,       // s4

    // Aliases
    MintPalette::LightBg,       // background
    MintPalette::LightAcc,      // primary
    ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // onPrimary
    MintPalette::LightCt,       // primaryContainer
    MintPalette::LightOnCt,     // onPrimaryContainer
    MintPalette::LightMute,     // secondary
    MintPalette::LightInk,      // textPrimary
    MintPalette::LightMute,     // textSecondary
    MintPalette::LightMute,     // textDisabled
    MintPalette::LightSurface,  // cardBg
    MintPalette::LightLine,     // cardBorder
    MintPalette::LightCt2,      // controlBg
    MintPalette::LightCt,       // controlHovered
    MintPalette::LightMint,     // controlActive
    MintPalette::LightBad,      // error
    ImVec4(0.996f, 0.886f, 0.886f, 1.00f), // errorContainer
    MintPalette::LightBad,      // onErrorContainer
    MintPalette::LightOk,       // success
    MintPalette::LightCt,       // successContainer
    MintPalette::LightOnCt,     // onSuccessContainer
    MintPalette::LightCt,       // surfaceContainerHigh
    MintPalette::LightCt2       // surfaceContainerLow
};

inline const ThemeColors DarkTheme = {
    MintPalette::DarkBg,        // bg
    MintPalette::DarkSurface,   // surface
    MintPalette::DarkCt,        // ct
    MintPalette::DarkCt2,       // ct2
    MintPalette::DarkMint,      // mint
    MintPalette::DarkOnMint,    // onMint
    MintPalette::DarkAcc,       // acc
    ImVec4(0.0f, 0.220f, 0.165f, 1.0f), // on
    MintPalette::DarkOnCt,      // onCt
    MintPalette::DarkInk,       // ink
    MintPalette::DarkMute,      // mute
    MintPalette::DarkLine,      // line
    MintPalette::DarkOk,        // ok
    MintPalette::DarkWarn,      // warn
    MintPalette::DarkBad,       // bad
    MintPalette::DarkS1,        // s1
    MintPalette::DarkS2,        // s2
    MintPalette::DarkS3,        // s3
    MintPalette::DarkS4,        // s4

    // Aliases
    MintPalette::DarkBg,        // background
    MintPalette::DarkAcc,       // primary
    ImVec4(0.0f, 0.220f, 0.165f, 1.0f), // onPrimary
    MintPalette::DarkCt,        // primaryContainer
    MintPalette::DarkOnCt,      // onPrimaryContainer
    MintPalette::DarkMute,      // secondary
    MintPalette::DarkInk,       // textPrimary
    MintPalette::DarkMute,      // textSecondary
    MintPalette::DarkMute,      // textDisabled
    MintPalette::DarkSurface,   // cardBg
    MintPalette::DarkLine,      // cardBorder
    MintPalette::DarkCt2,       // controlBg
    MintPalette::DarkCt,        // controlHovered
    MintPalette::DarkMint,      // controlActive
    MintPalette::DarkBad,       // error
    ImVec4(0.380f, 0.118f, 0.118f, 1.00f), // errorContainer
    ImVec4(1.000f, 0.850f, 0.850f, 1.00f), // onErrorContainer
    MintPalette::DarkOk,        // success
    MintPalette::DarkCt,        // successContainer
    MintPalette::DarkOnCt,      // onSuccessContainer
    MintPalette::DarkCt,        // surfaceContainerHigh
    MintPalette::DarkCt2        // surfaceContainerLow
};

inline const ThemeColors& getActiveTheme(ThemeMode mode) {
    return (mode == ThemeMode::Light) ? LightTheme : DarkTheme;
}

inline void applyTheme(ThemeMode mode, bool highContrast = false) {
    ImGuiStyle& style = ImGui::GetStyle();
    const ThemeColors& c = getActiveTheme(mode);
    ImVec4* colors = style.Colors;

    // Material 3 Rounded Metrics from HTML mockup
    style.WindowRounding    = 0.0f;                    // Window canvas fills screen
    style.ChildRounding     = MintShapes::Card;       // 28px card rounding
    style.FrameRounding     = MintShapes::Control;    // 16px
    style.PopupRounding     = MintShapes::Dialog;     // 28px dialogs
    style.ScrollbarRounding = MintShapes::Control;
    style.GrabRounding      = MintShapes::Pill;
    style.TabRounding       = MintShapes::Control;

    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(16.0f, 10.0f);
    style.ItemSpacing       = ImVec2(12.0f, 12.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f,  6.0f);
    style.ScrollbarSize     = 10.0f;

    ImVec4 lineCol = highContrast ? c.mute : c.line;
    ImVec4 muteCol = highContrast ? c.ink : c.mute;

    colors[ImGuiCol_Text]                  = c.ink;
    colors[ImGuiCol_TextDisabled]          = muteCol;
    colors[ImGuiCol_WindowBg]              = c.bg;
    colors[ImGuiCol_ChildBg]               = c.surface;
    colors[ImGuiCol_PopupBg]               = c.surface;
    colors[ImGuiCol_Border]                = lineCol;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]               = c.ct2;
    colors[ImGuiCol_FrameBgHovered]        = c.ct;
    colors[ImGuiCol_FrameBgActive]         = c.ct;
    colors[ImGuiCol_TitleBg]               = c.surface;
    colors[ImGuiCol_TitleBgActive]         = c.surface;
    colors[ImGuiCol_TitleBgCollapsed]      = c.surface;
    colors[ImGuiCol_MenuBarBg]             = c.surface;
    colors[ImGuiCol_ScrollbarBg]           = c.ct2;
    colors[ImGuiCol_ScrollbarGrab]         = c.mute;
    colors[ImGuiCol_ScrollbarGrabHovered]  = c.acc;
    colors[ImGuiCol_ScrollbarGrabActive]   = c.mint;
    colors[ImGuiCol_CheckMark]             = c.acc;
    colors[ImGuiCol_SliderGrab]            = c.acc;
    colors[ImGuiCol_SliderGrabActive]      = c.mint;
    colors[ImGuiCol_Button]                = c.ct2;
    colors[ImGuiCol_ButtonHovered]         = c.ct;
    colors[ImGuiCol_ButtonActive]          = c.mint;
    colors[ImGuiCol_Header]                = c.ct;
    colors[ImGuiCol_HeaderHovered]         = c.ct;
    colors[ImGuiCol_HeaderActive]          = c.mint;
    colors[ImGuiCol_Separator]             = lineCol;
    colors[ImGuiCol_SeparatorHovered]      = c.acc;
    colors[ImGuiCol_SeparatorActive]       = c.acc;
    colors[ImGuiCol_ResizeGrip]            = c.ct;
    colors[ImGuiCol_ResizeGripHovered]     = c.mint;
    colors[ImGuiCol_ResizeGripActive]      = c.acc;
    colors[ImGuiCol_Tab]                   = c.ct2;
    colors[ImGuiCol_TabHovered]            = c.ct;
    colors[ImGuiCol_TabActive]             = c.ct;
    colors[ImGuiCol_TabUnfocused]          = c.surface;
    colors[ImGuiCol_TabUnfocusedActive]    = c.ct2;
    colors[ImGuiCol_TableHeaderBg]         = c.ct2;
    colors[ImGuiCol_TableBorderStrong]     = lineCol;
    colors[ImGuiCol_TableBorderLight]      = lineCol;
    colors[ImGuiCol_TableRowBg]            = ImVec4(c.surface.x, c.surface.y, c.surface.z, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(c.ct2.x, c.ct2.y, c.ct2.z, 0.5f);
    colors[ImGuiCol_TextSelectedBg]        = c.ct;
    colors[ImGuiCol_NavHighlight]          = c.acc;
}

} // namespace mist::desktop::tokens

#endif // _WIN32
