#pragma once

// ============================================================================
//  core/ui_theme.hpp — ImGui visual theme & drawing utilities
//
//  Usage:
//    #include "core/ui_theme.hpp"
//    // Once at startup (after ImGui::CreateContext):
//    ui_theme::ApplyTheme();
//    // Anywhere you need a drawing helper:
//    ui_theme::DrawGlowRect(dl, tl, br, ...);
//    // Switch accent at runtime:
//    ui_theme::SetAccent(2);  // red preset
// ============================================================================

#include "../dependencies/imgui/imgui.h"

namespace ui_theme {

// ============================================================================
//  1. Color Palette
// ============================================================================
namespace colors {

    // ── Background layers (darkest → lightest) ──────────────────────────────
    // Matches the existing d3d9_hook.cpp palette exactly; WindowBg = bg_darkest.
    inline ImVec4 bg_darkest    = ImVec4(0.04f,  0.04f,  0.06f,  0.97f); // #0A0A0F almost-black
    inline ImVec4 bg_dark       = ImVec4(0.051f, 0.051f, 0.059f, 1.00f); // #0D0D0F  window bg
    inline ImVec4 bg_mid        = ImVec4(0.078f, 0.078f, 0.094f, 1.00f); // #141418  child bg
    inline ImVec4 bg_light      = ImVec4(0.102f, 0.102f, 0.133f, 1.00f); // #1A1A22  hover bg
    inline ImVec4 bg_highlight  = ImVec4(0.133f, 0.133f, 0.188f, 1.00f); // #222230  active bg

    // ── Border / separator ───────────────────────────────────────────────────
    inline ImVec4 border        = ImVec4(0.118f, 0.118f, 0.141f, 1.00f); // #1E1E24
    inline ImVec4 border_light  = ImVec4(0.160f, 0.160f, 0.196f, 1.00f); // #292932

    // ── Accent — cyan (default, matches existing #00B4D8) ───────────────────
    inline ImVec4 accent        = ImVec4(0.000f, 0.706f, 0.847f, 1.00f); // #00B4D8
    inline ImVec4 accent_dim    = ImVec4(0.000f, 0.502f, 0.647f, 1.00f); // #0080A5
    inline ImVec4 accent_dark   = ImVec4(0.000f, 0.353f, 0.431f, 1.00f); // #005A6E
    inline ImVec4 accent_glow   = ImVec4(0.000f, 0.706f, 0.847f, 0.30f); // cyan 30% α

    // ── Semantic ─────────────────────────────────────────────────────────────
    inline ImVec4 danger        = ImVec4(0.90f,  0.20f,  0.20f,  1.00f); // #E63333 red
    inline ImVec4 danger_dim    = ImVec4(0.60f,  0.08f,  0.08f,  1.00f); // #991414
    inline ImVec4 danger_glow   = ImVec4(0.90f,  0.20f,  0.20f,  0.28f);
    inline ImVec4 success       = ImVec4(0.18f,  0.80f,  0.44f,  1.00f); // #2ECC70 green
    inline ImVec4 success_dim   = ImVec4(0.10f,  0.55f,  0.28f,  1.00f);
    inline ImVec4 success_glow  = ImVec4(0.18f,  0.80f,  0.44f,  0.28f);
    inline ImVec4 warning       = ImVec4(0.95f,  0.77f,  0.06f,  1.00f); // #F2C40F gold
    inline ImVec4 warning_dim   = ImVec4(0.65f,  0.50f,  0.02f,  1.00f);
    inline ImVec4 warning_glow  = ImVec4(0.95f,  0.77f,  0.06f,  0.28f);

