#pragma once
// ──────────────────────────────────────────────────────────────────────────────
// core/ui_window.hpp  —  Custom resizable / draggable / persistent window
//                         for FRIENDLYDLL's ImGui menu
//
// Drop-in replacement for ImGui::Begin / ImGui::End.
//
// Usage (per-frame inside Present hook):
//
//     if (!ui_window::BeginWindow()) return;   // window fully faded out
//     if (!ui_window::g_state.minimized) {
//         /* tab content */
//     }
//     ui_window::EndWindow();
//
// Toggle visibility (bind to INSERT / menu key):
//     ui_window::Toggle();
//
// ──────────────────────────────────────────────────────────────────────────────

#include "../dependencies/imgui/imgui.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

// Pull in menu_anim helpers when available (glow, colour waves, etc.)
#if __has_include("menu_anim.hpp")
#   include "menu_anim.hpp"
#   define UIW_HAS_MENU_ANIM 1
#else
#   define UIW_HAS_MENU_ANIM 0
#endif

namespace ui_window {

// ── Tunables ──────────────────────────────────────────────────────────────────
constexpr float kTitleBarH      = 36.f;   // custom title bar height (px)
constexpr float kResizeEdgePx   = 6.f;    // grab-zone thickness for all edges
constexpr float kCornerSz       = 14.f;   // corner-indicator L-arm length
constexpr float kAlphaSpeed     = 9.f;    // open/close fade speed (1/s)
constexpr float kMinimizeSpeed  = 14.f;   // height-collapse speed multiplier
constexpr float kAnimScaleLo    = 0.95f;  // scale when window is fully closed
constexpr float kBtnW           = 22.f;   // title-bar button width
constexpr float kBtnH           = 20.f;   // title-bar button height

// Colours
constexpr ImU32 kClrAccent      = IM_COL32(  0, 180, 216, 255);
constexpr ImU32 kClrAccentGlow  = IM_COL32(  0, 180, 216,  40);
constexpr ImU32 kClrTitleBg     = IM_COL32( 10,  10,  15, 255);
constexpr ImU32 kClrBtnNormal   = IM_COL32( 28,  28,  40, 180);
constexpr ImU32 kClrBtnHover    = IM_COL32( 50,  50,  68, 210);
constexpr ImU32 kClrBtnClose    = IM_COL32(160,  28,  28, 190);
constexpr ImU32 kClrBtnCloseHov = IM_COL32(210,  48,  48, 230);
constexpr ImU32 kClrEdgeNormal  = IM_COL32(  0, 180, 216,  65);
constexpr ImU32 kClrEdgeHover   = IM_COL32(  0, 210, 240, 150);

// ── Resize edge bitmask ───────────────────────────────────────────────────────
enum ResizeEdge : int {
    Edge_None   = 0,
    Edge_Left   = 1 << 0,
    Edge_Right  = 1 << 1,
    Edge_Top    = 1 << 2,
    Edge_Bottom = 1 << 3,
};

// ── WindowState ───────────────────────────────────────────────────────────────
struct WindowState {
    // Geometry (saved to disk)
    ImVec2 pos     = { 100.f, 100.f };
    ImVec2 size    = { 820.f, 680.f };    // default — bigger than old 900×540
    ImVec2 minSize = { 600.f, 450.f };
    ImVec2 maxSize = { 1600.f, 1000.f };

    // Visibility / state
    bool   minimized  = false;
    bool   maximized  = false;
    float  openAlpha  = 0.f;             // currently rendered alpha (animated)
    float  targetAlpha= 0.f;             // desired alpha (1=open, 0=closed)
    bool   isOpen     = false;

    // Drag
    bool   isDragging = false;
    ImVec2 dragOffset = { 0.f, 0.f };

    // Resize
    bool   isResizing       = false;
    int    resizeEdge        = Edge_None;
    ImVec2 resizeStartMouse = { 0.f, 0.f };
    ImVec2 resizeStartPos   = { 0.f, 0.f };
    ImVec2 resizeStartSize  = { 0.f, 0.f };

    // Maximize save-slot
    ImVec2 preMaxPos  = { 0.f, 0.f };
    ImVec2 preMaxSize = { 0.f, 0.f };

    // Animated display height (for minimize collapse)
    float  displayH   = 0.f;

