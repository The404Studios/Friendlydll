#pragma once
#include "../includes.hpp"
#include <cmath>

namespace hud {

// ── DrawRadar ────────────────────────────────────────────────────────────────
// Minimap radar in the top-right corner showing nearby players as colored dots.
inline void DrawRadar(ImDrawList* dl, ImFont* font, float fontSize,
                      int screenW, int screenH,
                      const Vector& localPos, float localYaw)
{
    if (!config::minimap) return;

    const float cx     = static_cast<float>(screenW) - 120.f;
    const float cy     = 120.f;
    const float radius = 90.f;

    // background + border
    dl->AddCircleFilled(ImVec2(cx, cy), radius, IM_COL32(0, 0, 0, 160), 64);
    dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(0, 180, 216, 180), 64, 1.5f);

    // crosshair
    const ImU32 crossCol = IM_COL32(255, 255, 255, 40);
    dl->AddLine(ImVec2(cx - radius, cy), ImVec2(cx + radius, cy), crossCol);
    dl->AddLine(ImVec2(cx, cy - radius), ImVec2(cx, cy + radius), crossCol);

    // local player dot
    dl->AddCircleFilled(ImVec2(cx, cy), 3.f, IM_COL32(255, 255, 255, 255));

    const float yawRad = localYaw * 3.14159f / 180.f;
    const float cosY   = cosf(-yawRad);
    const float sinY   = sinf(-yawRad);
    const float scale  = radius / config::minimap_zoom;

    auto& cache = config::BoneRead();
    for (int i = 1; i < 128; ++i) {
        if (!cache[i].valid) continue;

        float dx = cache[i].absOrigin.x - localPos.x;
        float dy = cache[i].absOrigin.y - localPos.y;

        // rotate into view-relative space
        float rx = dx * cosY - dy * sinY;
        float ry = dx * sinY + dy * cosY;

        rx *= scale;
        ry *= scale;

        // clamp to circle
        float dist = sqrtf(rx * rx + ry * ry);
        if (dist > radius - 4.f) {
            float f = (radius - 4.f) / dist;
            rx *= f;
            ry *= f;
        }

        ImU32 dotCol = IM_COL32(0, 220, 80, 255);   // green: normal
        if (cache[i].isAdmin || cache[i].isSuperAdmin)
            dotCol = IM_COL32(255, 50, 50, 255);     // red: admin
        else if (cache[i].isWanted)
            dotCol = IM_COL32(255, 165, 0, 255);     // orange: wanted

        dl->AddCircleFilled(ImVec2(cx + rx, cy - ry), 4.f, dotCol);
    }

    // label
    const char* label = "RADAR";
    ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, label);
    float lx = cx - sz.x * 0.5f;
    float ly = cy - radius - sz.y - 4.f;
    dl->AddText(font, fontSize, ImVec2(lx + 1.f, ly + 1.f), IM_COL32(0, 0, 0, 180), label);
    dl->AddText(font, fontSize, ImVec2(lx, ly), IM_COL32(0, 180, 216, 220), label);
}

// ── DrawFOVCircle ────────────────────────────────────────────────────────────
// Draws an aimbot FOV circle at screen center.
inline void DrawFOVCircle(ImDrawList* dl, int screenW, int screenH)
{
    if (!config::fov_circle || !config::aimbot) return;

    float radius = config::aimbot_fov * static_cast<float>(screenH) / 180.f;
    float cx = static_cast<float>(screenW) * 0.5f;
    float cy = static_cast<float>(screenH) * 0.5f;

    ImU32 col = IM_COL32(
        static_cast<int>(config::fov_circle_color[0] * 255.f),
        static_cast<int>(config::fov_circle_color[1] * 255.f),
        static_cast<int>(config::fov_circle_color[2] * 255.f),
        100);

    dl->AddCircle(ImVec2(cx, cy), radius, col, 64, 1.5f);
}