    // ── Text ─────────────────────────────────────────────────────────────────
    inline ImVec4 text_primary   = ImVec4(0.92f, 0.93f, 0.95f, 1.00f); // #EBEDF2
    inline ImVec4 text_secondary = ImVec4(0.62f, 0.63f, 0.68f, 1.00f); // #9EA1AD
    inline ImVec4 text_dim       = ImVec4(0.38f, 0.38f, 0.42f, 1.00f); // #61616B disabled
    inline ImVec4 text_accent    = ImVec4(0.000f, 0.706f, 0.847f, 1.00f); // cyan text

} // namespace colors

// ============================================================================
//  2. Font Scale Constants
// ============================================================================

inline float titleSize  = 22.f;
inline float headerSize = 16.f;
inline float bodySize   = 14.f;
inline float smallSize  = 11.f;

// ============================================================================
//  3. Accent Color Presets
// ============================================================================

struct AccentPreset {
    ImVec4 accent;
    ImVec4 accent_dim;
    ImVec4 accent_dark;
    ImVec4 accent_glow;
    const char* name;
};

// Preset index:  0=cyan  1=purple  2=red  3=green  4=gold  5=pink
inline AccentPreset kAccentPresets[] = {
    // cyan  (#00B4D8)
    {
        ImVec4(0.000f, 0.706f, 0.847f, 1.00f),
        ImVec4(0.000f, 0.502f, 0.647f, 1.00f),
        ImVec4(0.000f, 0.353f, 0.431f, 1.00f),
        ImVec4(0.000f, 0.706f, 0.847f, 0.30f),
        "Cyan"
    },
    // purple (#9B59B6)
    {
        ImVec4(0.608f, 0.349f, 0.714f, 1.00f),
        ImVec4(0.420f, 0.220f, 0.510f, 1.00f),
        ImVec4(0.275f, 0.125f, 0.345f, 1.00f),
        ImVec4(0.608f, 0.349f, 0.714f, 0.30f),
        "Purple"
    },
    // red (#E63333)
    {
        ImVec4(0.902f, 0.200f, 0.200f, 1.00f),
        ImVec4(0.620f, 0.100f, 0.100f, 1.00f),
        ImVec4(0.380f, 0.055f, 0.055f, 1.00f),
        ImVec4(0.902f, 0.200f, 0.200f, 0.28f),
        "Red"
    },
    // green (#2ECC70)
    {
        ImVec4(0.180f, 0.800f, 0.439f, 1.00f),
        ImVec4(0.100f, 0.545f, 0.278f, 1.00f),
        ImVec4(0.051f, 0.310f, 0.161f, 1.00f),
        ImVec4(0.180f, 0.800f, 0.439f, 0.28f),
        "Green"
    },
    // gold (#F2C40F)
    {
        ImVec4(0.949f, 0.769f, 0.059f, 1.00f),
        ImVec4(0.655f, 0.506f, 0.020f, 1.00f),
        ImVec4(0.380f, 0.278f, 0.008f, 1.00f),
        ImVec4(0.949f, 0.769f, 0.059f, 0.28f),
        "Gold"
    },
    // pink (#E84393)
    {
        ImVec4(0.910f, 0.263f, 0.576f, 1.00f),
        ImVec4(0.620f, 0.145f, 0.373f, 1.00f),
        ImVec4(0.365f, 0.067f, 0.208f, 1.00f),
        ImVec4(0.910f, 0.263f, 0.576f, 0.28f),
        "Pink"
    },
};

inline int kAccentPresetCount = 6;

// Switch accent colors to a preset and propagate to the live ImGui palette.
// Safe to call at any time (will take effect from the next frame).
inline void SetAccent(int presetIdx) {
    if (presetIdx < 0 || presetIdx >= kAccentPresetCount)
        presetIdx = 0;

    const AccentPreset& p = kAccentPresets[presetIdx];
    colors::accent      = p.accent;
    colors::accent_dim  = p.accent_dim;
    colors::accent_dark = p.accent_dark;
    colors::accent_glow = p.accent_glow;
    // Also update text_accent to match.
    colors::text_accent = p.accent;

    // Propagate immediately to live ImGui style so everything updates atomically.
    ImGuiStyle& s = ImGui::GetStyle();
    s.Colors[ImGuiCol_CheckMark]            = p.accent;
    s.Colors[ImGuiCol_SliderGrab]           = p.accent;
    s.Colors[ImGuiCol_SliderGrabActive]     = p.accent_dim;
    s.Colors[ImGuiCol_ButtonActive]         = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.80f);
    s.Colors[ImGuiCol_HeaderActive]         = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.40f);
    s.Colors[ImGuiCol_ScrollbarGrabActive]  = p.accent;
    s.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.60f);
    s.Colors[ImGuiCol_ResizeGripActive]     = p.accent;
    s.Colors[ImGuiCol_TabActive]            = p.accent;
    s.Colors[ImGuiCol_SeparatorActive]      = p.accent;
    s.Colors[ImGuiCol_NavHighlight]         = p.accent;
    s.Colors[ImGuiCol_PlotLines]            = p.accent;
    s.Colors[ImGuiCol_PlotLinesHovered]     = p.accent_dim;
    s.Colors[ImGuiCol_PlotHistogram]        = p.accent;
    s.Colors[ImGuiCol_PlotHistogramHovered] = p.accent_dim;
    s.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.35f);
    s.Colors[ImGuiCol_DragDropTarget]       = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.90f);
}

