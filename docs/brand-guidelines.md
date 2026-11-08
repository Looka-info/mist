# Mist Brand Identity & Design System: Mint Material

## 1. Brand Essence

- **Brand:** Mist
- **Product:** Native Windows High-Performance Game-Streaming Platform
- **Primary Tagline:** *Play beyond your desk.*
- **Supporting Message:** *Your PC. Your games. Your freedom.*
- **Brand Personality:**
  - **Calm:** Soothing mint surfaces and restrained palette reduce cognitive fatigue during prolonged gaming and management sessions.
  - **Modern:** Geometric precision, generous corner radii, and semantic tokens inspired by Google Pixel and Material Design 3.
  - **Trustworthy:** Transparent, truthful metrics without fabricated telemetry or deceptive progress bars.
  - **Responsive:** Real-time feedback across input controls, theme changes, and hardware streaming states.
  - **Premium:** Uncluttered layouts, balanced negative space, and disciplined contrast hierarchies.
  - **Technically Sophisticated:** Direct3D 11, hardware NVENC, WASAPI audio, and WebRTC peer-to-peer transport presented clearly and honestly.

---

## 2. Visual Identity & Logo System

The Mist brand mark is an abstract geometric **"M"** expressing **twin connected streaming paths**:

```
        Host Pillar               Client Pillar
          (Upload)                  (Playout)
            | |                         | |
            | |====== Upper Bridge =====| |
            | |  \                   /  | |
            | |   \                 /   | |
            | |    \    [Nexus]    /    | |
            | |     \     < >     /     | |
            | |      \           /      | |
            ===       ===========       ===
```

### 2.1 Geometric Components
1. **Host Upload Pillar (Left):** Represents high-throughput, low-latency GPU frame capture and NVENC encoding from the host workstation. Rendered in **Pixel Mint (`#B8EBD5`)**.
2. **Client Playout Pillar (Right):** Represents direct Direct3D 11 NVDEC decoding, presentation, and low-latency input capture on client hardware. Rendered in **Fresh Mint (`#8ED8BA`)**.
3. **Twin Diagonal Vectors:** Symmetrical converging paths meeting in the center valley, visualizing bidirectional media and input traffic.
4. **Interlocking Upper Bridge:** Horizontal connection line binding host and client into an unmistakable geometric "M".
5. **Sync Nexus Node:** Central diamond crest symbolizing instantaneous WebRTC peer-to-peer state alignment and sub-frame synchronization.
6. **Ultra-Low Latency Core:** Center focal point denoting precision motion-to-photon delivery under 16.6ms.

### 2.2 Brand Assets
All brand assets are generated deterministically via `scripts/generate-icon.ps1`:
- `resources/mist-logo.png`: 256x256 high-resolution primary application branding.
- `resources/mist-logo-light.png`: Light theme presentation variant on Cloud background.
- `resources/mist-logo-dark.png`: Dark theme presentation variant on Deep Evergreen background.
- `resources/mist-logo-mono.png`: High-contrast monochrome presentation variant.
- `resources/mist.ico`: Multi-resolution Windows application icon embedding 256, 64, 48, 32, and 16 px PNG streams for taskbar, title bar, shortcuts, and installer metadata.

---

## 3. Color System: Mint Material

The color system avoids harsh neon greens, jarring contrast spikes, and excessive glassmorphism. It uses subtle mint tints balanced by authoritative forest-green anchors.

### 3.1 Primary Palette
| Token Name | Hex Code | RGB Normal (0-1) | Purpose |
| :--- | :--- | :--- | :--- |
| **Pixel Mint** | `#B8EBD5` | `(0.722, 0.922, 0.835)` | Primary brand highlight, dark theme accent, host pillar |
| **Fresh Mint** | `#8ED8BA` | `(0.557, 0.847, 0.729)` | Active controls, client pillar, secondary accent |
| **Forest Green** | `#174B38` | `(0.090, 0.294, 0.220)` | Light theme primary anchor, brand typography, badges |
| **Deep Evergreen** | `#0D3024` | `(0.051, 0.188, 0.141)` | Dark theme baseline canvas, high-contrast text |
| **Cloud Background** | `#F2F8F4` | `(0.949, 0.973, 0.957)` | Light theme canvas surface, calming clean backdrop |
| **Soft Sage** | `#DDEFE4` | `(0.867, 0.937, 0.894)` | Primary containers, subtle borders, card separators |

