#pragma once
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "ui_anim.hpp"
#include "ui_theme.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

// ============================================================================
//  core/ui_widgets.hpp  —  Custom animated ImGui widgets
//  Requires ui_anim.hpp  (ui_anim::Float, ui_anim::Pulse, easing helpers)
//  Requires ui_theme.hpp (ui_theme::colors::*, DrawGlowRect, DrawGradientRect)
//  All functions are inline; no .cpp needed.
// ============================================================================

namespace ui_widgets {

// Convert ImVec4 colour to packed ImU32 for ImDrawList calls.
inline ImU32 V4(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ── Internal helpers ─────────────────────────────────────────────────────────
namespace detail {

inline float Approach(float cur, float tgt, float speed) {
    float dt = ImGui::GetIO().DeltaTime;
    if (cur < tgt) return (std::min)(cur + speed * dt, tgt);
    return (std::max)(cur - speed * dt, tgt);
}

inline float SmoothStep(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
    int ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
    int bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t),
                    ab + (int)((bb - ab) * t), aa + (int)((ba - aa) * t));
}

inline ImU32 MulAlpha(ImU32 col, float alpha) {
    int a = (col >> IM_COL32_A_SHIFT) & 0xFF;
    a = (int)(a * std::clamp(alpha, 0.f, 1.f));
    return (col & 0x00FFFFFF) | ((ImU32)a << IM_COL32_A_SHIFT);
}

inline void GlowOutline(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImU32 col,
                         float spread = 3.f, float rounding = 4.f) {
    for (int i = 4; i >= 1; --i) {
        float f = (float)i;
        ImU32 gc = (col & 0x00FFFFFF) | ((ImU32)(0.055f * (5 - i) * 255.f) << IM_COL32_A_SHIFT);
        float ex = f * spread * 0.5f;
        dl->AddRect(ImVec2(mn.x - ex, mn.y - ex), ImVec2(mx.x + ex, mx.y + ex),
                    gc, rounding + ex, 0, 1.f + f * 0.35f);
    }
    dl->AddRect(mn, mx, col, rounding, 0, 1.f);
}

struct WidgetState {
    float  v    = 0.f;   // primary animated value
    float  v2   = 0.f;   // secondary
    float  v3   = 0.f;   // tertiary
    bool   flag = false;
    double lastTime = 0.0;
};

inline std::unordered_map<ImGuiID, WidgetState>& StateMap() {
    static std::unordered_map<ImGuiID, WidgetState> s;
    return s;
}
inline WidgetState& GetState(ImGuiID id) { return StateMap()[id]; }

// Key name lookup (Windows VK codes)
inline const char* VKName(int vk) {
    if (vk <= 0) return "NONE";
    static char buf[8];
    switch (vk) {
    case VK_LBUTTON:  return "MOUSE1";  case VK_RBUTTON:  return "MOUSE2";
    case VK_MBUTTON:  return "MOUSE3";  case VK_XBUTTON1: return "MOUSE4";
    case VK_XBUTTON2: return "MOUSE5";  case VK_BACK:     return "BACKSPACE";
    case VK_TAB:      return "TAB";     case VK_RETURN:   return "ENTER";
    case VK_SHIFT:    return "SHIFT";   case VK_CONTROL:  return "CTRL";
    case VK_MENU:     return "ALT";     case VK_CAPITAL:  return "CAPS";
    case VK_ESCAPE:   return "ESC";     case VK_SPACE:    return "SPACE";
    case VK_PRIOR:    return "PGUP";    case VK_NEXT:     return "PGDN";
    case VK_END:      return "END";     case VK_HOME:     return "HOME";
    case VK_LEFT:     return "LEFT";    case VK_UP:       return "UP";
    case VK_RIGHT:    return "RIGHT";   case VK_DOWN:     return "DOWN";
    case VK_INSERT:   return "INS";     case VK_DELETE:   return "DEL";
    case VK_LSHIFT:   return "LSHIFT";  case VK_RSHIFT:   return "RSHIFT";
    case VK_LCONTROL: return "LCTRL";   case VK_RCONTROL: return "RCTRL";
    case VK_LMENU:    return "LALT";    case VK_RMENU:    return "RALT";
    case VK_F1:  return "F1";  case VK_F2:  return "F2";  case VK_F3:  return "F3";
    case VK_F4:  return "F4";  case VK_F5:  return "F5";  case VK_F6:  return "F6";
    case VK_F7:  return "F7";  case VK_F8:  return "F8";  case VK_F9:  return "F9";
    case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
    case VK_NUMPAD0: return "NUM0"; case VK_NUMPAD1: return "NUM1";
    case VK_NUMPAD2: return "NUM2"; case VK_NUMPAD3: return "NUM3";
    case VK_NUMPAD4: return "NUM4"; case VK_NUMPAD5: return "NUM5";
    case VK_NUMPAD6: return "NUM6"; case VK_NUMPAD7: return "NUM7";
    case VK_NUMPAD8: return "NUM8"; case VK_NUMPAD9: return "NUM9";
    default: break;
    }
    if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = 0; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = 0; return buf; }
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

// Global: which KeybindButton is currently listening
inline ImGuiID g_listeningID = 0;

} // namespace detail