// ── DrawSpectatorAlert ───────────────────────────────────────────────────────
// Pulsing red warning when being spectated.
inline void DrawSpectatorAlert(ImDrawList* dl, ImFont* font, float fontSize,
                               int screenW, int screenH)
{
    if (!config::spectator_alert) return;
    if (!config::g_beingSpectated.load(std::memory_order_relaxed)) return;

    float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 4.f) * 0.3f + 0.7f;
    int alpha   = static_cast<int>(pulse * 255.f);

    const float cx = static_cast<float>(screenW) * 0.5f;
    const float pad = 6.f;

    // main warning text
    const char* warn = "!! SPECTATED !!";
    ImVec2 warnSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, warn);
    float warnX = cx - warnSz.x * 0.5f;
    float warnY = 30.f;

    // spectator count text
    int count = config::g_spectatorCount.load(std::memory_order_relaxed);
    char countBuf[48];
    snprintf(countBuf, sizeof(countBuf), "Spectators: %d", count);
    ImVec2 countSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, countBuf);
    float countX = cx - countSz.x * 0.5f;
    float countY = warnY + warnSz.y + 4.f;

    // background rect behind both lines
    float bgLeft  = (warnSz.x > countSz.x ? warnX : countX) - pad;
    float bgRight = (warnSz.x > countSz.x ? warnX + warnSz.x : countX + countSz.x) + pad;
    float bgTop   = warnY - 2.f;
    float bgBot   = countY + countSz.y + 2.f;
    dl->AddRectFilled(ImVec2(bgLeft, bgTop), ImVec2(bgRight, bgBot),
                      IM_COL32(0, 0, 0, 180), 4.f);

    // draw warning text
    dl->AddText(font, fontSize, ImVec2(warnX, warnY),
                IM_COL32(255, 30, 30, alpha), warn);

    // draw count
    dl->AddText(font, fontSize, ImVec2(countX, countY),
                IM_COL32(255, 200, 200, alpha), countBuf);
}

// ── DrawPlayerIntel ──────────────────────────────────────────────────────────
// Per-player intel badges (admin, money, wanted) drawn below ESP.
// Returns the Y position after the last line drawn.
inline float DrawPlayerIntel(ImDrawList* dl, ImFont* font, float fontSize,
                             float cx, float y,
                             const config::BoneRecord& rec)
{
    const float pad    = 4.f;
    const float lineH  = fontSize + 2.f;
    const ImU32 shadow = IM_COL32(0, 0, 0, 220);

    auto drawBadge = [&](const char* text, ImU32 textCol, ImU32 bgCol) {
        ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        float lx = cx - sz.x * 0.5f;
        dl->AddRectFilled(ImVec2(lx - pad, y - 1.f),
                          ImVec2(lx + sz.x + pad, y + lineH + 1.f),
                          bgCol, 2.f);
        dl->AddText(font, fontSize, ImVec2(lx + 1.f, y + 1.f), shadow, text);
        dl->AddText(font, fontSize, ImVec2(lx, y), textCol, text);
        y += lineH + 3.f;
    };

    // admin badge
    if (rec.isSuperAdmin) {
        drawBadge("S.ADMIN", IM_COL32(255, 160, 0, 255), IM_COL32(80, 40, 0, 200));
    } else if (rec.isAdmin) {
        drawBadge("ADMIN", IM_COL32(255, 50, 50, 255), IM_COL32(80, 0, 0, 200));
    }

    // money tracker
    if (config::money_tracker && rec.money > 0) {
        // format money with commas
        char raw[32];
        snprintf(raw, sizeof(raw), "%d", rec.money);
        int rawLen = static_cast<int>(strlen(raw));

        char formatted[48] = "$";
        int fi = 1;
        for (int i = 0; i < rawLen; ++i) {
            int remaining = rawLen - i;
            if (i > 0 && remaining % 3 == 0)
                formatted[fi++] = ',';
            formatted[fi++] = raw[i];
        }
        formatted[fi] = '\0';

        drawBadge(formatted, IM_COL32(0, 220, 80, 255), IM_COL32(0, 40, 10, 200));
    }

    // wanted badge with pulsing alpha
    if (rec.isWanted) {
        float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 4.f) * 0.3f + 0.7f;
        int alpha   = static_cast<int>(pulse * 255.f);
        ImVec2 sz   = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, "WANTED");
        float lx    = cx - sz.x * 0.5f;
        dl->AddRectFilled(ImVec2(lx - pad, y - 1.f),
                          ImVec2(lx + sz.x + pad, y + lineH + 1.f),
                          IM_COL32(80, 0, 0, 180), 2.f);
        dl->AddText(font, fontSize, ImVec2(lx + 1.f, y + 1.f), shadow, "WANTED");
        dl->AddText(font, fontSize, ImVec2(lx, y),
                    IM_COL32(255, 30, 30, alpha), "WANTED");
        y += lineH + 3.f;
    }

    return y;
}

// These are in the hud namespace to avoid conflicts with config.hpp edits happening in parallel
inline bool velocity_graph = true;
inline bool spectator_list = true;

