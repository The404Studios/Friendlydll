#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <imgui/imgui.h>

namespace menu_anim {

// ── Easing ──────────────────────────────────────────────────────────────────
inline float EaseOutCubic(float t) { return 1.f - powf(1.f - t, 3.f); }
inline float EaseOutBack(float t) {
    constexpr float c1 = 1.70158f, c3 = c1 + 1.f;
    return 1.f + c3 * powf(t - 1.f, 3.f) + c1 * powf(t - 1.f, 2.f);
}
inline float EaseInOutQuad(float t) {
    return t < 0.5f ? 2.f * t * t : 1.f - powf(-2.f * t + 2.f, 2.f) / 2.f;
}

// ── Color Utilities ─────────────────────────────────────────────────────────
inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
    int ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
    int bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(
        ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t),
        ab + (int)((bb - ab) * t), aa + (int)((ba - aa) * t));
}

inline ImU32 HsvToRgb(float h, float s, float v, float a = 1.f) {
    float c = v * s, x = c * (1.f - fabsf(fmodf(h / 60.f, 2.f) - 1.f)), m = v - c;
    float r, g, b;
    if      (h < 60.f)  { r=c; g=x; b=0; }
    else if (h < 120.f) { r=x; g=c; b=0; }
    else if (h < 180.f) { r=0; g=c; b=x; }
    else if (h < 240.f) { r=0; g=x; b=c; }
    else if (h < 300.f) { r=x; g=0; b=c; }
    else                { r=c; g=0; b=x; }
    return IM_COL32((int)((r+m)*255), (int)((g+m)*255), (int)((b+m)*255), (int)(a*255));
}

inline ImU32 PulseAlpha(ImU32 base, float lo, float hi, float speed) {
    float t = (sinf((float)ImGui::GetTime() * speed) + 1.f) * 0.5f;
    float alpha = lo + (hi - lo) * t;
    return (base & 0x00FFFFFF) | ((ImU32)(alpha * 255.f) << IM_COL32_A_SHIFT);
}

// ── Particles ───────────────────────────────────────────────────────────────
struct Particle {
    float x, y, vx, vy, life, maxLife, size;
    ImU32 baseColor;
    bool active = false;
};

constexpr int MAX_PARTICLES = 48;

struct ParticleSystem {
    std::array<Particle, MAX_PARTICLES> p{};
    float emitAccum = 0.f;

    void Emit(float areaX, float areaY, float areaW, float areaH, int count = 1) {
        for (int c = 0; c < count; ++c) {
            for (auto& pt : p) {
                if (pt.active) continue;
                pt.active = true;
                pt.x = areaX + ((float)(rand() % 1000) / 1000.f) * areaW;
                pt.y = areaY + areaH * 0.8f + ((float)(rand() % 1000) / 1000.f) * areaH * 0.2f;
                pt.vx = ((float)(rand() % 1000) / 1000.f - 0.5f) * 15.f;
                pt.vy = -20.f - ((float)(rand() % 1000) / 1000.f) * 30.f;
                pt.life = 1.5f + ((float)(rand() % 1000) / 1000.f) * 2.f;
                pt.maxLife = pt.life;
                pt.size = 1.f + ((float)(rand() % 1000) / 1000.f) * 1.5f;
                int hue = 180 + (rand() % 60); // cyan-blue range
                pt.baseColor = HsvToRgb((float)hue, 0.8f, 0.95f);
                break;
            }
        }
    }

    void Update(float dt, float areaX, float areaY, float areaW, float areaH) {
        emitAccum += dt;
        float emitRate = 0.08f;
        while (emitAccum >= emitRate) {
            emitAccum -= emitRate;
            Emit(areaX, areaY, areaW, areaH);
        }

        for (auto& pt : p) {
            if (!pt.active) continue;
            pt.x += pt.vx * dt;
            pt.y += pt.vy * dt;
            pt.vy += 2.f * dt; // slight gravity dampening float-up
            pt.life -= dt;
            if (pt.life <= 0.f) pt.active = false;
        }
    }

    void Draw(ImDrawList* dl) const {
        for (const auto& pt : p) {
            if (!pt.active) continue;
            float t = pt.life / pt.maxLife;
            float alpha = t < 0.3f ? t / 0.3f : (t > 0.7f ? (1.f - t) / 0.3f : 1.f);
            alpha *= 0.6f;
            ImU32 col = (pt.baseColor & 0x00FFFFFF) | ((ImU32)(alpha * 255.f) << IM_COL32_A_SHIFT);
            ImU32 glow = (pt.baseColor & 0x00FFFFFF) | ((ImU32)(alpha * 80.f) << IM_COL32_A_SHIFT);
            dl->AddCircleFilled(ImVec2(pt.x, pt.y), pt.size * 2.f, glow, 8);
            dl->AddCircleFilled(ImVec2(pt.x, pt.y), pt.size, col, 8);
        }
    }
};