// ============================================================================
//  1. AnimatedToggle
//     iOS-style pill toggle — knob slides left/right, track fades gray->accent.
//     Returns true when the value was toggled this frame.
// ============================================================================
inline bool AnimatedToggle(const char* label, bool* value, float speed = 8.f) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    constexpr float W  = 38.f;   // track width
    constexpr float H  = 20.f;   // track height
    constexpr float R  = H * 0.5f;
    constexpr float KR = R - 2.f; // knob radius

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(label, ImVec2(W, H));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    if (clicked && value) {
        *value = !(*value);
        return true;
    }

    // Animated fill: 0.0 = OFF, 1.0 = ON
    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);
    float tgt = (value && *value) ? 1.f : 0.f;
    st.v = detail::Approach(st.v, tgt, speed);
    float t = detail::SmoothStep(st.v);

    ImVec2 trackMn = pos;
    ImVec2 trackMx = ImVec2(pos.x + W, pos.y + H);

    // Glow halo behind track when ON
    if (t > 0.04f) {
        ImU32 glowBase = detail::MulAlpha(V4(ui_theme::colors::accent_glow), t * 0.45f);
        for (int i = 3; i >= 1; --i) {
            float g = (float)i * 2.5f;
            dl->AddRectFilled(ImVec2(trackMn.x - g, trackMn.y - g),
                              ImVec2(trackMx.x + g, trackMx.y + g),
                              detail::MulAlpha(glowBase, 0.08f * (4 - i)), R + g);
        }
    }

    // Track fill (lerp dark gray -> accent)
    ImU32 trackOff = IM_COL32(55, 55, 68, 220);
    ImU32 trackCol = detail::LerpColor(trackOff, V4(ui_theme::colors::accent), t);
    dl->AddRectFilled(trackMn, trackMx, trackCol, R);

    // Inner shadow (dark lip at top of track for depth)
    dl->AddRectFilled(trackMn, ImVec2(trackMx.x, trackMn.y + 3.f),
                      IM_COL32(0, 0, 0, 38), R, ImDrawFlags_RoundCornersTop);

    // Knob: slides from left to right
    float knobX = trackMn.x + R + t * (W - 2.f * R);
    float knobY = trackMn.y + R;

    // Knob shadow
    dl->AddCircleFilled(ImVec2(knobX, knobY + 1.5f), KR, IM_COL32(0, 0, 0, 80), 24);
    // Knob body
    dl->AddCircleFilled(ImVec2(knobX, knobY), KR, IM_COL32(240, 240, 248, 255), 24);
    // Specular highlight
    dl->AddCircleFilled(ImVec2(knobX - KR * 0.28f, knobY - KR * 0.32f),
                        KR * 0.42f, IM_COL32(255, 255, 255, 55), 12);

    // Label to the right
    float ty = trackMn.y + (H - ImGui::GetTextLineHeight()) * 0.5f;
    ImU32 labelCol = hovered ? V4(ui_theme::colors::accent) : IM_COL32(200, 200, 214, 200);
    dl->AddText(ImVec2(trackMx.x + 8.f, ty), labelCol, label);

    float labelW = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + H + 4.f));
    ImGui::Dummy(ImVec2(W + 8.f + labelW, 0.f));

    return false;
}

