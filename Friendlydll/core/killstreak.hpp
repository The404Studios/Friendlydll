#pragma once
#include "../includes.hpp"

namespace killstreak {

    inline int g_current = 0;
    inline int g_best = 0;
    inline float g_lastKillTime = 0.f;
    inline float g_displayUntil = 0.f;
    inline char g_lastVictim[32] = "";

    inline const char* GetStreakName(int streak) {
        if (streak >= 20) return "UNSTOPPABLE";
        if (streak >= 15) return "GODLIKE";
        if (streak >= 10) return "RAMPAGE";
        if (streak >= 7) return "DOMINATING";
        if (streak >= 5) return "KILLING SPREE";
        if (streak >= 3) return "TRIPLE KILL";
        if (streak >= 2) return "DOUBLE KILL";
        return nullptr;
    }

    inline ImU32 GetStreakColor(int streak) {
        if (streak >= 15) return IM_COL32(255, 0, 0, 255);
        if (streak >= 10) return IM_COL32(255, 100, 0, 255);
        if (streak >= 7) return IM_COL32(255, 180, 0, 255);
        if (streak >= 5) return IM_COL32(255, 255, 0, 255);
        if (streak >= 3) return IM_COL32(100, 255, 100, 255);
        return IM_COL32(200, 200, 200, 255);
    }

    inline void OnKill(const char* victim, float curtime) {
        if (curtime - g_lastKillTime > 10.f) g_current = 0;
        g_current++;
        g_lastKillTime = curtime;
        g_displayUntil = curtime + 3.f;
        if (g_current > g_best) g_best = g_current;
        strncpy_s(g_lastVictim, victim, 31);
    }

    inline void OnDeath() {
        g_current = 0;
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH, float curtime) {
        if (!config::killstreak_enabled) return;

        // Persistent streak counter (top right)
        if (g_current >= 2) {
            char buf[48];
            snprintf(buf, sizeof(buf), "STREAK: %d", g_current);
            ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, buf);
            float rx = static_cast<float>(screenW) - sz.x - 20.f;
            float ry = 60.f;
            dl->AddRectFilled(ImVec2(rx - 8.f, ry - 3.f), ImVec2(rx + sz.x + 8.f, ry + sz.y + 3.f),
                              IM_COL32(0, 0, 0, 140), 4.f);
            dl->AddText(font, fontSize, ImVec2(rx, ry), GetStreakColor(g_current), buf);
        }

        // Center screen announcement
        if (curtime < g_displayUntil) {
            const char* name = GetStreakName(g_current);
            if (name) {
                float titleSize = fontSize * 2.f;
                float fadeAlpha = (g_displayUntil - curtime) / 3.f;
                if (fadeAlpha > 1.f) fadeAlpha = 1.f;
                int alpha = static_cast<int>(fadeAlpha * 255.f);

                ImVec2 sz = font->CalcTextSizeA(titleSize, FLT_MAX, 0.f, name);
                float cx = (static_cast<float>(screenW) - sz.x) * 0.5f;
                float cy = static_cast<float>(screenH) * 0.3f;

                float pulse = sinf(curtime * 4.f) * 0.15f + 0.85f;
                ImU32 col = GetStreakColor(g_current);
                col = (col & 0x00FFFFFF) | (static_cast<ImU32>(alpha * pulse) << 24);

                dl->AddText(font, titleSize, ImVec2(cx + 2.f, cy + 2.f),
                            IM_COL32(0, 0, 0, alpha / 2), name);
                dl->AddText(font, titleSize, ImVec2(cx, cy), col, name);

                // Victim name below
                char victimBuf[48];
                snprintf(victimBuf, sizeof(victimBuf), "Killed: %s", g_lastVictim);
                ImVec2 vs = font->CalcTextSizeA(fontSize * 0.9f, FLT_MAX, 0.f, victimBuf);
                float vx = (static_cast<float>(screenW) - vs.x) * 0.5f;
                dl->AddText(font, fontSize * 0.9f, ImVec2(vx, cy + sz.y + 4.f),
                            IM_COL32(200, 200, 200, alpha), victimBuf);
            }
        }

        // Best streak indicator
        if (g_best >= 3) {
            char bestBuf[32];
            snprintf(bestBuf, sizeof(bestBuf), "Best: %d", g_best);
            dl->AddText(font, fontSize * 0.8f,
                ImVec2(static_cast<float>(screenW) - 80.f, 80.f + fontSize),
                IM_COL32(150, 150, 150, 120), bestBuf);
        }
    }

} // namespace killstreak