    // First-frame flag (triggers auto-load + snap)
    bool   initialized = false;
};

inline WindowState g_state;

// ── Internal helpers (not part of public API) ─────────────────────────────────
namespace _impl {

inline float Lerp(float a, float b, float t)           { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi)        { return v < lo ? lo : (v > hi ? hi : v); }
inline ImVec2 ClampVec(ImVec2 v, ImVec2 lo, ImVec2 hi) {
    return { Clamp(v.x, lo.x, hi.x), Clamp(v.y, lo.y, hi.y) };
}

// Apply an alpha multiplier to an IM_COL32 colour's alpha channel.
inline ImU32 WithAlpha(ImU32 col, float a) {
    float baseA = float((col >> 24) & 0xFF);
    ImU32 newA  = ImU32(Clamp(baseA * a, 0.f, 255.f));
    return (col & 0x00FFFFFFu) | (newA << 24);
}

// Returns true if (mx,my) falls inside the AABB [x1,x2] x [y1,y2].
inline bool InRect(float mx, float my,
                   float x1, float y1, float x2, float y2) {
    return mx >= x1 && mx <= x2 && my >= y1 && my <= y2;
}

// Draw a labelled button using the provided drawlist.
// Returns true if clicked this frame.
inline bool TitleBarButton(ImDrawList* dl,
                            const char* label,
                            float x, float y, float w, float h,
                            ImU32 normalCol, ImU32 hoverCol,
                            float alpha) {
    ImVec2 mp  = ImGui::GetMousePos();
    bool   hov = InRect(mp.x, mp.y, x, y, x + w, y + h);

    dl->AddRectFilled({ x, y }, { x + w, y + h },
                      WithAlpha(hov ? hoverCol : normalCol, alpha), 4.f);

    // Thin border on hover
    if (hov)
        dl->AddRect({ x, y }, { x + w, y + h },
                    WithAlpha(kClrAccent, alpha * 0.5f), 4.f, 0, 1.f);

    // Centre the label glyph
    ImVec2 tsz = ImGui::CalcTextSize(label);
    dl->AddText({ x + (w - tsz.x) * 0.5f, y + (h - tsz.y) * 0.5f },
                WithAlpha(IM_COL32(220, 220, 230, 255), alpha),
                label);

    return hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

} // namespace _impl

// ── Persistence ───────────────────────────────────────────────────────────────

inline void SaveState(const char* filepath = "friendlydll_window.cfg") {
    std::ofstream f(filepath);
    if (!f) return;
    f << "pos_x="     << g_state.pos.x        << "\n"
      << "pos_y="     << g_state.pos.y        << "\n"
      << "size_w="    << g_state.size.x       << "\n"
      << "size_h="    << g_state.size.y       << "\n"
      << "minimized=" << int(g_state.minimized) << "\n"
      << "maximized=" << int(g_state.maximized) << "\n"
      << "premax_px=" << g_state.preMaxPos.x  << "\n"
      << "premax_py=" << g_state.preMaxPos.y  << "\n"
      << "premax_sw=" << g_state.preMaxSize.x << "\n"
      << "premax_sh=" << g_state.preMaxSize.y << "\n";
}

inline void LoadState(const char* filepath = "friendlydll_window.cfg") {
    std::ifstream f(filepath);
    if (!f) return;

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        float fv = 0.f;
        try { fv = std::stof(val); } catch (...) { continue; }

        if      (key == "pos_x")     g_state.pos.x        = fv;
        else if (key == "pos_y")     g_state.pos.y        = fv;
        else if (key == "size_w")    g_state.size.x       = fv;
        else if (key == "size_h")    g_state.size.y       = fv;
        else if (key == "minimized") g_state.minimized    = (fv != 0.f);
        else if (key == "maximized") g_state.maximized    = (fv != 0.f);
        else if (key == "premax_px") g_state.preMaxPos.x  = fv;
        else if (key == "premax_py") g_state.preMaxPos.y  = fv;
        else if (key == "premax_sw") g_state.preMaxSize.x = fv;
        else if (key == "premax_sh") g_state.preMaxSize.y = fv;
    }

    // Clamp loaded geometry to valid ranges
    g_state.size = _impl::ClampVec(g_state.size, g_state.minSize, g_state.maxSize);