// ============================================================================
//  2. GradientSliderFloat
//     Dark track, gradient fill from accent_dim to accent, glowing grab knob.
//     Returns true if value changed.
// ============================================================================
inline bool GradientSliderFloat(const char* label, float* v,
                                float vMin, float vMax,
                                const char* fmt = "%.1f") {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImGuiIO&    io  = ImGui::GetIO();
    float       avW = ImGui::GetContentRegionAvail().x;

    constexpr float TRACK_H  = 6.f;
    constexpr float GRAB_R   = 8.f;
    constexpr float SIDE_PAD = GRAB_R + 2.f;

    // Reserve right side for label + value text
    float rightW = ImGui::CalcTextSize(label).x + 56.f;
    float trackW = avW - SIDE_PAD * 2.f - rightW;
    if (trackW < 40.f) trackW = 40.f;

    ImVec2 pos    = ImGui::GetCursorScreenPos();
    float  trackX = pos.x + SIDE_PAD;
    float  trackY = pos.y + GRAB_R + 2.f;

    ImGui::SetCursorScreenPos(ImVec2(trackX - SIDE_PAD, pos.y));
    ImGui::InvisibleButton(label, ImVec2(trackW + SIDE_PAD * 2.f, GRAB_R * 2.f + 4.f));
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    bool changed = false;
    if (active && v) {
        float t = std::clamp((io.MousePos.x - trackX) / trackW, 0.f, 1.f);
        float nv = vMin + t * (vMax - vMin);
        if (nv != *v) { *v = nv; changed = true; }
    }

    float frac = v ? std::clamp((*v - vMin) / (vMax - vMin), 0.f, 1.f) : 0.f;

    // Animated grab scale
    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);
    st.v = detail::Approach(st.v, (active || hovered) ? 1.25f : 1.f, 12.f);
    float sr = GRAB_R * st.v;

    ImVec2 tl = ImVec2(trackX,            trackY - TRACK_H * 0.5f);
    ImVec2 br = ImVec2(trackX + trackW,   trackY + TRACK_H * 0.5f);

    // Track background
    dl->AddRectFilled(tl, br, IM_COL32(32, 32, 44, 220), TRACK_H * 0.5f);
    dl->AddRect(tl, br, IM_COL32(65, 65, 85, 110), TRACK_H * 0.5f, 0, 1.f);

    // Gradient filled portion
    if (frac > 0.001f) {
        float fx = trackX + trackW * frac;
        dl->AddRectFilledMultiColor(tl, ImVec2(fx, br.y),
            V4(ui_theme::colors::accent_dim), V4(ui_theme::colors::accent),
            V4(ui_theme::colors::accent),     V4(ui_theme::colors::accent_dim));
    }

    float grabX = trackX + trackW * frac;

    // Grab glow layers
    if (hovered || active) {
        float glowAlpha = active ? 0.45f : 0.25f;
        ImU32 gc = detail::MulAlpha(V4(ui_theme::colors::accent_glow), glowAlpha * st.v);
        for (int i = 3; i >= 1; --i)
            dl->AddCircleFilled(ImVec2(grabX, trackY), sr + i * 3.f,
                                detail::MulAlpha(gc, 0.06f * (4 - i)), 24);
    }

    // Grab shadow + body
    dl->AddCircleFilled(ImVec2(grabX, trackY + 1.5f), sr, IM_COL32(0,0,0,75), 24);
    ImU32 grabCol = active
        ? V4(ui_theme::colors::accent)
        : detail::LerpColor(IM_COL32(220,220,235,255), V4(ui_theme::colors::accent),
                            (hovered ? 0.35f : 0.f));
    dl->AddCircleFilled(ImVec2(grabX, trackY), sr, grabCol, 24);
    // Specular
    dl->AddCircleFilled(ImVec2(grabX - sr * 0.25f, trackY - sr * 0.3f),
                        sr * 0.38f, IM_COL32(255,255,255,55), 12);

    // Label + value text
    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), fmt, v ? *v : 0.f);
    float labelW  = ImGui::CalcTextSize(label).x;
    float valW    = ImGui::CalcTextSize(valBuf).x;
    float textY   = pos.y + (GRAB_R * 2.f + 4.f - ImGui::GetTextLineHeight()) * 0.5f;
    float textX   = trackX + trackW + SIDE_PAD + 4.f;

    dl->AddText(ImVec2(textX, textY), IM_COL32(195,195,210,200), label);
    dl->AddText(ImVec2(textX + labelW + 5.f, textY), V4(ui_theme::colors::accent), valBuf);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + GRAB_R * 2.f + 8.f));
    ImGui::Dummy(ImVec2(avW, 0.f));

    return changed;
}

// ============================================================================
//  3. GlowButton
//     Rounded rect with hover glow, scale pop, and click flash.
//     Returns true if clicked.
// ============================================================================
inline bool GlowButton(const char* label, ImVec2 size = ImVec2(0.f, 0.f)) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    ImVec2 textSz = ImGui::CalcTextSize(label);
    if (size.x <= 0.f) size.x = textSz.x + 24.f;
    if (size.y <= 0.f) size.y = textSz.y + 12.f;
    constexpr float ROUNDING = 6.f;

    ImGui::InvisibleButton(label, size);
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);

    // st.v  = hover progress [0,1]
    // st.v2 = flash timer [1->0] on click
    st.v = detail::Approach(st.v, (hovered || active) ? 1.f : 0.f, 10.f);
    if (clicked) st.v2 = 1.f;
    st.v2 = detail::Approach(st.v2, 0.f, 6.f);

    float h     = st.v;
    float flash = st.v2;

    // Scale: expand slightly on hover
    float scale = 1.f + h * 0.018f;
    float dw = (size.x * scale - size.x) * 0.5f;
    float dh = (size.y * scale - size.y) * 0.5f;
    ImVec2 mn = ImVec2(pos.x - dw, pos.y - dh);
    ImVec2 mx = ImVec2(pos.x + size.x + dw, pos.y + size.y + dh);

    // Background
    ImU32 bgNorm  = IM_COL32(40, 40, 56, 220);
    ImU32 bgHover = IM_COL32(50, 50, 72, 240);
    ImU32 bg = detail::LerpColor(
        detail::LerpColor(bgNorm, bgHover, h),
        V4(ui_theme::colors::accent), flash * 0.25f);
    dl->AddRectFilled(mn, mx, bg, ROUNDING);

    // Sheen (top half lighter gradient)
    dl->AddRectFilledMultiColor(mn, ImVec2(mx.x, mn.y + (mx.y - mn.y) * 0.5f),
        IM_COL32(255,255,255,14), IM_COL32(255,255,255,14),
        IM_COL32(255,255,255, 0), IM_COL32(255,255,255, 0));

    // Border + optional glow
    if (h > 0.02f || flash > 0.02f) {
        float ba = (std::max)(h, flash);
        dl->AddRect(mn, mx, detail::MulAlpha(V4(ui_theme::colors::accent), ba * 0.88f),
                    ROUNDING, 0, 1.2f);
        detail::GlowOutline(dl, mn, mx,
            detail::MulAlpha(V4(ui_theme::colors::accent_glow), ba * 0.65f),
            3.f, ROUNDING);
    } else {
        dl->AddRect(mn, mx, IM_COL32(68, 68, 90, 120), ROUNDING, 0, 1.f);
    }

    // Label
    ImU32 textCol = detail::LerpColor(IM_COL32(178,178,198,220), V4(ui_theme::colors::accent), h);
    textCol = detail::LerpColor(textCol, IM_COL32(255,255,255,255), flash * 0.5f);
    dl->AddText(ImVec2(pos.x + (size.x - textSz.x) * 0.5f,
                       pos.y + (size.y - textSz.y) * 0.5f),
                textCol, label);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 4.f));
    ImGui::Dummy(ImVec2(size.x, 0.f));

    return clicked;
}

