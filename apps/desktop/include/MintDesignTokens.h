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
// Exact Material Design 3 Google Pixel palette for Mist
struct MintPalette {
    // Pixel Mint:     #B8EBD5 (Primary action & brand highlight)
    // Fresh Mint:     #8ED8BA (Vibrant secondary mint)
    // Forest Green:   #174B38 (Emphasis & primary dark green)
    // Deep Evergreen: #0D3024 (Deepest contrast & primary text)
    // Cloud:          #F2F8F4 (Calm light background)
    // Soft Sage:      #DDEFE4 (Soft light surface container)
    // Dark background:#101A15 (Pitch dark canvas)
    // Dark surface:   #18231C (Elevated dark card surface)
    // Dark variant:   #202E25 (Soft dark container)

    static constexpr ImVec4 PixelMint     { 0.722f, 0.922f, 0.835f, 1.00f }; // #b8ebd5
    static constexpr ImVec4 FreshMint     { 0.557f, 0.847f, 0.729f, 1.00f }; // #8ed8ba
    static constexpr ImVec4 ForestGreen   { 0.090f, 0.294f, 0.220f, 1.00f }; // #174b38
    static constexpr ImVec4 DeepEvergreen { 0.051f, 0.188f, 0.141f, 1.00f }; // #0d3024
    static constexpr ImVec4 Cloud         { 0.949f, 0.973f, 0.957f, 1.00f }; // #f2f8f4
    static constexpr ImVec4 SoftSage      { 0.867f, 0.937f, 0.894f, 1.00f }; // #ddefe4
    static constexpr ImVec4 DarkBg        { 0.063f, 0.102f, 0.082f, 1.00f }; // #101a15
    static constexpr ImVec4 DarkSurface   { 0.094f, 0.137f, 0.110f, 1.00f }; // #18231c
    static constexpr ImVec4 DarkVariant   { 0.125f, 0.180f, 0.145f, 1.00f }; // #202e25

    // Light Theme
    static constexpr ImVec4 LightBg       { 0.949f, 0.973f, 0.957f, 1.00f }; // Cloud #f2f8f4
    static constexpr ImVec4 LightSurface  { 1.000f, 1.000f, 1.000f, 1.00f }; // White card
    static constexpr ImVec4 LightCt       { 0.722f, 0.922f, 0.835f, 1.00f }; // Pixel Mint #b8ebd5
    static constexpr ImVec4 LightCt2      { 0.867f, 0.937f, 0.894f, 1.00f }; // Soft Sage #ddefe4
    static constexpr ImVec4 LightMint     { 0.557f, 0.847f, 0.729f, 1.00f }; // Fresh Mint #8ed8ba
    static constexpr ImVec4 LightOnMint   { 0.051f, 0.188f, 0.141f, 1.00f }; // Deep Evergreen #0d3024
    static constexpr ImVec4 LightAcc      { 0.090f, 0.294f, 0.220f, 1.00f }; // Forest Green #174b38
    static constexpr ImVec4 LightOnCt     { 0.051f, 0.188f, 0.141f, 1.00f }; // Deep Evergreen #0d3024
    static constexpr ImVec4 LightInk      { 0.051f, 0.188f, 0.141f, 1.00f }; // Deep Evergreen primary text
    static constexpr ImVec4 LightMute     { 0.260f, 0.370f, 0.320f, 1.00f }; // Muted sage text
    static constexpr ImVec4 LightLine     { 0.768f, 0.870f, 0.812f, 1.00f }; // Border outline
    static constexpr ImVec4 LightOk       { 0.090f, 0.440f, 0.320f, 1.00f }; // Success green
    static constexpr ImVec4 LightWarn     { 0.650f, 0.420f, 0.050f, 1.00f }; // Amber warning
    static constexpr ImVec4 LightBad      { 0.720f, 0.150f, 0.150f, 1.00f }; // Error red
    static constexpr ImVec4 LightS1       { 0.722f, 0.922f, 0.835f, 1.00f };
    static constexpr ImVec4 LightS2       { 0.867f, 0.937f, 0.894f, 1.00f };
    static constexpr ImVec4 LightS3       { 0.557f, 0.847f, 0.729f, 1.00f };
    static constexpr ImVec4 LightS4       { 0.800f, 0.910f, 0.850f, 1.00f };

    // Dark Theme
    static constexpr ImVec4 DarkCt        { 0.090f, 0.294f, 0.220f, 1.00f }; // Forest Green #174b38
    static constexpr ImVec4 DarkCt2       { 0.125f, 0.180f, 0.145f, 1.00f }; // Dark Variant #202e25
    static constexpr ImVec4 DarkMint      { 0.557f, 0.847f, 0.729f, 1.00f }; // Fresh Mint #8ed8ba
    static constexpr ImVec4 DarkOnMint    { 0.051f, 0.188f, 0.141f, 1.00f }; // Deep Evergreen text on mint
    static constexpr ImVec4 DarkAcc       { 0.722f, 0.922f, 0.835f, 1.00f }; // Pixel Mint accent
    static constexpr ImVec4 DarkOnCt      { 0.722f, 0.922f, 0.835f, 1.00f }; // Pixel Mint text on active container
    static constexpr ImVec4 DarkInk       { 0.949f, 0.973f, 0.957f, 1.00f }; // Cloud #f2f8f4 text
    static constexpr ImVec4 DarkMute      { 0.600f, 0.730f, 0.670f, 1.00f }; // Soft Sage muted text
    static constexpr ImVec4 DarkLine      { 0.160f, 0.250f, 0.200f, 1.00f }; // Dark line
    static constexpr ImVec4 DarkOk        { 0.557f, 0.847f, 0.729f, 1.00f };
    static constexpr ImVec4 DarkWarn      { 0.898f, 0.678f, 0.271f, 1.00f };
    static constexpr ImVec4 DarkBad       { 0.949f, 0.525f, 0.490f, 1.00f };
    static constexpr ImVec4 DarkS1        { 0.090f, 0.294f, 0.220f, 1.00f };
    static constexpr ImVec4 DarkS2        { 0.051f, 0.188f, 0.141f, 1.00f };
    static constexpr ImVec4 DarkS3        { 0.125f, 0.180f, 0.145f, 1.00f };
    static constexpr ImVec4 DarkS4        { 0.070f, 0.220f, 0.160f, 1.00f };
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

// Material 3 Shape Metrics
struct MintShapes {
    static constexpr float ExtraSmall = 4.0f;
    static constexpr float Small      = 8.0f;  // Chips, badges, small items
    static constexpr float Medium     = 12.0f; // Text fields, menus, cards
    static constexpr float Large      = 16.0f; // Action tiles, floating HUDs
    static constexpr float ExtraLarge = 28.0f; // Large cards, dialogs, modals
    static constexpr float Full       = 999.0f;// Pill buttons, switches, rail indicators

    // Backward-compatible aliases
    static constexpr float Dialog     = ExtraLarge;
    static constexpr float Card       = ExtraLarge;
    static constexpr float Tile       = Large;
    static constexpr float Control    = Medium;
    static constexpr float Pill       = Full;
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