    // Snap display height so there is no animation glitch on first frame
    g_state.displayH = g_state.minimized ? kTitleBarH : g_state.size.y;
}

// ── Open / Close / Toggle ─────────────────────────────────────────────────────

inline void Open() {
    g_state.targetAlpha = 1.f;
    g_state.isOpen      = true;
}

inline void Close() {
    g_state.targetAlpha = 0.f;
    SaveState();
}

inline void Toggle() {
    if (g_state.targetAlpha > 0.5f) Close();
    else                             Open();
}

inline bool IsVisible() { return g_state.openAlpha > 0.001f; }

// ── Maximize / Restore ────────────────────────────────────────────────────────

inline void Maximize() {
    if (g_state.maximized) return;
    g_state.preMaxPos  = g_state.pos;
    g_state.preMaxSize = g_state.size;
    g_state.maximized  = true;
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    g_state.pos  = { 0.f, 0.f };
    g_state.size = { disp.x, disp.y };
    g_state.displayH = disp.y;
}

inline void RestoreFromMaximize() {
    if (!g_state.maximized) return;
    g_state.maximized = false;
    g_state.pos   = g_state.preMaxPos;
    g_state.size  = g_state.preMaxSize;
    // Clamp in case resolution changed between maximize and restore
    ImVec2 disp   = ImGui::GetIO().DisplaySize;
    g_state.pos.x = _impl::Clamp(g_state.pos.x, 0.f, disp.x - g_state.minSize.x);
    g_state.pos.y = _impl::Clamp(g_state.pos.y, 0.f, disp.y - kTitleBarH);
    g_state.displayH = g_state.minimized ? kTitleBarH : g_state.size.y;
}

inline void ToggleMaximize() {
    if (g_state.maximized) RestoreFromMaximize();
    else                    Maximize();
}

// ── Minimize ──────────────────────────────────────────────────────────────────

inline void ToggleMinimize() {
    g_state.minimized = !g_state.minimized;
}

// ── BeginWindow ───────────────────────────────────────────────────────────────
// Call every frame.  Returns false ONLY when the window is fully faded out and
// invisible — caller should skip all content AND EndWindow in that case.

inline bool BeginWindow() {
    const float dt = ImGui::GetIO().DeltaTime;

    // ─── Auto-load + snap on first frame ────────────────────────────────
    if (!g_state.initialized) {
        LoadState();
        g_state.initialized = true;
        // If already marked open, snap alpha to 1 so there is no initial fade
        if (g_state.targetAlpha > 0.5f) g_state.openAlpha = 1.f;
        g_state.displayH = g_state.minimized ? kTitleBarH : g_state.size.y;
    }

    // ─── Animate openAlpha → targetAlpha ────────────────────────────────
    {
        float diff = g_state.targetAlpha - g_state.openAlpha;
        float step = kAlphaSpeed * dt;
        if (std::fabs(diff) <= step) g_state.openAlpha = g_state.targetAlpha;
        else                          g_state.openAlpha += (diff > 0.f ? step : -step);
        g_state.openAlpha = _impl::Clamp(g_state.openAlpha, 0.f, 1.f);
    }

    // Fully invisible — nothing to render
    if (g_state.openAlpha < 0.001f) {
        g_state.isOpen = false;
        return false;
    }

    const float alpha = g_state.openAlpha;

    // ─── Animate minimize height collapse ───────────────────────────────
    {
        float targetH = g_state.minimized ? kTitleBarH : g_state.size.y;
        float diff    = targetH - g_state.displayH;
        // Scale step so the animation takes roughly the same wall-clock time
        // regardless of window height, but enforce a minimum 1px/frame step.
        float step = (std::max)(kMinimizeSpeed * std::fabs(g_state.size.y - kTitleBarH) * dt, 1.f);
        if (std::fabs(diff) <= step) g_state.displayH = targetH;
        else                          g_state.displayH += (diff > 0.f ? step : -step);
    }

    // ─── Scale animation (0.95 → 1.0 while fading in) ───────────────────
    const float animScale = _impl::Lerp(kAnimScaleLo, 1.f, alpha);

    // ─── Compute draw position and size ─────────────────────────────────
    ImVec2 drawSize = { g_state.size.x, g_state.displayH };
    ImVec2 drawPos  = g_state.pos;

    if (!g_state.maximized) {
        // Apply scale around the window center
        if (animScale < 0.9999f) {
            float cx    = g_state.pos.x + g_state.size.x    * 0.5f;
            float cy    = g_state.pos.y + g_state.displayH  * 0.5f;
            drawSize.x *= animScale;
            drawSize.y *= animScale;
            drawPos.x   = cx - drawSize.x * 0.5f;
            drawPos.y   = cy - drawSize.y * 0.5f;
        }

        // Clamp window onto visible display (only when not actively dragging / resizing)
        if (!g_state.isDragging && !g_state.isResizing) {
            ImVec2 disp = ImGui::GetIO().DisplaySize;
            g_state.pos.x = _impl::Clamp(g_state.pos.x, 0.f, disp.x - g_state.minSize.x);
            g_state.pos.y = _impl::Clamp(g_state.pos.y, 0.f, disp.y - kTitleBarH);
            drawPos = g_state.pos;
        }
    }

    // ─── Tell ImGui where to place the window ───────────────────────────
    ImGui::SetNextWindowPos(drawPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(drawSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(alpha);

    // Push fade alpha so all child widgets inherit it automatically
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    ImGui::Begin("##ui_window_main", nullptr,
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoCollapse        |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 wPos     = ImGui::GetWindowPos();
    ImVec2 wSz      = ImGui::GetWindowSize();

    // ─── Title bar background ────────────────────────────────────────────
    dl->AddRectFilled(wPos,
                      { wPos.x + wSz.x, wPos.y + kTitleBarH },
                      _impl::WithAlpha(kClrTitleBg, alpha),
                      6.f, ImDrawFlags_RoundCornersTop);

    // Accent line at the very top of the title bar
    dl->AddLine(wPos,
                { wPos.x + wSz.x, wPos.y },
                _impl::WithAlpha(kClrAccent, alpha), 2.f);

    // ─── Title text ──────────────────────────────────────────────────────
    {
        const char* title = "FRIENDLYDLL";
        float       tx    = wPos.x + 12.f;
        ImVec2      tSz   = ImGui::CalcTextSize(title);
        float       ty    = wPos.y + (kTitleBarH - tSz.y) * 0.5f;

        // Breathing glow (re-use menu_anim if available, else fallback)
        float pulse   = (std::sinf((float)ImGui::GetTime() * 2.f) + 1.f) * 0.5f;
        int   glowA   = int(_impl::Clamp((16.f + pulse * 26.f) * alpha, 0.f, 255.f));
        ImU32 glowCol = IM_COL32(0, 180, 216, glowA);
        for (int gx = -2; gx <= 2; ++gx)
            for (int gy = -2; gy <= 2; ++gy)
                if (gx != 0 || gy != 0)
                    dl->AddText({ tx + gx, ty + gy }, glowCol, title);

        // Main title glyph — animated hue if menu_anim present, plain cyan otherwise
#if UIW_HAS_MENU_ANIM
        float hue    = 190.f + std::sinf((float)ImGui::GetTime() * 1.2f) * 12.f;
        ImU32 txtCol = menu_anim::HsvToRgb(hue, 0.55f, 1.f, alpha);
#else
        ImU32 txtCol = _impl::WithAlpha(kClrAccent, alpha);
#endif
        dl->AddText({ tx, ty }, txtCol, title);

        // Small version tag beside the title
        const char* tag   = "v3.0";
        ImVec2      tagSz = ImGui::CalcTextSize(tag);
        dl->AddText({ tx + tSz.x + 8.f, ty + tSz.y - tagSz.y },
                    _impl::WithAlpha(IM_COL32(80, 150, 175, 185), alpha),
                    tag);
    }

    // ─── Title bar buttons (right side, right→left) ──────────────────────
    //   [ _ ]  [ o/# ]  [ x ]
    float btnY   = wPos.y + (kTitleBarH - kBtnH) * 0.5f;
    float rightX = wPos.x + wSz.x - 6.f;   // tracks right edge

    // Close [x]
    rightX -= kBtnW;
    if (_impl::TitleBarButton(dl, "x",
                              rightX, btnY, kBtnW, kBtnH,
                              kClrBtnClose, kClrBtnCloseHov, alpha))
        Close();

    // Maximize / Restore [o / #]
    rightX -= kBtnW + 3.f;
    if (_impl::TitleBarButton(dl,
                              g_state.maximized ? "#" : "o",
                              rightX, btnY, kBtnW, kBtnH,
                              kClrBtnNormal, kClrBtnHover, alpha))
        ToggleMaximize();

    // Minimize [_]
    rightX -= kBtnW + 3.f;
    const float btnAreaRight = rightX - 3.f;    // everything to the left of buttons
    if (_impl::TitleBarButton(dl, "_",
                              rightX, btnY, kBtnW, kBtnH,
                              kClrBtnNormal, kClrBtnHover, alpha))
        ToggleMinimize();

    // ─── Drag logic ──────────────────────────────────────────────────────
    ImVec2 mp = ImGui::GetMousePos();

    // Title bar area (exclude buttons)
    bool inTitleBar = _impl::InRect(mp.x, mp.y,
                                    wPos.x, wPos.y,
                                    btnAreaRight, wPos.y + kTitleBarH);

    if (inTitleBar && !g_state.isResizing &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_state.isDragging = true;
        g_state.dragOffset = { mp.x - g_state.pos.x, mp.y - g_state.pos.y };
    }

    if (g_state.isDragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Dragging a maximized window restores it first and re-anchors offset
            if (g_state.maximized) {
                float relX = _impl::Clamp((mp.x - g_state.pos.x) / g_state.size.x, 0.f, 1.f);
                RestoreFromMaximize();
                g_state.dragOffset.x = g_state.size.x * relX;
                g_state.dragOffset.y = kTitleBarH * 0.5f;
            }
            g_state.pos = { mp.x - g_state.dragOffset.x,
                            mp.y - g_state.dragOffset.y };
        } else {
            g_state.isDragging = false;
        }
    }

    // Double-click title bar → toggle maximize
    if (inTitleBar && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        ToggleMaximize();

    // ─── Resize logic (edges + corners) ─────────────────────────────────
    if (!g_state.maximized) {
        float l = wPos.x,       r = wPos.x + wSz.x;
        float t = wPos.y,       b = wPos.y + wSz.y;
        float e = kResizeEdgePx;

        // Compute hovered edge bitmask
        auto ComputeEdge = [&]() -> int {
            int edge = Edge_None;
            if (mp.x >= l - e && mp.x <= l + e && mp.y >= t && mp.y <= b) edge |= Edge_Left;
            if (mp.x >= r - e && mp.x <= r + e && mp.y >= t && mp.y <= b) edge |= Edge_Right;
            if (mp.y >= t - e && mp.y <= t + e && mp.x >= l && mp.x <= r) edge |= Edge_Top;
            if (mp.y >= b - e && mp.y <= b + e && mp.x >= l && mp.x <= r) edge |= Edge_Bottom;
            return edge;
        };

        int hoverEdge = g_state.isResizing ? g_state.resizeEdge : ComputeEdge();

        // Update mouse cursor for both hover preview and active resize
        if (!g_state.isDragging) {
            bool diagNWSE = ((hoverEdge & Edge_Left)  && (hoverEdge & Edge_Top))   ||
                            ((hoverEdge & Edge_Right) && (hoverEdge & Edge_Bottom));
            bool diagNESW = ((hoverEdge & Edge_Right) && (hoverEdge & Edge_Top))   ||
                            ((hoverEdge & Edge_Left)  && (hoverEdge & Edge_Bottom));
            if      (diagNWSE)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            else if (diagNESW)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
            else if (hoverEdge & (Edge_Left | Edge_Right))
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (hoverEdge & (Edge_Top | Edge_Bottom))
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        // Begin resize on click
        if (!g_state.isResizing && !g_state.isDragging &&
            hoverEdge != Edge_None &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            g_state.isResizing       = true;
            g_state.resizeEdge       = hoverEdge;
            g_state.resizeStartMouse = mp;
            g_state.resizeStartPos   = g_state.pos;
            g_state.resizeStartSize  = g_state.size;
        }

        // Perform resize while mouse button is held
        if (g_state.isResizing) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 d       = { mp.x - g_state.resizeStartMouse.x,
                                   mp.y - g_state.resizeStartMouse.y };
                ImVec2 newPos  = g_state.resizeStartPos;
                ImVec2 newSize = g_state.resizeStartSize;

                if (g_state.resizeEdge & Edge_Right)  { newSize.x += d.x; }
                if (g_state.resizeEdge & Edge_Bottom)  { newSize.y += d.y; }
                if (g_state.resizeEdge & Edge_Left)   { newSize.x -= d.x; newPos.x += d.x; }
                if (g_state.resizeEdge & Edge_Top)    { newSize.y -= d.y; newPos.y += d.y; }

                // Clamp width
                if (newSize.x < g_state.minSize.x) {
                    if (g_state.resizeEdge & Edge_Left)
                        newPos.x -= g_state.minSize.x - newSize.x;
                    newSize.x = g_state.minSize.x;
                }
                if (newSize.x > g_state.maxSize.x) newSize.x = g_state.maxSize.x;

                // Clamp height
                if (newSize.y < g_state.minSize.y) {
                    if (g_state.resizeEdge & Edge_Top)
                        newPos.y -= g_state.minSize.y - newSize.y;
                    newSize.y = g_state.minSize.y;
                }
                if (newSize.y > g_state.maxSize.y) newSize.y = g_state.maxSize.y;

                g_state.pos  = newPos;
                g_state.size = newSize;

                // Keep displayH in sync (not minimized)
                if (!g_state.minimized)
                    g_state.displayH = g_state.size.y;
            } else {
                g_state.isResizing = false;
                g_state.resizeEdge = Edge_None;
            }
        }
    }

    // ─── Push content cursor below title bar ─────────────────────────────
    ImGui::SetCursorPos({ 0.f, kTitleBarH });

    return true;
}

// ── EndWindow ─────────────────────────────────────────────────────────────────
// Always call this when BeginWindow() returned true.

inline void EndWindow() {
    // ─── Draw resize-edge visual feedback ───────────────────────────────
    if (!g_state.maximized && !g_state.minimized) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 wPos     = ImGui::GetWindowPos();
        ImVec2 wSz      = ImGui::GetWindowSize();
        ImVec2 mp       = ImGui::GetMousePos();
        float  alpha    = g_state.openAlpha;
        float  l = wPos.x, r = wPos.x + wSz.x;
        float  t = wPos.y, b = wPos.y + wSz.y;
        float  e = kResizeEdgePx;

        auto EdgeCol = [&](float x1, float y1, float x2, float y2) -> ImU32 {
            bool hov = _impl::InRect(mp.x, mp.y, x1, y1, x2, y2) || g_state.isResizing;
            return _impl::WithAlpha(hov ? kClrEdgeHover : kClrEdgeNormal, alpha);
        };

        // Right edge
        dl->AddLine({ r - 1.f, t + kTitleBarH + 4.f },
                    { r - 1.f, b - 4.f },
                    EdgeCol(r - e, t, r + e, b), 1.f);

        // Left edge
        dl->AddLine({ l + 1.f, t + kTitleBarH + 4.f },
                    { l + 1.f, b - 4.f },
                    EdgeCol(l - e, t, l + e, b), 1.f);

        // Bottom edge
        dl->AddLine({ l + 4.f, b - 1.f },
                    { r - 4.f, b - 1.f },
                    EdgeCol(l, b - e, r, b + e), 1.f);

        float cs = kCornerSz;

        // Corner L-shapes to give an obvious resize grab indicator
        // Bottom-right
        {
            ImU32 cc = EdgeCol(r - e - cs, b - e - cs, r + e, b + e);
            dl->AddLine({ r - cs, b - 1.f }, { r - 1.f, b - 1.f }, cc, 2.f);
            dl->AddLine({ r - 1.f, b - cs }, { r - 1.f, b - 1.f }, cc, 2.f);
        }
        // Bottom-left
        {
            ImU32 cc = EdgeCol(l - e, b - e - cs, l + e + cs, b + e);
            dl->AddLine({ l + 1.f, b - 1.f }, { l + cs,  b - 1.f }, cc, 2.f);
            dl->AddLine({ l + 1.f, b - cs  }, { l + 1.f, b - 1.f }, cc, 2.f);
        }
        // Top-right
        {
            ImU32 cc = EdgeCol(r - e - cs, t - e, r + e, t + e + cs);
            dl->AddLine({ r - cs, t + 1.f }, { r - 1.f, t + 1.f }, cc, 2.f);
            dl->AddLine({ r - 1.f, t + 1.f }, { r - 1.f, t + cs  }, cc, 2.f);
        }
        // Top-left
        {
            ImU32 cc = EdgeCol(l - e, t - e, l + e + cs, t + e + cs);
            dl->AddLine({ l + 1.f, t + 1.f }, { l + cs,  t + 1.f }, cc, 2.f);
            dl->AddLine({ l + 1.f, t + 1.f }, { l + 1.f, t + cs  }, cc, 2.f);
        }
    }

    ImGui::End();                        // ##ui_window_main
    ImGui::PopStyleVar();                // ImGuiStyleVar_Alpha pushed in BeginWindow
    ImGui::GetStyle().Alpha = 1.f;       // Restore global alpha for anything drawn after
}

} // namespace ui_window