// ============================================================================
//  4. SectionHeader
//     Left accent bar, bold label, animated chevron, separator.
//     Returns the current open state.  If open==nullptr, always returns true.
// ============================================================================
inline bool SectionHeader(const char* label, bool* open = nullptr) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       avW = ImGui::GetContentRegionAvail().x;
    float       lH  = ImGui::GetTextLineHeightWithSpacing() + 6.f;

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(label, ImVec2(avW, lH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);

    bool isOpen = open ? *open : true;
    if (clicked && open) { *open = !(*open); isOpen = *open; }

    // st.v = chevron angle: 0 when open, -pi/2 when closed
    float tgtAngle = isOpen ? 0.f : -1.5707963f;
    st.v  = detail::Approach(st.v, tgtAngle, 12.f);
    // st.v2 = hover brightness [0,1]
    st.v2 = detail::Approach(st.v2, hovered ? 1.f : 0.f, 10.f);

    // Hover background
    if (st.v2 > 0.01f)
        dl->AddRectFilled(pos, ImVec2(pos.x + avW, pos.y + lH),
                          IM_COL32(255,255,255, (int)(8.f * st.v2)), 3.f);

    // Left accent bar (3 px)
    ImU32 barCol = detail::LerpColor(V4(ui_theme::colors::accent_dim), V4(ui_theme::colors::accent), st.v2);
    dl->AddRectFilled(ImVec2(pos.x, pos.y + 4.f),
                      ImVec2(pos.x + 3.f, pos.y + lH - 4.f),
                      barCol, 1.5f);

    // Label (doubled draw for pseudo-bold)
    float tx = pos.x + 10.f;
    float ty = pos.y + (lH - ImGui::GetTextLineHeight()) * 0.5f;
    ImU32 labelCol = detail::LerpColor(IM_COL32(200,200,218,220), IM_COL32(230,230,248,255), st.v2);
    dl->AddText(ImVec2(tx + 0.5f, ty + 0.5f), IM_COL32(0,0,0,55), label); // drop shadow
    dl->AddText(ImVec2(tx,        ty),         labelCol,             label);

    // Chevron (V-shape, rotated)
    if (open) {
        float cx  = pos.x + avW - 14.f;
        float cy  = pos.y + lH * 0.5f;
        float ang = st.v - 1.5707963f;  // so 0=pointing down, -pi/2=pointing right
        auto rot = [&](float rx, float ry) -> ImVec2 {
            float c = cosf(ang), s = sinf(ang);
            return ImVec2(cx + rx * c - ry * s, cy + rx * s + ry * c);
        };
        constexpr float hs = 4.5f;
        ImU32 chevCol = detail::LerpColor(IM_COL32(130,130,152,200), V4(ui_theme::colors::accent), st.v2);
        dl->AddLine(rot(-hs, -hs * 0.6f), rot(0.f, hs * 0.6f), chevCol, 1.6f);
        dl->AddLine(rot( hs, -hs * 0.6f), rot(0.f, hs * 0.6f), chevCol, 1.6f);
    }

    // Separator under header
    dl->AddLine(ImVec2(pos.x + 6.f, pos.y + lH - 1.f),
                ImVec2(pos.x + avW - 6.f, pos.y + lH - 1.f),
                IM_COL32(75, 75, 98, 80), 1.f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + lH + 2.f));
    ImGui::Dummy(ImVec2(avW, 0.f));

    return open ? *open : true;
}