// ============================================================================
//  4. ApplyTheme()  —  Call once after ImGui::CreateContext()
// ============================================================================

inline void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ── Geometry & spacing ───────────────────────────────────────────────────
    s.Alpha                       = 1.00f;
    s.DisabledAlpha               = 0.55f;

    // Window
    s.WindowPadding               = ImVec2(16.0f, 16.0f);  // generous breathing room
    s.WindowRounding              = 8.0f;
    s.WindowBorderSize            = 1.0f;
    s.WindowMinSize               = ImVec2(32.0f, 32.0f);
    s.WindowTitleAlign            = ImVec2(0.5f, 0.5f);
    s.WindowMenuButtonPosition    = ImGuiDir_None;

    // Child windows
    s.ChildRounding               = 6.0f;
    s.ChildBorderSize             = 1.0f;

    // Popups
    s.PopupRounding               = 6.0f;
    s.PopupBorderSize             = 1.0f;

    // Frames (input fields, checkboxes, sliders, etc.)
    s.FramePadding                = ImVec2(8.0f, 6.0f);    // comfortable click targets
    s.FrameRounding               = 4.0f;
    s.FrameBorderSize             = 1.0f;

    // Items
    s.ItemSpacing                 = ImVec2(10.0f, 8.0f);
    s.ItemInnerSpacing            = ImVec2(6.0f, 4.0f);
    s.CellPadding                 = ImVec2(4.0f, 3.0f);
    s.IndentSpacing               = 20.0f;
    s.ColumnsMinSpacing           = 6.0f;

    // Scrollbar — thin & rounded (pill style)
    s.ScrollbarSize               = 8.0f;
    s.ScrollbarRounding           = 18.0f;

    // Grab (slider/scrollbar grab)
    s.GrabMinSize                 = 10.0f;
    s.GrabRounding                = 4.0f;

    // Tabs — pill-shaped active tab
    s.TabRounding                 = 6.0f;
    s.TabBorderSize               = 0.0f;

    // Misc
    s.ColorButtonPosition         = ImGuiDir_Right;
    s.ButtonTextAlign             = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign         = ImVec2(0.0f, 0.5f);

    // ── Color table ──────────────────────────────────────────────────────────
    // Abbreviation: A = accent (#00B4D8), D = accent_dim, BG = bg layers.

    // Text
    s.Colors[ImGuiCol_Text]                  = colors::text_primary;
    s.Colors[ImGuiCol_TextDisabled]          = colors::text_dim;

    // Windows
    s.Colors[ImGuiCol_WindowBg]              = colors::bg_dark;
    s.Colors[ImGuiCol_ChildBg]               = colors::bg_mid;
    s.Colors[ImGuiCol_PopupBg]               = colors::bg_mid;

    // Borders
    s.Colors[ImGuiCol_Border]                = colors::border;
    s.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (input boxes, tree nodes, etc.)
    s.Colors[ImGuiCol_FrameBg]               = colors::bg_mid;
    s.Colors[ImGuiCol_FrameBgHovered]        = colors::bg_light;
    s.Colors[ImGuiCol_FrameBgActive]         = colors::bg_highlight;

    // Title bars (hidden in main menu but kept sensible for popups)
    s.Colors[ImGuiCol_TitleBg]               = colors::bg_dark;
    s.Colors[ImGuiCol_TitleBgActive]         = colors::bg_dark;
    s.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(colors::bg_dark.x, colors::bg_dark.y, colors::bg_dark.z, 0.75f);
    s.Colors[ImGuiCol_MenuBarBg]             = colors::bg_mid;

    // Scrollbar — thin, semi-transparent pill
    s.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // invisible track
    s.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(colors::border.x, colors::border.y, colors::border.z, 0.80f);
    s.Colors[ImGuiCol_ScrollbarGrabHovered]  = colors::border_light;
    s.Colors[ImGuiCol_ScrollbarGrabActive]   = colors::accent;

    // Checkboxes — accent tick
    s.Colors[ImGuiCol_CheckMark]             = colors::accent;

    // Sliders — accent grab, dark track (track = FrameBg, so inherited)
    s.Colors[ImGuiCol_SliderGrab]            = colors::accent;
    s.Colors[ImGuiCol_SliderGrabActive]      = colors::accent_dim;

    // Buttons — subtle, lights up on hover, accent flash on click
    s.Colors[ImGuiCol_Button]                = colors::bg_mid;
    s.Colors[ImGuiCol_ButtonHovered]         = colors::bg_light;
    s.Colors[ImGuiCol_ButtonActive]          = ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.80f);

    // Headers (CollapsingHeader, TreeNode, Selectable)
    s.Colors[ImGuiCol_Header]                = colors::bg_light;
    s.Colors[ImGuiCol_HeaderHovered]         = colors::bg_highlight;
    s.Colors[ImGuiCol_HeaderActive]          = ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.40f);

    // Separators
    s.Colors[ImGuiCol_Separator]             = colors::border;
    s.Colors[ImGuiCol_SeparatorHovered]      = colors::border_light;
    s.Colors[ImGuiCol_SeparatorActive]       = colors::accent;

    // Resize grip
    s.Colors[ImGuiCol_ResizeGrip]            = ImVec4(colors::border.x, colors::border.y, colors::border.z, 0.50f);
    s.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.60f);
    s.Colors[ImGuiCol_ResizeGripActive]      = colors::accent;

    // Tabs — pill-style: dark inactive, accent active
    s.Colors[ImGuiCol_Tab]                   = colors::bg_mid;
    s.Colors[ImGuiCol_TabHovered]            = colors::bg_light;
    s.Colors[ImGuiCol_TabActive]             = colors::accent;
    s.Colors[ImGuiCol_TabUnfocused]          = colors::bg_mid;
    s.Colors[ImGuiCol_TabUnfocusedActive]    = colors::bg_light;

    // Plots
    s.Colors[ImGuiCol_PlotLines]             = colors::accent;
    s.Colors[ImGuiCol_PlotLinesHovered]      = colors::accent_dim;
    s.Colors[ImGuiCol_PlotHistogram]         = colors::accent;
    s.Colors[ImGuiCol_PlotHistogramHovered]  = colors::accent_dim;

    // Tables
    s.Colors[ImGuiCol_TableHeaderBg]         = colors::bg_mid;
    s.Colors[ImGuiCol_TableBorderStrong]     = colors::border;
    s.Colors[ImGuiCol_TableBorderLight]      = ImVec4(colors::border.x, colors::border.y, colors::border.z, 0.60f);
    s.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    s.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);

    // Selection / drag / nav
    s.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.35f);
    s.Colors[ImGuiCol_DragDropTarget]        = ImVec4(colors::accent.x, colors::accent.y, colors::accent.z, 0.90f);
    s.Colors[ImGuiCol_NavHighlight]          = colors::accent;
    s.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    s.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    s.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
}