// ── Tab Transition ──────────────────────────────────────────────────────────
struct TabTransition {
    float progress = 1.f;
    float speed = 10.f;

    bool Trigger() {
        if (progress >= 1.f) { progress = 0.f; return true; }
        return false;
    }

    void Update(float dt) {
        if (progress < 1.f) {
            progress = (std::min)(progress + speed * dt, 1.f);
        }
    }

    float Alpha() const { return EaseOutCubic(progress); }
    float OffsetY() const { return (1.f - EaseOutCubic(progress)) * 12.f; }
};

// ── Animated Gradient Bar ───────────────────────────────────────────────────
inline void DrawAnimatedGradientBar(ImDrawList* dl, float x, float y, float w, float h) {
    float time = (float)ImGui::GetTime();
    int segments = 16;
    float segW = w / segments;
    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i / segments;
        float t1 = (float)(i + 1) / segments;
        float hue0 = fmodf((t0 * 0.3f + time * 0.15f) * 360.f, 360.f);
        float hue1 = fmodf((t1 * 0.3f + time * 0.15f) * 360.f, 360.f);
        // Keep hues in the cyan-blue-purple range (170-290)
        hue0 = 170.f + fmodf(hue0, 120.f);
        hue1 = 170.f + fmodf(hue1, 120.f);
        ImU32 c0 = HsvToRgb(hue0, 0.85f, 0.9f);
        ImU32 c1 = HsvToRgb(hue1, 0.85f, 0.9f);
        dl->AddRectFilledMultiColor(
            ImVec2(x + i * segW, y), ImVec2(x + (i + 1) * segW, y + h),
            c0, c1, c1, c0);
    }
}

// ── Per-character Color Wave ────────────────────────────────────────────────
inline void DrawTextWave(ImDrawList* dl, ImFont* font, float fontSize,
                         float x, float y, const char* text, float waveSpeed = 3.f) {
    float time = (float)ImGui::GetTime();
    float curX = x;
    for (int i = 0; text[i]; ++i) {
        char ch[2] = { text[i], 0 };
        float phase = time * waveSpeed + i * 0.4f;
        float hue = 190.f + sinf(phase) * 40.f; // oscillate around cyan (190)
        ImU32 col = HsvToRgb(hue, 0.7f, 1.f);
        float yOff = sinf(phase * 1.5f) * 1.5f;
        dl->AddText(font, fontSize, ImVec2(curX, y + yOff), col, ch);
        curX += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, ch).x;
    }
}

// ── Glow Rect ───────────────────────────────────────────────────────────────
inline void DrawGlowRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color,
                         float rounding = 0.f, float spread = 4.f) {
    for (int i = 3; i >= 1; --i) {
        float f = (float)i;
        float alpha = 0.07f * (4 - i);
        ImU32 gc = (color & 0x00FFFFFF) | ((ImU32)(alpha * 255.f) << IM_COL32_A_SHIFT);
        dl->AddRect(ImVec2(min.x - f * spread, min.y - f * spread),
                    ImVec2(max.x + f * spread, max.y + f * spread),
                    gc, rounding + f, 0, 1.f + f * 0.5f);
    }
}

// ── Scanning Line Effect ────────────────────────────────────────────────────
inline void DrawScanLine(ImDrawList* dl, float x, float y, float w, float h,
                         float speed = 0.4f) {
    float time = (float)ImGui::GetTime();
    float t = fmodf(time * speed, 1.f);
    float lineY = y + t * h;
    float alpha = 0.08f;
    ImU32 col = IM_COL32(0, 180, 216, (int)(alpha * 255.f));
    dl->AddRectFilledMultiColor(
        ImVec2(x, lineY - 15.f), ImVec2(x + w, lineY),
        IM_COL32(0, 180, 216, 0), IM_COL32(0, 180, 216, 0), col, col);
    dl->AddRectFilledMultiColor(
        ImVec2(x, lineY), ImVec2(x + w, lineY + 15.f),
        col, col, IM_COL32(0, 180, 216, 0), IM_COL32(0, 180, 216, 0));
}

} // namespace menu_anim