// ============================================================================
//  5. TabButton
//     Pill-shaped tab. Active: filled accent + glow. Smooth transitions.
//     Returns true if clicked (caller should switch tab index).
// ============================================================================
inline bool TabButton(const char* label, bool isActive, int tabIdx) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    ImVec2 textSz  = ImGui::CalcTextSize(label);
    float  btnW    = textSz.x + 20.f;
    float  btnH    = textSz.y + 10.f;
    float  rounding = btnH * 0.5f; // full pill

    ImGui::PushID(tabIdx);
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(label, ImVec2(btnW, btnH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    // Unique state ID combining label + index
    char keyBuf[80];
    snprintf(keyBuf, sizeof(keyBuf), "__tab%d_%s", tabIdx, label);
    ImGuiID          id = ImGui::GetID(keyBuf);
    detail::WidgetState& st = detail::GetState(id);

    // st.v = active fill [0,1], st.v2 = hover [0,1]
    st.v  = detail::Approach(st.v,  isActive ? 1.f : 0.f, 10.f);
    st.v2 = detail::Approach(st.v2, hovered  ? 1.f : 0.f, 10.f);

    float t   = detail::SmoothStep(st.v);
    float hov = st.v2;

    ImVec2 mn = pos;
    ImVec2 mx = ImVec2(pos.x + btnW, pos.y + btnH);

    // Background
    ImU32 bgInactive = IM_COL32(255,255,255, (int)(20.f * hov));
    ImU32 bg = detail::LerpColor(bgInactive, V4(ui_theme::colors::accent), t);
    dl->AddRectFilled(mn, mx, bg, rounding);

    // Glow when active
    if (t > 0.05f)
        detail::GlowOutline(dl, mn, mx,
            detail::MulAlpha(V4(ui_theme::colors::accent_glow), t * 0.55f),
            3.f, rounding);

    // Text
    ImU32 textInactive = IM_COL32(138,138,160,200);
    ImU32 textHover    = IM_COL32(190,190,215,255);
    ImU32 textActive   = IM_COL32(255,255,255,255);
    ImU32 textCol = detail::LerpColor(
        detail::LerpColor(textInactive, textHover, hov), textActive, t);

    dl->AddText(ImVec2(mn.x + (btnW - textSz.x) * 0.5f,
                       mn.y + (btnH - textSz.y) * 0.5f),
                textCol, label);

    // Advance cursor past this button (caller places buttons side by side with SameLine)
    ImGui::SetCursorScreenPos(ImVec2(mx.x + 4.f, pos.y));
    ImGui::Dummy(ImVec2(0.f, btnH));

    return clicked;
}

// ============================================================================
//  6. ColorPreview
//     Rounded swatch, checkerboard BG, hover border, click opens picker popup.
// ============================================================================
inline void ColorPreview(const char* label, float color[3]) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    constexpr float SW = 22.f;
    constexpr float SH = 22.f;
    constexpr float R  = 4.f;

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(label, ImVec2(SW, SH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);
    st.v = detail::Approach(st.v, hovered ? 1.f : 0.f, 10.f);

    // Checkerboard behind swatch
    constexpr float CS = 5.f; // cell size
    int cols = (int)(SW / CS) + 1, rows = (int)(SH / CS) + 1;
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col) {
            bool dark = (row + col) % 2 == 0;
            ImU32 c = dark ? IM_COL32(110,110,110,255) : IM_COL32(170,170,170,255);
            float x0 = pos.x + col * CS, y0 = pos.y + row * CS;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + CS + 0.5f, y0 + CS + 0.5f), c);
        }

    // Clip checker to rounded bounds
    dl->PushClipRect(pos, ImVec2(pos.x + SW, pos.y + SH), true);
    // Color swatch
    if (color) {
        ImU32 col = IM_COL32((int)(color[0]*255.f), (int)(color[1]*255.f), (int)(color[2]*255.f), 255);
        dl->AddRectFilled(pos, ImVec2(pos.x + SW, pos.y + SH), col, R);
    }
    dl->PopClipRect();

    // Border (brightens on hover, turns accent)
    ImU32 borderCol = detail::LerpColor(IM_COL32(75,75,95,160), V4(ui_theme::colors::accent), st.v);
    dl->AddRect(pos, ImVec2(pos.x + SW, pos.y + SH), borderCol, R, 0, 1.2f);

    // Label
    float ty = pos.y + (SH - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText(ImVec2(pos.x + SW + 8.f, ty), IM_COL32(188,188,205,200), label);

    // Color picker popup
    if (clicked && color) ImGui::OpenPopup(label);
    if (ImGui::BeginPopup(label)) {
        ImGui::ColorPicker3(label, color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::EndPopup();
    }

    float labelW = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + SH + 4.f));
    ImGui::Dummy(ImVec2(SW + 8.f + labelW, 0.f));
}