### 3.2 Light Theme Semantics
- **Background (`#F2F8F4`):** Soft cloud canvas.
- **Surface (`#FFFFFF`):** High-clarity white card backgrounds.
- **Surface Variant (`#E8F2EC`):** Elevated utility sections, metrics, and nested cards.
- **Primary (`#174B38`):** Authoritative forest green for brand headings, primary buttons, and selected states.
- **On Primary (`#FFFFFF`):** Pure white text on primary fills.
- **Primary Container (`#DDEFE4`):** Soft sage highlight behind active navigation pills and code boxes.
- **On Primary Container (`#123D2C`):** High-legibility deep forest text on containers.
- **Secondary (`#527263`):** Subdued headers, inactive controls, and body secondary text.
- **Outline (`#B8C9BF`):** Soft mint-gray card and table borders.

### 3.3 Dark Theme Semantics
- **Background (`#101A15`):** Near-black charcoal mint eliminating glare.
- **Surface (`#18231C`):** Elevated charcoal surfaces for cards and header bar.
- **Surface Variant (`#202E25`):** Deep sage containers for nested controls and inputs.
- **Primary (`#B8EBD5`):** Pixel Mint luminous accent for brand focus and active states.
- **On Primary (`#103D2C`):** High-contrast deep evergreen text on mint buttons.
- **Primary Container (`#244C39`):** Subdued pine container for active chips and selected profile cards.
- **On Primary Container (`#DDF5E8`):** Radiant mint text on container elements.
- **Secondary (`#B0C7B9`):** Pale sage secondary labels and disabled states.
- **Outline (`#53675B`):** Restrained sage borders maintaining card structure without visual noise.

---

## 4. Typography Scale

Mist standardizes typography hierarchy using clean, legible sans-serif type (Roboto / Segoe UI on Windows):

| Level | Size | Weight | Line Spacing | Intended Application |
| :--- | :--- | :--- | :--- | :--- |
| **Display** | 36px | Bold (700) | 44px | Product brand hero title |
| **Headline** | 28px | SemiBold (600) | 36px | Primary screen titles ("Play beyond your desk.") |
| **Title** | 20px | Medium (500) | 28px | Section headers, card titles, pairing code display |
| **Body Large** | 16px | Regular (400) | 24px | Primary descriptions, host URLs, status values |
| **Body** | 14px | Regular (400) | 20px | Table cell contents, button labels, slider descriptions |
| **Label** | 12px | Medium (500) | 16px | Overline labels, KPI categories, badge text |

---

## 5. Shape & Elevation System

Consistent corner rounding creates the recognizable Material Design 3 tactile softness:

- **Extra Large (28px):** Modal dialogs, floating pairing confirmation sheets.
- **Large (24px):** Primary content cards (`BeginM3CardHelper`), main navigation rail, header bar.
- **Medium (16px):** Action buttons, input fields, dropdown combos, metric summary boxes.
- **Small (12px):** Scrollbar thumbs, tabs, chips, tooltips.
- **Pill (Fully Rounded / 9999px):** Status badges ("ONLINE", "DIRECT LAN"), taglines, theme switcher pill.

---

## 6. Spacing Grid (4px Base)

Layouts follow strict multiples of 4px:
- `S4` (4px): Inner item spacing, badge text vertical padding.
- `S8` (8px): Icon-to-text spacing, small gap between related buttons.
- `S12` (12px): Navigation button spacing, metric card interior padding.
- `S16` (16px): Card internal padding, window border margins.
- `S20` (20px): Inter-column gutters.
- `S24` (24px): Section separators, hero card margins.
- `S32` (32px): Major layout block division.
- `S48` (48px): Top-level viewport margins.
