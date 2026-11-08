# Mist Design Tokens Specification

This document defines the formal semantic design tokens for the **Mint Material** design system implemented in `apps/desktop/include/MintDesignTokens.h`.

---

## 1. Color Palette Tokens

### 1.1 Core Primitives
```cpp
namespace mist::desktop::tokens {
    struct MintPalette {
        static constexpr ImVec4 PixelMint     { 0.722f, 0.922f, 0.835f, 1.00f }; // #B8EBD5
        static constexpr ImVec4 FreshMint     { 0.557f, 0.847f, 0.729f, 1.00f }; // #8ED8BA
        static constexpr ImVec4 ForestGreen   { 0.090f, 0.294f, 0.220f, 1.00f }; // #174B38
        static constexpr ImVec4 DeepEvergreen { 0.051f, 0.188f, 0.141f, 1.00f }; // #0D3024
        static constexpr ImVec4 CloudBg       { 0.949f, 0.973f, 0.957f, 1.00f }; // #F2F8F4
        static constexpr ImVec4 SoftSage      { 0.867f, 0.937f, 0.894f, 1.00f }; // #DDEFE4
    };
}
```

---

## 2. Semantic Theme Tokens

The application supports real-time switching between `ThemeMode::Light` and `ThemeMode::Dark` without window re-creation.

| Token Key | Light Theme Value | Dark Theme Value | UI Component Mapping |
| :--- | :--- | :--- | :--- |
| `background` | `#F2F8F4` (Cloud) | `#101A15` (Dark Mint) | Main window viewport background |
| `surface` | `#FFFFFF` (White) | `#18231C` (Dark Charcoal) | Top header bar, navigation rail, cards |
| `surfaceVariant` | `#E8F2EC` (Mint White) | `#202E25` (Charcoal Variant) | Metric boxes, nested sub-panels, headers |
| `primary` | `#174B38` (Forest Green) | `#B8EBD5` (Pixel Mint) | Key accents, titles, active badges, checks |
| `onPrimary` | `#FFFFFF` (White) | `#103D2C` (Deep Forest) | Text and icons on primary fills |
| `primaryContainer` | `#DDEFE4` (Soft Sage) | `#244C39` (Pine) | Active navigation selection, code boxes |
| `onPrimaryContainer` | `#123D2C` (Deep Forest) | `#DDF5E8` (Mint Light) | Text rendered inside container fills |
| `secondary` | `#527263` (Sage Gray) | `#B0C7B9` (Pale Sage) | Subheadings, scrollbar thumbs, slider tracks |
| `outline` | `#B8C9BF` (Muted Mint) | `#53675B` (Dark Sage) | Table grid lines, card borders, dividers |
| `textPrimary` | `#0D3024` (Deep Evergreen)| `#F2F8F4` (Light Cloud) | Main body copy, headings, table values |
| `textSecondary` | `#527263` (Sage Gray) | `#B0C7B9` (Pale Sage) | Overlines, technical specs, hints |
| `textDisabled` | `#8C9E94` (Muted Slate) | `#607368` (Muted Dark) | Inactive options, placeholder hints |
| `successContainer` | `#DDEFE4` (Soft Sage) | `#244C39` (Pine) | Success status badges ("ONLINE", "READY") |
| `onSuccessContainer` | `#123D2C` (Dark Forest) | `#DDF5E8` (Mint Light) | Text inside positive status badges |
| `errorContainer` | `#FEE2E2` (Soft Rose) | `#3D1414` (Deep Crimson) | Warning badges, disconnect buttons |
| `onErrorContainer` | `#991B1B` (Dark Crimson) | `#FFCDCD` (Light Rose) | Text on emergency disconnect / deny |

---

## 3. Shape & Geometry Tokens

```cpp
struct MintShapes {
    static constexpr float ExtraLarge = 28.0f; // Modal dialogs, popup sheets
    static constexpr float Large      = 24.0f; // Primary M3 cards, navigation rail
    static constexpr float Medium     = 16.0f; // Buttons, input fields, metric boxes
    static constexpr float Small      = 12.0f; // Scrollbars, chips, sub-tabs
    static constexpr float Pill       = 9999.0f; // Fully rounded pills and badges
};
```

### Component Shape Mappings
- **Cards (`BeginM3CardHelper`):** `ChildRounding = 24.0f`, `ChildBorderSize = 1.0f`.
- **Buttons (`ImGui::Button`):** `FrameRounding = 16.0f`.
- **Text Inputs (`ImGui::InputTextWithHint`):** `FrameRounding = 16.0f`.
- **Status Badges (`renderBadge`):** `Rounding = height * 0.5f` (Pill shape).
- **Metric Cards (`renderMetricBox`):** `ChildRounding = 16.0f`.
- **Popup Dialogs:** `PopupRounding = 28.0f`.

---

## 4. Spacing & Layout Grid Tokens

```cpp
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
```

---

## 5. Interaction State Matrix

| Control Type | Default State | Hovered State | Active / Pressed State |
| :--- | :--- | :--- | :--- |
| **Primary Action** | `controlBg` (`primaryContainer`) | `controlHovered` (`PixelMint`) | `controlActive` (`FreshMint`) |
| **Secondary Button** | Transparent with `outline` | `surfaceVariant` fill | `primaryContainer` fill |
| **Destructive Button** | `errorContainer` fill | High-contrast red fill | Deep red active fill |
| **Navigation Item** | Ghost button (no border) | `surfaceVariant` (0.6 opacity) | `primaryContainer` with `primary` border |
| **Slider / Toggle** | `secondary` track | `primary` glow | `primaryActive` fill |
| **Card Surface** | `cardBg` with `cardBorder` | Static (0 elevation change) | Focus highlight on focusable contents |