// ============================================================================
//  7. KeybindButton
//     Right-aligned key-name button; click enters listening mode.
//     Returns true when a new key is set.
// ============================================================================
inline bool KeybindButton(const char* label, int* vkCode) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImGuiIO&    io  = ImGui::GetIO();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       avW = ImGui::GetContentRegionAvail().x;

    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);
    bool listening = (detail::g_listeningID == id);

    constexpr float BTN_W = 82.f;
    float           btnH  = ImGui::GetTextLineHeight() + 8.f;
    constexpr float R     = 4.f;

    // Label on left
    float ty = pos.y + (btnH - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText(ImVec2(pos.x, ty), IM_COL32(190,190,207,200), label);

    // Button rect on right
    float  btnX = pos.x + avW - BTN_W;
    ImVec2 mn   = ImVec2(btnX, pos.y);
    ImVec2 mx   = ImVec2(btnX + BTN_W, pos.y + btnH);

    ImGui::SetCursorScreenPos(mn);
    std::string btnId = std::string("##kb_") + label;
    ImGui::InvisibleButton(btnId.c_str(), ImVec2(BTN_W, btnH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    if (clicked) {
        detail::g_listeningID = (listening ? 0 : id);
        listening = !listening;
    }

    bool changed = false;
    if (listening) {
        // Check all VK codes for fresh press (use flag to avoid holding)
        bool anyHeld = false;
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_ESCAPE) continue;
            if (GetAsyncKeyState(vk) & 0x8000) {
                anyHeld = true;
                if (!st.flag) {
                    if (vkCode) *vkCode = vk;
                    detail::g_listeningID = 0;
                    listening = false;
                    st.flag  = true;
                    changed  = true;
                    break;
                }
            }
        }
        if (!anyHeld) st.flag = false;

        // ESC to cancel
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            detail::g_listeningID = 0;
            listening = false;
        }
    }

    // Pulse for listening state
    float pulse = listening ? 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 6.f) : 0.f;
    // Hover
    st.v2 = detail::Approach(st.v2, hovered ? 1.f : 0.f, 10.f);

    // Background
    ImU32 bg = listening
        ? detail::LerpColor(IM_COL32(50,28,72,240),
                            detail::MulAlpha(V4(ui_theme::colors::accent), 220.f / 255.f),
                            pulse * 0.18f)
        : (hovered ? IM_COL32(48,48,68,230) : IM_COL32(34,34,50,220));
    dl->AddRectFilled(mn, mx, bg, R);

    // Border
    ImU32 border = listening
        ? detail::LerpColor(V4(ui_theme::colors::accent_dim), V4(ui_theme::colors::accent), pulse)
        : (hovered ? detail::MulAlpha(V4(ui_theme::colors::accent), 0.6f) : IM_COL32(68,68,90,140));
    dl->AddRect(mn, mx, border, R, 0, 1.2f);

    // Pulsing glow when listening
    if (listening)
        detail::GlowOutline(dl, mn, mx,
            detail::MulAlpha(V4(ui_theme::colors::accent_glow), pulse * 0.65f),
            4.f, R);

    // Key text
    const char* keyStr = listening ? "..." : (vkCode ? detail::VKName(*vkCode) : "NONE");
    ImVec2      keySz  = ImGui::CalcTextSize(keyStr);
    ImU32 keyCol = listening
        ? detail::LerpColor(IM_COL32(180,140,255,200), IM_COL32(225,185,255,255), pulse)
        : (hovered ? V4(ui_theme::colors::accent) : IM_COL32(178,178,200,225));
    dl->AddText(ImVec2(mn.x + (BTN_W - keySz.x) * 0.5f, mn.y + (btnH - keySz.y) * 0.5f),
                keyCol, keyStr);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + btnH + 4.f));
    ImGui::Dummy(ImVec2(avW, 0.f));

    return changed;
}

// ============================================================================
//  8. SearchBar
//     Rounded input with magnifying glass icon, placeholder, and clear (X) btn.
//     Returns true if text changed.
// ============================================================================
inline bool SearchBar(const char* label, char* buf, int bufSize) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       avW = ImGui::GetContentRegionAvail().x;

    constexpr float H       = 26.f;
    constexpr float ICON_OX = 9.f;   // icon left offset
    constexpr float ICON_R  = 5.5f;
    constexpr float X_BTN_W = 18.f;
    float           rounding = H * 0.5f;
    bool            hasText  = buf && buf[0] != '\0';

    // Unique ID for focus detection
    ImGuiID id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);

    ImVec2 mn = pos;
    ImVec2 mx = ImVec2(pos.x + avW, pos.y + H);

    // Detect focus after InputText (compare active ID)
    // We push the ID ahead of InputText so we can track it
    bool focused = (ImGui::GetActiveID() == id);
    st.v = detail::Approach(st.v, focused ? 1.f : 0.f, 10.f);

    // Glow when focused
    if (st.v > 0.01f)
        detail::GlowOutline(dl, mn, mx,
            detail::MulAlpha(V4(ui_theme::colors::accent_glow), st.v * 0.45f),
            3.f, rounding);

    // Background
    dl->AddRectFilled(mn, mx, IM_COL32(28, 28, 42, 235), rounding);
    ImU32 border = detail::LerpColor(IM_COL32(58,58,78,160), V4(ui_theme::colors::accent), st.v * 0.75f);
    dl->AddRect(mn, mx, border, rounding, 0, 1.2f);

    // Magnifying glass icon
    float icx = pos.x + ICON_OX + ICON_R;
    float icy = pos.y + H * 0.5f;
    ImU32 iconCol = detail::LerpColor(IM_COL32(110,110,138,200), V4(ui_theme::colors::accent), st.v);
    dl->AddCircle(ImVec2(icx, icy), ICON_R, iconCol, 16, 1.5f);
    constexpr float HANDLE_ANG = 0.7853982f; // 45°
    ImVec2 hStart = ImVec2(icx + ICON_R * cosf(HANDLE_ANG), icy + ICON_R * sinf(HANDLE_ANG));
    ImVec2 hEnd   = ImVec2(icx + (ICON_R + 5.f) * cosf(HANDLE_ANG),
                           icy + (ICON_R + 5.f) * sinf(HANDLE_ANG));
    dl->AddLine(hStart, hEnd, iconCol, 2.f);

    // InputText: positioned after icon, before optional X button
    float inputX = pos.x + ICON_OX + ICON_R * 2.f + 8.f;
    float inputW = avW - (inputX - pos.x) - (hasText ? X_BTN_W + 6.f : 6.f);
    float inputY = pos.y + (H - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(inputX, inputY));
    ImGui::SetNextItemWidth(inputW);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
    bool changed = ImGui::InputText("##input", buf, (size_t)bufSize,
                                    ImGuiInputTextFlags_NoHorizontalScroll);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::PopID();

    // Placeholder when empty and not active
    if (buf && buf[0] == '\0' && !ImGui::IsItemActive()) {
        char ph[80];
        snprintf(ph, sizeof(ph), "Search %s...", label);
        dl->AddText(ImVec2(inputX, inputY), IM_COL32(72,72,96,140), ph);
    }

    // Clear (X) button when text is present
    if (hasText) {
        float xCx = pos.x + avW - X_BTN_W * 0.5f - 3.f;
        float xCy = pos.y + H * 0.5f;
        ImVec2 xMn = ImVec2(xCx - X_BTN_W * 0.5f, xCy - X_BTN_W * 0.5f);
        ImVec2 xMx = ImVec2(xCx + X_BTN_W * 0.5f, xCy + X_BTN_W * 0.5f);

        ImGui::SetCursorScreenPos(xMn);
        std::string xId = std::string("##xbtn_") + label;
        ImGui::InvisibleButton(xId.c_str(), ImVec2(X_BTN_W, X_BTN_W));
        bool xHov  = ImGui::IsItemHovered();
        bool xClick = ImGui::IsItemClicked();

        if (xClick && buf) { buf[0] = '\0'; changed = true; }

        float xs = 3.8f;
        ImU32 xCol = xHov ? IM_COL32(222,90,90,255) : IM_COL32(115,115,142,185);
        dl->AddLine(ImVec2(xCx-xs, xCy-xs), ImVec2(xCx+xs, xCy+xs), xCol, 1.8f);
        dl->AddLine(ImVec2(xCx+xs, xCy-xs), ImVec2(xCx-xs, xCy+xs), xCol, 1.8f);
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + H + 4.f));
    ImGui::Dummy(ImVec2(avW, 0.f));

    return changed;
}

