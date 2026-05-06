#pragma once
#include "../includes.hpp"
#include <unordered_map>
#include <algorithm>

namespace heatmap {

    inline bool enabled = false;
    inline float grid_size = 200.f;
    inline float opacity = 0.3f;
    inline int min_hits = 2;

    struct GridKey {
        int x, y;
        bool operator==(const GridKey& o) const { return x == o.x && y == o.y; }
    };

    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 16);
        }
    };

    inline std::unordered_map<GridKey, float, GridKeyHash> g_grid;
    inline float g_maxHeat = 1.f;
    inline float g_floorZ = 0.f;
    inline int g_updateThrottle = 0;

    // Decay settings
    inline float g_decayFactor = 0.9f;
    inline float g_decayInterval = 30.f; // seconds between decay passes
    inline float g_lastDecayTime = 0.f;

    inline ImU32 HeatColor(float ratio) {
        // green(0.0) -> yellow(0.5) -> red(1.0)
        ratio = (ratio < 0.f) ? 0.f : (ratio > 1.f) ? 1.f : ratio;

        float r, g, b;
        if (ratio < 0.5f) {
            float t = ratio * 2.f;       // 0..1 over green->yellow
            r = t;
            g = 1.f;
            b = 0.f;
        } else {
            float t = (ratio - 0.5f) * 2.f; // 0..1 over yellow->red
            r = 1.f;
            g = 1.f - t;
            b = 0.f;
        }

        int a = static_cast<int>(opacity * 255.f);
        return IM_COL32(
            static_cast<int>(r * 255.f),
            static_cast<int>(g * 255.f),
            static_cast<int>(b * 255.f),
            a
        );
    }

    inline void Update() {
        if (!enabled) return;

        // throttle: only accumulate every ~10 ticks
        if (++g_updateThrottle < 10) return;
        g_updateThrottle = 0;

        const auto& bones = config::BoneRead();

        // Use local player's Z as floor reference
        int localIdx = interfaces::engine->GetLocalPlayer();
        int ri = config::g_boneReadIdx.load(std::memory_order_acquire);
        if (localIdx > 0 && localIdx < 128 && config::g_boneBuffers[ri][localIdx].valid) {
            g_floorZ = config::g_boneBuffers[ri][localIdx].absOrigin.z - 64.f;
        }

        for (int i = 1; i < 128; ++i) {
            const auto& rec = bones[i];
            if (!rec.valid || rec.dormant) continue;

            float wx = rec.absOrigin.x;
            float wy = rec.absOrigin.y;

            int gx = static_cast<int>(std::floor(wx / grid_size));
            int gy = static_cast<int>(std::floor(wy / grid_size));

            GridKey key{ gx, gy };
            float& count = g_grid[key];
            count += 1.f;

            if (count > g_maxHeat)
                g_maxHeat = count;
        }

        // Decay pass: every g_decayInterval seconds, multiply all cells by decay factor
        float curTime = interfaces::globalVars->curtime;
        if (curTime - g_lastDecayTime >= g_decayInterval) {
            g_lastDecayTime = curTime;
            g_maxHeat = 1.f;
            for (auto it = g_grid.begin(); it != g_grid.end(); ) {
                it->second *= g_decayFactor;
                if (it->second < 0.5f) {
                    it = g_grid.erase(it);
                } else {
                    if (it->second > g_maxHeat)
                        g_maxHeat = it->second;
                    ++it;
                }
            }
        }
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize,
                     int screenW, int screenH)
    {
        if (!enabled) return;
        if (g_grid.empty()) return;

        const float gs = grid_size;
        const float fz = g_floorZ;
        const float maxH = g_maxHeat > 1.f ? g_maxHeat : 1.f;

        for (const auto& [key, count] : g_grid) {
            if (count < min_hits) continue;

            // world-space corners of this grid cell on the floor plane
            float x0 = static_cast<float>(key.x) * gs;
            float y0 = static_cast<float>(key.y) * gs;
            float x1 = x0 + gs;
            float y1 = y0 + gs;

            Vector corners[4] = {
                { x0, y0, fz },
                { x1, y0, fz },
                { x1, y1, fz },
                { x0, y1, fz }
            };

            ImVec2 screen[4];
            int onScreen = 0;

            for (int c = 0; c < 4; ++c) {
                if (config::WorldToScreen(corners[c], screen[c].x, screen[c].y))
                    onScreen++;
            }

            // need at least 2 corners visible to draw
            if (onScreen < 2) continue;

            float ratio = count / maxH;
            ImU32 col = HeatColor(ratio);

            dl->AddConvexPolyFilled(screen, 4, col);
        }
    }

    inline void Clear() {
        g_grid.clear();
        g_maxHeat = 1.f;
        g_floorZ = 0.f;
        g_lastDecayTime = 0.f;
    }

} // namespace heatmap