// ============================================================================
//  5. Drawing Helpers
//  All helpers are inline and operate on a caller-supplied ImDrawList*.
//  They do NOT push/pop any ImGui state — purely imperative draw calls.
// ============================================================================

// Helper: convert ImVec4 (0-1 RGBA) to packed ImU32 (ABGR).
// Prefer IM_COL32 for compile-time constants; this is for runtime ImVec4.
inline ImU32 Vec4ToU32(const ImVec4& c) {
    return IM_COL32(
        static_cast<int>(c.x * 255.f),
        static_cast<int>(c.y * 255.f),
        static_cast<int>(c.z * 255.f),
        static_cast<int>(c.w * 255.f)
    );
}

// Helper: scale alpha channel of an ImU32 color by [0..1].
inline ImU32 ScaleAlpha(ImU32 col, float alpha) {
    const ImU32 a = static_cast<ImU32>((col >> IM_COL32_A_SHIFT & 0xFF) * alpha);
    return (col & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
}

// Helper: linearly interpolate between two ImU32 colors by t=[0..1].
inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    const float r = (float)((a >> 0)  & 0xFF) + ((float)((b >> 0)  & 0xFF) - (float)((a >> 0)  & 0xFF)) * t;
    const float g = (float)((a >> 8)  & 0xFF) + ((float)((b >> 8)  & 0xFF) - (float)((a >> 8)  & 0xFF)) * t;
    const float bv= (float)((a >> 16) & 0xFF) + ((float)((b >> 16) & 0xFF) - (float)((a >> 16) & 0xFF)) * t;
    const float av= (float)((a >> 24) & 0xFF) + ((float)((b >> 24) & 0xFF) - (float)((a >> 24) & 0xFF)) * t;
    return IM_COL32((int)r, (int)g, (int)bv, (int)av);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawGradientRect
//   Horizontal gradient filled rectangle (left colour → right colour).
//   Accomplished via two AddRectFilledMultiColor corners.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawGradientRect(
    ImDrawList* dl,
    ImVec2      tl,       // top-left
    ImVec2      br,       // bottom-right
    ImU32       colLeft,
    ImU32       colRight
) {
    dl->AddRectFilledMultiColor(tl, br, colLeft, colRight, colRight, colLeft);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawGradientRectV
//   Vertical gradient filled rectangle (top colour → bottom colour).
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawGradientRectV(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    ImU32       colTop,
    ImU32       colBottom
) {
    dl->AddRectFilledMultiColor(tl, br, colTop, colTop, colBottom, colBottom);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawGlowRect
//   Draws a rectangle with an outer glow by layering multiple expanding,
//   semi-transparent filled rects behind the base rect.
//   `glowSize` controls the maximum expansion distance in pixels.
//   `rounding` is applied to all rects for consistent shape.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawGlowRect(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    ImU32       col,
    float       glowSize  = 12.0f,
    float       rounding  = 4.0f
) {
    // Layer count: more layers = smoother glow (at slight CPU cost).
    constexpr int kLayers = 6;
    for (int i = kLayers; i >= 1; --i) {
        const float t       = static_cast<float>(i) / static_cast<float>(kLayers);
        const float expand  = glowSize * t;
        // Alpha falls off quadratically so the outermost layer is barely visible.
        const float alpha   = 0.045f * (1.0f - t * t);
        const ImU32 layerCol = ScaleAlpha(col, alpha);
        dl->AddRectFilled(
            ImVec2(tl.x - expand, tl.y - expand),
            ImVec2(br.x + expand, br.y + expand),
            layerCol,
            rounding + expand
        );
    }
    // Core rect — drawn last so it sits on top.
    dl->AddRectFilled(tl, br, col, rounding);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawShadowRect
//   Drop shadow under a rectangle.  The shadow expands outward from the rect
//   and fades to transparent.  Draws BEHIND the caller's rect; call this
//   before drawing the rect itself.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawShadowRect(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    float       shadowSize = 10.0f,
    float       rounding   = 4.0f
) {
    constexpr int kLayers = 5;
    for (int i = 1; i <= kLayers; ++i) {
        const float t      = static_cast<float>(i) / static_cast<float>(kLayers);
        const float expand = shadowSize * t;
        // Classic drop shadow: shift down-right, alpha fades out.
        const float alpha  = 0.35f * (1.0f - t);
        const ImU32 shadowCol = IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.f));
        const float offsetX = expand * 0.3f;
        const float offsetY = expand * 0.6f;
        dl->AddRectFilled(
            ImVec2(tl.x - expand * 0.5f + offsetX, tl.y - expand * 0.2f + offsetY),
            ImVec2(br.x + expand * 0.5f + offsetX, br.y + expand * 0.8f + offsetY),
            shadowCol,
            rounding + expand * 0.5f
        );
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawGradientBorder
//   Draws a border around a rectangle whose colour transitions vertically
//   from colA (top) to colB (bottom).  Implemented by drawing four thin
//   gradient-filled quads along each edge, so the corners blend naturally.
//   `thickness` is the border line width in pixels.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawGradientBorder(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    ImU32       colA,       // top colour
    ImU32       colB,       // bottom colour
    float       rounding  = 4.0f,
    float       thickness = 1.0f
) {
    // We interpolate colA→colB at each horizontal edge's y-position.
    const float totalH  = br.y - tl.y;
    const float epsilon = 0.001f;
    if (totalH < epsilon) return;

    // Top edge — pure colA
    // Bottom edge — pure colB
    // Left / right edges — gradient from top to bottom.

    // We render as a sequence of thin gradient rects for each edge.
    const float t = thickness;

    // Top edge (horizontal, colA → colA)
    dl->AddRectFilledMultiColor(
        ImVec2(tl.x,        tl.y),
        ImVec2(br.x,        tl.y + t),
        colA, colA, colA, colA
    );
    // Bottom edge (horizontal, colB → colB)
    dl->AddRectFilledMultiColor(
        ImVec2(tl.x,        br.y - t),
        ImVec2(br.x,        br.y),
        colB, colB, colB, colB
    );
    // Left edge (vertical gradient colA → colB)
    dl->AddRectFilledMultiColor(
        ImVec2(tl.x,        tl.y + t),
        ImVec2(tl.x + t,    br.y - t),
        colA, colA, colB, colB
    );
    // Right edge (vertical gradient colA → colB)
    dl->AddRectFilledMultiColor(
        ImVec2(br.x - t,    tl.y + t),
        ImVec2(br.x,        br.y - t),
        colA, colA, colB, colB
    );
    (void)rounding; // rounding on a raw border quad is non-trivial; reserved.
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawSeparatorGradient
//   Horizontal separator line that fades from full opacity at the centre
//   outward to transparent at both ends.  Useful for section dividers that
//   feel less harsh than a solid line.
//
//   `col`    — base colour (typically border or accent_glow).
//   `y`      — screen-space Y coordinate of the separator.
//   `xStart` / `xEnd` — horizontal extent.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawSeparatorGradient(
    ImDrawList* dl,
    float       y,
    float       xStart,
    float       xEnd,
    ImU32       col
) {
    const float mid   = xStart + (xEnd - xStart) * 0.5f;
    const ImU32 clear = col & ~(0xFFu << IM_COL32_A_SHIFT); // col with alpha=0

    // Left half: transparent → col
    dl->AddRectFilledMultiColor(
        ImVec2(xStart, y),
        ImVec2(mid,    y + 1.0f),
        clear, col, col, clear
    );
    // Right half: col → transparent
    dl->AddRectFilledMultiColor(
        ImVec2(mid,  y),
        ImVec2(xEnd, y + 1.0f),
        col, clear, clear, col
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawPanelBackground
//   Convenience wrapper: dark filled rect + 1-px gradient border.
//   Used for sidebar panels, section boxes, sub-panels, etc.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawPanelBackground(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    float       rounding = 6.0f
) {
    // Fill
    dl->AddRectFilled(tl, br, Vec4ToU32(colors::bg_mid), rounding);
    // Gradient border: subtle top-accent → border bottom
    const ImU32 borderTop = IM_COL32(
        static_cast<int>(colors::border.x * 255),
        static_cast<int>(colors::border.y * 255),
        static_cast<int>(colors::border.z * 255),
        180
    );
    const ImU32 borderBot = IM_COL32(
        static_cast<int>(colors::border.x * 255),
        static_cast<int>(colors::border.y * 255),
        static_cast<int>(colors::border.z * 255),
        80
    );
    DrawGradientBorder(dl, tl, br, borderTop, borderBot, rounding, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawAccentBar
//   Thin horizontal accent stripe — useful as a header underline or active
//   indicator beneath a selected tab/section.
//   `width` controls how wide the bar is (0 = full extent from tl to br.x).
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawAccentBar(
    ImDrawList* dl,
    ImVec2      pos,         // left-centre point of the bar
    float       width,
    float       height   = 2.0f,
    float       rounding = 1.0f
) {
    const ImVec2 tl = ImVec2(pos.x, pos.y - height * 0.5f);
    const ImVec2 br = ImVec2(pos.x + width, pos.y + height * 0.5f);
    const ImU32  colLeft  = Vec4ToU32(colors::accent_dim);
    const ImU32  colRight = Vec4ToU32(colors::accent);
    DrawGradientRect(dl, tl, br, colLeft, colRight);
    // Tiny glow beneath
    DrawGlowRect(dl, tl, br, Vec4ToU32(colors::accent), height * 3.0f, rounding);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawTooltipBg
//   Styled background for custom tooltip windows.
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawTooltipBg(
    ImDrawList* dl,
    ImVec2      tl,
    ImVec2      br,
    float       rounding = 4.0f
) {
    DrawShadowRect(dl, tl, br, 8.0f, rounding);
    dl->AddRectFilled(tl, br, Vec4ToU32(colors::bg_mid), rounding);
    dl->AddRect(tl, br, Vec4ToU32(colors::border), rounding, 0, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawStatusDot
//   Small circle indicator (e.g. enabled = accent, disabled = dim).
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawStatusDot(
    ImDrawList* dl,
    ImVec2      centre,
    float       radius,
    bool        active
) {
    const ImU32 fillCol  = active ? Vec4ToU32(colors::accent) : Vec4ToU32(colors::border_light);
    const ImU32 glowCol  = active ? Vec4ToU32(colors::accent_glow) : IM_COL32(0, 0, 0, 0);

    if (active) {
        // Soft glow ring
        dl->AddCircleFilled(centre, radius + 4.0f, glowCol, 24);
        dl->AddCircleFilled(centre, radius + 2.0f, ScaleAlpha(glowCol, 0.5f), 24);
    }
    dl->AddCircleFilled(centre, radius, fillCol, 24);
}

} // namespace ui_theme