// ── DrawSpectatorList ────────────────────────────────────────────────────────
// Small panel listing all players currently spectating the local player.
// Uses bone cache data (observerMode > 0 && observerTarget == localIdx).
inline void DrawSpectatorList(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH, int localIdx)
{
    if (!config::spectator_alert) return;
    int specCount = config::g_spectatorCount.load(std::memory_order_relaxed);
    if (specCount <= 0) return;

    const float pad = 8.f;
    const float lineH = fontSize + 2.f;
    float panelW = 200.f;
    float panelX = static_cast<float>(screenW) - panelW - 20.f;
    float panelY = 250.f; // below the radar

    auto& cache = config::BoneRead();

    // Collect spectator names
    struct SpecInfo { const char* name; int mode; };
    SpecInfo specs[16];
    int count = 0;

    for (int i = 0; i < 128 && count < 16; ++i) {
        const auto& rec = cache[i];
        if (!rec.valid) continue;
        if (rec.observerTarget != localIdx) continue;
        if (rec.observerMode <= 0) continue;
        specs[count].name = rec.rpName[0] ? rec.rpName : rec.name;
        specs[count].mode = rec.observerMode;
        ++count;
    }

    if (count == 0) return;

    float panelH = (lineH * (count + 1)) + pad * 2 + 4.f;

    // Background
    dl->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                      IM_COL32(10, 10, 14, 220), 6.f);
    dl->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                IM_COL32(255, 50, 50, 150), 6.f);

    // Header
    float textY = panelY + pad;
    const char* header = "SPECTATORS";
    ImVec2 hdrSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, header);
    float hdrX = panelX + (panelW - hdrSz.x) * 0.5f;
    dl->AddText(font, fontSize, ImVec2(hdrX, textY), IM_COL32(255, 60, 60, 255), header);
    textY += lineH + 2.f;

    // List each spectator
    const char* modeNames[] = { "", "1st", "3rd", "Free", "Chase", "Roam" };
    for (int s = 0; s < count; ++s) {
        char buf[96];
        const char* modeName = (specs[s].mode >= 1 && specs[s].mode <= 5) ? modeNames[specs[s].mode] : "?";
        snprintf(buf, sizeof(buf), "%s [%s]", specs[s].name, modeName);
        dl->AddText(font, fontSize, ImVec2(panelX + pad, textY),
                    IM_COL32(255, 200, 200, 230), buf);
        textY += lineH;
    }
}

// ── DrawVelocityGraph ────────────────────────────────────────────────────────
// Small line graph in the bottom-left showing velocity over time.
inline void DrawVelocityGraph(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH)
{
    constexpr int SAMPLES = 60;
    static float history[SAMPLES] = {};
    static int head = 0;

    // Push current velocity
    history[head] = config::currentvelocity;
    head = (head + 1) % SAMPLES;

    const float graphW = 160.f;
    const float graphH = 50.f;
    const float graphX = 20.f;
    const float graphY = static_cast<float>(screenH) - 200.f;

    // Background
    dl->AddRectFilled(ImVec2(graphX, graphY), ImVec2(graphX + graphW, graphY + graphH),
                      IM_COL32(0, 0, 0, 140), 4.f);
    dl->AddRect(ImVec2(graphX, graphY), ImVec2(graphX + graphW, graphY + graphH),
                IM_COL32(0, 180, 216, 80), 4.f);

    // Find max for scaling
    float maxVel = 1.f;
    for (int i = 0; i < SAMPLES; ++i)
        if (history[i] > maxVel) maxVel = history[i];

    // Draw line graph
    float stepX = graphW / (SAMPLES - 1);
    for (int i = 0; i < SAMPLES - 1; ++i) {
        int idx0 = (head + i) % SAMPLES;
        int idx1 = (head + i + 1) % SAMPLES;
        float y0 = graphY + graphH - (history[idx0] / maxVel) * (graphH - 4.f) - 2.f;
        float y1 = graphY + graphH - (history[idx1] / maxVel) * (graphH - 4.f) - 2.f;
        float x0 = graphX + i * stepX;
        float x1 = graphX + (i + 1) * stepX;
        dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 180, 216, 200), 1.5f);
    }

    // Current velocity label
    char velBuf[32];
    snprintf(velBuf, sizeof(velBuf), "%.0f u/s", config::currentvelocity);
    ImVec2 velSz = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.f, velBuf);
    dl->AddText(font, fontSize * 0.85f,
                ImVec2(graphX + graphW - velSz.x - 4.f, graphY + 2.f),
                IM_COL32(0, 220, 255, 200), velBuf);
}

} // namespace hud
