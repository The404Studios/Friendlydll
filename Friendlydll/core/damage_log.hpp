#pragma once
#include "../includes.hpp"
#include <deque>

namespace damage_log {

    inline bool enabled = false;
    inline bool show_indicators = true;
    inline float indicator_duration = 2.0f;

    struct DamageEvent {
        int amount;
        Vector worldPos;
        float time;
        bool dealt; // true=we dealt, false=we received
    };

    inline std::deque<DamageEvent> g_events;
    inline int g_lastHealth = 100;
    inline int g_totalDealt = 0;
    inline int g_totalReceived = 0;

    inline void Reset() {
        g_events.clear();
        g_lastHealth = 100;
        g_totalDealt = 0;
        g_totalReceived = 0;
    }

    inline void Update(C_BasePlayer* local, float curtime) {
        if (!enabled || !local) return;

        int hp = local->GetHealth();
        if (hp < g_lastHealth && hp > 0) {
            int dmg = g_lastHealth - hp;
            g_totalReceived += dmg;
            Vector pos = local->GetAbsOrigin();
            pos.z += 60.f;
            pos.x += static_cast<float>((rand() % 40) - 20);
            pos.y += static_cast<float>((rand() % 40) - 20);
            g_events.push_back({ dmg, pos, curtime, false });
        }
        g_lastHealth = hp;

        while (!g_events.empty() && (curtime - g_events.front().time) > indicator_duration + 1.f)
            g_events.pop_front();
    }

    inline void RecordDamageDealt(int amount, const Vector& pos, float curtime) {
        if (!enabled) return;
        g_totalDealt += amount;
        Vector p = pos;
        p.z += 60.f;
        p.x += static_cast<float>((rand() % 40) - 20);
        p.y += static_cast<float>((rand() % 40) - 20);
        g_events.push_back({ amount, p, curtime, true });
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH, float curtime) {
        if (!enabled || !show_indicators) return;

        for (const auto& ev : g_events) {
            float age = curtime - ev.time;
            if (age > indicator_duration) continue;

            float alpha = 1.f - (age / indicator_duration);
            float rise = age * 30.f;

            Vector drawPos = ev.worldPos;
            drawPos.z += rise;

            float sx, sy;
            if (!config::WorldToScreen(drawPos, sx, sy)) continue;

            char buf[16];
            snprintf(buf, sizeof(buf), "%s%d", ev.dealt ? "+" : "-", ev.amount);

            int a = static_cast<int>(alpha * 255.f);
            ImU32 col = ev.dealt ? IM_COL32(255, 80, 80, a) : IM_COL32(80, 255, 80, a);
            float sz = fontSize * (ev.dealt ? 1.2f : 1.0f);

            ImVec2 ts = font->CalcTextSizeA(sz, FLT_MAX, 0.f, buf);
            dl->AddText(font, sz, ImVec2(sx - ts.x * 0.5f + 1.f, sy + 1.f), IM_COL32(0, 0, 0, a / 2), buf);
            dl->AddText(font, sz, ImVec2(sx - ts.x * 0.5f, sy), col, buf);
        }

        // damage totals panel (bottom-right)
        if (g_totalDealt > 0 || g_totalReceived > 0) {
            float px = static_cast<float>(screenW) - 160.f;
            float py = static_cast<float>(screenH) - 60.f;

            dl->AddRectFilled(ImVec2(px, py), ImVec2(px + 150.f, py + 50.f), IM_COL32(0, 0, 0, 160), 4.f);

            char dealt[32], recv[32];
            snprintf(dealt, sizeof(dealt), "Dealt: %d", g_totalDealt);
            snprintf(recv, sizeof(recv), "Recv: %d", g_totalReceived);
            dl->AddText(font, fontSize * 0.8f, ImVec2(px + 8.f, py + 4.f), IM_COL32(255, 80, 80, 220), dealt);
            dl->AddText(font, fontSize * 0.8f, ImVec2(px + 8.f, py + 22.f), IM_COL32(80, 255, 80, 220), recv);
        }
    }

} // namespace damage_log
