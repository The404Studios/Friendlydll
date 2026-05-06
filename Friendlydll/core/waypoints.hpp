#pragma once
#include "../includes.hpp"
#include <vector>
#include <string>

namespace waypoints {

    inline bool enabled = false;

    struct Waypoint {
        Vector pos;
        char label[32];
        float color[3];
    };

    inline std::vector<Waypoint> g_waypoints;
    inline int g_selectedColor = 0;

    inline void Add(const Vector& pos, const char* label) {
        Waypoint wp;
        wp.pos = pos;
        strncpy_s(wp.label, label, 31);
        static const float presets[][3] = {
            {1.f, 0.3f, 0.3f}, {0.3f, 1.f, 0.3f}, {0.3f, 0.6f, 1.f},
            {1.f, 1.f, 0.3f}, {1.f, 0.3f, 1.f}, {0.3f, 1.f, 1.f}
        };
        int ci = g_selectedColor % 6;
        wp.color[0] = presets[ci][0];
        wp.color[1] = presets[ci][1];
        wp.color[2] = presets[ci][2];
        g_waypoints.push_back(wp);
    }

    inline void Remove(int idx) {
        if (idx >= 0 && idx < static_cast<int>(g_waypoints.size()))
            g_waypoints.erase(g_waypoints.begin() + idx);
    }

    inline void Clear() { g_waypoints.clear(); }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled || g_waypoints.empty()) return;

        for (int i = 0; i < static_cast<int>(g_waypoints.size()); ++i) {
            const auto& wp = g_waypoints[i];
            float sx, sy;
            if (!config::WorldToScreen(wp.pos, sx, sy)) continue;

            ImU32 col = ImColor(wp.color[0], wp.color[1], wp.color[2]);
            ImU32 bg = IM_COL32(0, 0, 0, 160);

            dl->AddCircleFilled(ImVec2(sx, sy), 6.f, bg);
            dl->AddCircleFilled(ImVec2(sx, sy), 4.f, col);

            dl->AddLine(ImVec2(sx, sy + 6.f), ImVec2(sx, sy + 20.f), col, 1.5f);

            char buf[48];
            snprintf(buf, sizeof(buf), "%s", wp.label);
            ImVec2 ts = font->CalcTextSizeA(fontSize * 0.8f, FLT_MAX, 0.f, buf);
            float tx = sx - ts.x * 0.5f;
            float ty = sy + 22.f;

            dl->AddRectFilled(ImVec2(tx - 3.f, ty - 1.f), ImVec2(tx + ts.x + 3.f, ty + ts.y + 1.f), bg, 3.f);
            dl->AddText(font, fontSize * 0.8f, ImVec2(tx, ty), col, buf);

            // Distance from local player (source units -> ~meters)
            {
                int ri = config::g_boneReadIdx.load(std::memory_order_acquire);
                int localIdx = interfaces::engine->GetLocalPlayer();
                Vector lpPos = config::g_boneBuffers[ri][localIdx].absOrigin;
                Vector diff = { wp.pos.x - lpPos.x, wp.pos.y - lpPos.y, wp.pos.z - lpPos.z };
                float dist = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                char distBuf[16];
                snprintf(distBuf, sizeof(distBuf), "%.0fm", dist * 0.01905f);
                ImVec2 ds = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, distBuf);
                float dtx = sx - ds.x * 0.5f;
                float dty = ty + ts.y + 4.f;
                dl->AddText(font, fontSize * 0.7f, ImVec2(dtx, dty), IM_COL32(200, 200, 200, 180), distBuf);
            }
        }
    }

} // namespace waypoints