// ============================================================================
//  9. NotificationToast system
//     ShowToast()  — queue a notification
//     DrawToasts() — call once per frame (pass window draw list or background)
// ============================================================================

enum class ToastType { Info, Success, Warning, Error };

namespace detail {
struct Toast {
    std::string message;
    ToastType   type;
    float       duration;
    float       elapsed;
    float       slideX;  // offset from final position (positive = offscreen right)
};
inline std::vector<Toast>& Toasts() {
    static std::vector<Toast> v;
    return v;
}
} // namespace detail

inline void ShowToast(const char* message, ToastType type = ToastType::Info, float duration = 3.f) {
    detail::Toast t;
    t.message  = message;
    t.type     = type;
    t.duration = duration;
    t.elapsed  = 0.f;
    t.slideX   = 310.f;
    detail::Toasts().push_back(std::move(t));
}

inline void DrawToasts(ImDrawList* dl) {
    auto& toasts = detail::Toasts();
    float   dt   = ImGui::GetIO().DeltaTime;
    ImVec2  disp = ImGui::GetIO().DisplaySize;

    constexpr float TW       = 275.f;
    constexpr float TH       = 44.f;
    constexpr float PAD      = 8.f;
    constexpr float ROUNDING = 6.f;

    // Prune dead toasts
    for (int i = (int)toasts.size() - 1; i >= 0; --i)
        if (toasts[i].elapsed >= toasts[i].duration + 0.45f)
            toasts.erase(toasts.begin() + i);

    for (int i = 0; i < (int)toasts.size(); ++i) {
        detail::Toast& t = toasts[i];
        t.elapsed += dt;

        // Slide animation: rush in, hold, slide out
        float slideTarget = (t.elapsed >= t.duration) ? 310.f : 0.f;
        float slideSpeed  = 9.f;
        float slideRate   = slideSpeed * dt * 310.f;
        if (t.slideX > slideTarget) t.slideX = (std::max)(t.slideX - slideRate, slideTarget);
        else                        t.slideX = (std::min)(t.slideX + slideRate, slideTarget);

        // Fade in/out
        float alpha = 1.f;
        if (t.elapsed < 0.18f)        alpha = t.elapsed / 0.18f;
        if (t.elapsed >= t.duration)  alpha = 1.f - (std::min)((t.elapsed - t.duration) / 0.45f, 1.f);
        alpha = std::clamp(alpha, 0.f, 1.f);

        float bx = disp.x - TW - 12.f + t.slideX;
        float by = disp.y - 12.f - (TH + PAD) * (float)(i + 1);
        ImVec2 mn = ImVec2(bx, by);
        ImVec2 mx = ImVec2(bx + TW, by + TH);

        // Type accent
        ImU32 accent;
        const char* icon;
        switch (t.type) {
        case ToastType::Success: accent = IM_COL32(48,198,100,255); icon = "[OK]"; break;
        case ToastType::Warning: accent = IM_COL32(255,178,0,255);  icon = "[!!]"; break;
        case ToastType::Error:   accent = IM_COL32(218,58,58,255);  icon = "[X]";  break;
        default:                 accent = IM_COL32(0,175,215,255);  icon = "[i]";  break;
        }

        // Background
        dl->AddRectFilled(mn, mx, detail::MulAlpha(IM_COL32(22,22,36,245), alpha), ROUNDING);

        // Left color stripe
        dl->AddRectFilled(mn, ImVec2(mn.x + 4.f, mx.y),
                          detail::MulAlpha(accent, alpha), ROUNDING, ImDrawFlags_RoundCornersLeft);

        // Border
        dl->AddRect(mn, mx, detail::MulAlpha(IM_COL32(68,68,92,200), alpha), ROUNDING, 0, 1.f);

        // Stripe glow
        for (int g = 1; g <= 2; ++g)
            dl->AddRectFilled(ImVec2(mn.x-g, mn.y-g), ImVec2(mn.x+4.f+g, mx.y+g),
                              detail::MulAlpha(accent, alpha * 0.055f * (3-g)), ROUNDING);

        // Progress bar at bottom
        float progress = (std::max)(1.f - t.elapsed / t.duration, 0.f);
        if (progress > 0.001f) {
            float barY = mx.y - 3.f;
            dl->AddRectFilled(
                ImVec2(mn.x + 4.f, barY),
                ImVec2(mn.x + 4.f + (TW - 4.f) * progress, mx.y - 1.f),
                detail::MulAlpha(accent, alpha * 0.48f), 1.f);
        }

        // Icon + message
        float ty     = mn.y + (TH - ImGui::GetTextLineHeight()) * 0.5f;
        float iconW  = ImGui::CalcTextSize(icon).x;
        dl->AddText(ImVec2(mn.x + 8.f, ty), detail::MulAlpha(accent, alpha), icon);
        float msgX  = mn.x + 8.f + iconW + 6.f;
        float msgMaxW = TW - (msgX - mn.x) - 8.f;
        dl->AddText(nullptr, 0.f, ImVec2(msgX, ty),
                    detail::MulAlpha(IM_COL32(208,208,225,255), alpha),
                    t.message.c_str(), nullptr, msgMaxW);
    }
}

