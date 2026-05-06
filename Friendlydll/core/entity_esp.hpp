#pragma once
#include "../includes.hpp"
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace entity_timer {
    inline std::unordered_map<int, double> g_firstSeen;
    inline double g_lastUpdateTime = 0.0;

    inline float GetElapsed(int entIndex, double curtime) {
        auto it = g_firstSeen.find(entIndex);
        if (it == g_firstSeen.end()) {
            g_firstSeen[entIndex] = curtime;
            return 0.f;
        }
        return static_cast<float>(curtime - it->second);
    }

    inline void PruneStale(double curtime, const config::EntRecord* ents, int count) {
        if (curtime - g_lastUpdateTime < 5.0) return;
        g_lastUpdateTime = curtime;
        std::unordered_set<int> activeIds;
        for (int i = 0; i < count; ++i)
            if (ents[i].valid) activeIds.insert(ents[i].entIndex);
        for (auto it = g_firstSeen.begin(); it != g_firstSeen.end(); ) {
            if (activeIds.find(it->first) == activeIds.end())
                it = g_firstSeen.erase(it);
            else
                ++it;
        }
    }
}

inline void DrawEntityESP(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH, double curtime = 0.0)
{
    if (!config::entity_esp) return;

    auto& entsRef = config::EntRead();
    int countRef = config::EntReadCount();
    entity_timer::PruneStale(curtime, entsRef, countRef);

    const float pad     = 4.f;
    const float lineH   = fontSize + 2.f;
    const ImU32 bgCol   = IM_COL32(0, 0, 0, 190);
    const ImU32 shadowCol = IM_COL32(0, 0, 0, 220);

    auto drawEntLabel = [&](float cx, float y, const char* text, ImU32 col) -> float {
        ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        float lx = cx - sz.x * 0.5f;
        float lh = fontSize + 2.f;
        dl->AddRectFilled(ImVec2(lx - 4, y - 1), ImVec2(lx + sz.x + 4, y + lh + 1), IM_COL32(0, 0, 0, 100), 2.f);
        dl->AddText(font, fontSize, ImVec2(lx + 1, y + 1), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(font, fontSize, ImVec2(lx, y), col, text);
        return y + lh + 3.f;
    };

    auto& ents = config::EntRead();
    int count  = config::EntReadCount();

    for (int i = 0; i < count; ++i)
    {
        const auto& ent = ents[i];
        if (!ent.valid) continue;

        // distance filter
        if (ent.distance > config::esp_max_dist) continue;

        // type filter
        switch (ent.type) {
            case 0: if (!config::entity_esp_printers)   continue; break;
            case 1: if (!config::entity_esp_shipments)   continue; break;
            case 2: if (!config::entity_esp_drugs)       continue; break;
            case 3: if (!config::entity_esp_doors)       continue; break;
            case 4: if (!config::entity_esp_weapons)     continue; break;
            case 5: if (!config::entity_esp_money)       continue; break;
            case 6: if (!config::entity_esp_vehicles)    continue; break;
            default: continue;
        }

        // project to screen
        float sx, sy;
        if (!config::WorldToScreen(ent.pos, sx, sy)) continue;

        // offscreen cull
        if (sx < -50.f || sx > screenW + 50.f || sy < -50.f || sy > screenH + 50.f)
            continue;

        // choose color by type
        ImU32 color;
        switch (ent.type) {
            case 0: // printer — green (from config)
                color = ImColor(config::entity_esp_color_printer[0],
                                config::entity_esp_color_printer[1],
                                config::entity_esp_color_printer[2]);
                break;
            case 1: // shipment — orange (from config)
                color = ImColor(config::entity_esp_color_shipment[0],
                                config::entity_esp_color_shipment[1],
                                config::entity_esp_color_shipment[2]);
                break;
            case 2: // drug — purple (from config)
                color = ImColor(config::entity_esp_color_drug[0],
                                config::entity_esp_color_drug[1],
                                config::entity_esp_color_drug[2]);
                break;
            case 3: // door
                color = ImColor(config::entity_esp_color_door[0],
                                config::entity_esp_color_door[1],
                                config::entity_esp_color_door[2]);
                break;
            case 4: // weapon
                color = ImColor(config::entity_esp_color_weapon[0],
                                config::entity_esp_color_weapon[1],
                                config::entity_esp_color_weapon[2]);
                break;
            case 5: // money
                color = ImColor(config::entity_esp_color_money[0],
                                config::entity_esp_color_money[1],
                                config::entity_esp_color_money[2]);
                break;
            case 6: // vehicle
                color = ImColor(config::entity_esp_color_vehicle[0],
                                config::entity_esp_color_vehicle[1],
                                config::entity_esp_color_vehicle[2]);
                break;
            default:
                color = IM_COL32(255, 255, 255, 255);
                break;
        }

        // diamond marker (8px)
        dl->AddLine(ImVec2(sx, sy - 6), ImVec2(sx + 6, sy), color);
        dl->AddLine(ImVec2(sx + 6, sy), ImVec2(sx, sy + 6), color);
        dl->AddLine(ImVec2(sx, sy + 6), ImVec2(sx - 6, sy), color);
        dl->AddLine(ImVec2(sx - 6, sy), ImVec2(sx, sy - 6), color);

        // labels below the diamond
        float labelY = sy + 10.f;

        // entity label (class/name)
        if (ent.label[0])
            labelY = drawEntLabel(sx, labelY, ent.label, color);

        // owner name (dim)
        if (ent.owner[0])
            labelY = drawEntLabel(sx, labelY, ent.owner, IM_COL32(160, 160, 160, 220));

        // money
        if (ent.money > 0) {
            char moneyBuf[32];
            snprintf(moneyBuf, sizeof(moneyBuf), "$%d", ent.money);
            labelY = drawEntLabel(sx, labelY, moneyBuf, IM_COL32(0, 255, 0, 255));
        }

        // shipment countdown timer
        if (ent.type == 1 && curtime > 0.f && ent.entIndex > 0) {
            float elapsed = entity_timer::GetElapsed(ent.entIndex, curtime);
            float remaining = config::entity_esp_timer_max - elapsed;
            if (remaining < 0.f) remaining = 0.f;
            int mins = (int)(remaining / 60.f);
            int secs = (int)fmodf(remaining, 60.f);
            char timerBuf[32];
            snprintf(timerBuf, sizeof(timerBuf), "%d:%02d", mins, secs);
            ImU32 timerCol = remaining < 60.f ? IM_COL32(255, 80, 80, 255) : IM_COL32(255, 220, 80, 255);
            labelY = drawEntLabel(sx, labelY, timerBuf, timerCol);
        }

        // entity health bar
        if (config::entity_esp_health_bars && ent.maxHealth > 0 && ent.health > 0) {
            float barW = 50.f;
            float barH = 4.f;
            float bx = sx - barW * 0.5f;
            float frac = std::clamp((float)ent.health / (float)ent.maxHealth, 0.f, 1.f);
            ImU32 hpCol = frac > 0.5f ? IM_COL32(60, 255, 60, 220)
                        : frac > 0.25f ? IM_COL32(255, 200, 40, 220)
                        : IM_COL32(255, 50, 50, 220);
            dl->AddRectFilled(ImVec2(bx - 1, labelY - 1), ImVec2(bx + barW + 1, labelY + barH + 1),
                              IM_COL32(0, 0, 0, 160), 2.f);
            dl->AddRectFilled(ImVec2(bx, labelY), ImVec2(bx + barW * frac, labelY + barH), hpCol, 1.f);
            labelY += barH + 4.f;
        }

        // distance in meters
        char distBuf[32];
        snprintf(distBuf, sizeof(distBuf), "%.0fm", ent.distance * 0.01905f);
        drawEntLabel(sx, labelY, distBuf, IM_COL32(120, 120, 120, 255));
    }
}
