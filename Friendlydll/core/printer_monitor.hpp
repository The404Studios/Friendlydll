#pragma once
#include "../includes.hpp"
#include <unordered_map>
#include <deque>

namespace printer_monitor {

    inline bool enabled = false;
    inline bool show_panel = true;

    struct PrinterData {
        int money = 0;
        int prevMoney = 0;
        float firstSeen = 0.f;
        float lastUpdate = 0.f;
        float incomeRate = 0.f; // $/sec estimated
        char label[64]{};
        char owner[32]{};
        Vector pos{};
        bool valid = false;
    };

    inline std::unordered_map<int, PrinterData> g_printers; // entity index -> data

    inline void Update(float curtime) {
        if (!enabled) return;

        auto& cache = config::EntRead();
        int cnt = config::EntReadCount();

        for (auto& [idx, pd] : g_printers) pd.valid = false;

        for (int i = 0; i < cnt; ++i) {
            const auto& ent = cache[i];
            if (!ent.valid || ent.type != 0) continue; // type 0 = printer

            auto& pd = g_printers[i];
            if (pd.firstSeen == 0.f) pd.firstSeen = curtime;
            pd.valid = true;
            pd.pos = ent.pos;
            strncpy_s(pd.label, ent.label, 63);
            strncpy_s(pd.owner, ent.owner, 31);

            if (ent.money != pd.money) {
                pd.prevMoney = pd.money;
                pd.money = ent.money;
                float dt = curtime - pd.lastUpdate;
                if (dt > 0.1f && pd.prevMoney > 0) {
                    float rate = static_cast<float>(pd.money - pd.prevMoney) / dt;
                    pd.incomeRate = pd.incomeRate * 0.7f + rate * 0.3f; // EMA
                }
                pd.lastUpdate = curtime;
            }
        }

        // prune stale
        for (auto it = g_printers.begin(); it != g_printers.end(); ) {
            if (!it->second.valid && (curtime - it->second.lastUpdate) > 30.f)
                it = g_printers.erase(it);
            else
                ++it;
        }
    }

    inline void DrawPanel(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH, float curtime) {
        if (!enabled || !show_panel || g_printers.empty()) return;

        float panelW = 280.f;
        float lineH = fontSize * 0.85f + 3.f;
        int count = 0;
        for (const auto& [idx, pd] : g_printers)
            if (pd.valid) ++count;
        if (count == 0) return;

        float panelH = 30.f + count * lineH + 10.f;
        float px = 10.f;
        float py = static_cast<float>(screenH) * 0.5f - panelH * 0.5f;

        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(10, 10, 16, 200), 5.f);
        dl->AddRect(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(0, 180, 216, 120), 5.f);

        dl->AddText(font, fontSize, ImVec2(px + 8.f, py + 4.f), IM_COL32(0, 220, 80, 255), "PRINTER MONITOR");

        float y = py + 28.f;
        for (const auto& [idx, pd] : g_printers) {
            if (!pd.valid) continue;

            float age = curtime - pd.firstSeen;
            int minutes = static_cast<int>(age) / 60;
            int secs = static_cast<int>(age) % 60;

            char buf[128];
            if (pd.incomeRate > 0.f) {
                float perMin = pd.incomeRate * 60.f;
                snprintf(buf, sizeof(buf), "$%d  +$%.0f/m  %d:%02d  %s",
                         pd.money, perMin, minutes, secs, pd.owner);
            } else {
                snprintf(buf, sizeof(buf), "$%d  %d:%02d  %s",
                         pd.money, minutes, secs, pd.owner);
            }

            ImU32 col = IM_COL32(0, 200, 80, 220);
            if (pd.money > 5000) col = IM_COL32(255, 200, 0, 220);
            if (pd.money > 10000) col = IM_COL32(255, 80, 80, 220);

            dl->AddText(font, fontSize * 0.8f, ImVec2(px + 10.f, y), col, buf);
            y += lineH;
        }
    }

} // namespace printer_monitor