// ============================================================================
//  10. ProgressBar
//      Rounded gradient fill with animated shimmer.
// ============================================================================
inline void ProgressBar(const char* label, float fraction, bool animated = true) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       avW = ImGui::GetContentRegionAvail().x;

    float labelW = ImGui::CalcTextSize(label).x;
    float barW   = avW - labelW - 50.f; // leave room for "100%" + label
    if (barW < 60.f) barW = 60.f;

    constexpr float BAR_H   = 10.f;
    constexpr float ROUNDING = BAR_H * 0.5f;

    fraction = std::clamp(fraction, 0.f, 1.f);

    // Animate fill toward target
    ImGuiID          id = ImGui::GetID(label);
    detail::WidgetState& st = detail::GetState(id);
    st.v = detail::Approach(st.v, fraction, 2.5f);
    float fillFrac = st.v;

    float lineH = ImGui::GetTextLineHeight();
    ImVec2 trackMn = ImVec2(pos.x, pos.y + (lineH - BAR_H) * 0.5f);
    ImVec2 trackMx = ImVec2(pos.x + barW, trackMn.y + BAR_H);

    // Track
    dl->AddRectFilled(trackMn, trackMx, IM_COL32(32,32,48,220), ROUNDING);
    dl->AddRect(trackMn, trackMx, IM_COL32(58,58,78,100), ROUNDING, 0, 1.f);

    // Gradient fill
    float fillX = trackMn.x + barW * fillFrac;
    if (fillFrac > 0.005f) {
        dl->AddRectFilledMultiColor(
            trackMn, ImVec2(fillX, trackMx.y),
            V4(ui_theme::colors::accent_dim), V4(ui_theme::colors::accent),
            V4(ui_theme::colors::accent),     V4(ui_theme::colors::accent_dim));

        // Animated shimmer
        if (animated && fraction < 1.f && fillFrac > 0.01f) {
            float time     = (float)ImGui::GetTime();
            float shimmerT = fmodf(time * 0.55f, 1.3f) - 0.15f;
            float shimX    = trackMn.x + shimmerT * barW * fillFrac;
            float shimW    = barW * fillFrac * 0.22f;

            dl->PushClipRect(trackMn, ImVec2(fillX, trackMx.y), true);
            dl->AddRectFilledMultiColor(
                ImVec2(shimX - shimW, trackMn.y),
                ImVec2(shimX + shimW, trackMx.y),
                IM_COL32(255,255,255,0), IM_COL32(255,255,255,38),
                IM_COL32(255,255,255,38), IM_COL32(255,255,255,0));
            dl->PopClipRect();
        }

        // Leading-edge glow cap
        if (fillFrac > 0.02f) {
            for (int g = 1; g <= 3; ++g)
                dl->AddRectFilled(
                    ImVec2(fillX - (float)g, trackMn.y - 1.f),
                    ImVec2(fillX,            trackMx.y + 1.f),
                    detail::MulAlpha(V4(ui_theme::colors::accent_glow), 0.07f * (4 - g)), ROUNDING);
        }
    }

    // Label and percentage on right
    char pctBuf[10];
    snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", fraction * 100.f);
    float textX = pos.x + barW + 6.f;
    dl->AddText(ImVec2(textX, pos.y), V4(ui_theme::colors::accent), pctBuf);
    float pctW = ImGui::CalcTextSize(pctBuf).x;
    dl->AddText(ImVec2(textX + pctW + 6.f, pos.y), IM_COL32(185,185,202,200), label);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + BAR_H + 6.f));
    ImGui::Dummy(ImVec2(avW, 0.f));
}

} // namespace ui_widgets
