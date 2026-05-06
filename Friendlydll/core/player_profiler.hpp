#pragma once
#include "../includes.hpp"
#include <array>
#include <deque>
#include <cmath>

namespace player_profiler {

    inline bool enabled = false;
    inline bool show_paths = true;
    inline bool show_panel = false;

    struct PosSnapshot {
        Vector pos;
        float time;
    };

    struct PlayerProfile {
        std::deque<PosSnapshot> history;
        float avgSpeed = 0.f;
        float maxSpeed = 0.f;
        int directionChanges = 0;
        float predictability = 0.f; // 0=unpredictable, 1=very predictable
        bool isPatrolling = false;
        float lastAnalysis = 0.f;
        bool valid = false;
    };

    inline std::array<PlayerProfile, 128> g_profiles{};
    constexpr int MAX_HISTORY = 120; // ~2 seconds at 60fps

    inline void Update(float curtime) {
        if (!enabled) return;

        auto& cache = config::BoneRead();
        for (int i = 1; i < 128; ++i) {
            auto& prof = g_profiles[i];
            if (!cache[i].valid) {
                prof.valid = false;
                continue;
            }

            prof.valid = true;
            Vector pos = cache[i].absOrigin;
            prof.history.push_back({ pos, curtime });

            while (prof.history.size() > MAX_HISTORY)
                prof.history.pop_front();

            // Analyze every 30 ticks
            if (curtime - prof.lastAnalysis < 0.5f) continue;
            prof.lastAnalysis = curtime;

            if (prof.history.size() < 10) continue;

            // Calculate average/max speed
            float totalSpeed = 0.f;
            float maxSpd = 0.f;
            int dirChanges = 0;
            float prevAngle = 0.f;
            bool firstAngle = true;

            for (size_t j = 1; j < prof.history.size(); ++j) {
                float dx = prof.history[j].pos.x - prof.history[j-1].pos.x;
                float dy = prof.history[j].pos.y - prof.history[j-1].pos.y;
                float dt = prof.history[j].time - prof.history[j-1].time;
                if (dt < 0.001f) continue;

                float speed = sqrtf(dx*dx + dy*dy) / dt;
                totalSpeed += speed;
                if (speed > maxSpd) maxSpd = speed;

                float angle = atan2f(dy, dx);
                if (!firstAngle) {
                    float diff = fabsf(angle - prevAngle);
                    if (diff > 3.14159f) diff = 6.28318f - diff;
                    if (diff > 0.8f) dirChanges++;
                }
                prevAngle = angle;
                firstAngle = false;
            }

            size_t n = prof.history.size() - 1;
            prof.avgSpeed = n > 0 ? totalSpeed / static_cast<float>(n) : 0.f;
            prof.maxSpeed = maxSpd;
            prof.directionChanges = dirChanges;

            // Predictability: low direction changes + consistent speed = predictable
            float dirScore = 1.f - std::clamp(static_cast<float>(dirChanges) / 20.f, 0.f, 1.f);
            float speedVariance = (maxSpd > 0.f) ? (prof.avgSpeed / maxSpd) : 0.f;
            prof.predictability = dirScore * 0.6f + speedVariance * 0.4f;

            // Patrol detection: if they return near start position
            if (prof.history.size() > 60) {
                Vector start = prof.history.front().pos;
                Vector end = prof.history.back().pos;
                float returnDist = sqrtf(
                    (end.x - start.x) * (end.x - start.x) +
                    (end.y - start.y) * (end.y - start.y));
                prof.isPatrolling = (returnDist < 200.f && prof.avgSpeed > 50.f);
            }
        }
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled) return;

        auto& cache = config::BoneRead();

        if (show_paths) {
            for (int i = 1; i < 128; ++i) {
                if (!g_profiles[i].valid || g_profiles[i].history.size() < 5) continue;
                if (!cache[i].valid) continue;
                if (cache[i].distance > config::esp_max_dist) continue;

                ImU32 pathCol = g_profiles[i].isPatrolling ?
                    IM_COL32(255, 200, 0, 100) : IM_COL32(0, 180, 255, 80);

                float prevSx = 0, prevSy = 0;
                bool hasPrev = false;
                for (size_t j = 0; j < g_profiles[i].history.size(); j += 3) {
                    float sx, sy;
                    if (config::WorldToScreen(g_profiles[i].history[j].pos, sx, sy)) {
                        if (hasPrev) {
                            dl->AddLine(ImVec2(prevSx, prevSy), ImVec2(sx, sy), pathCol, 1.2f);
                        }
                        prevSx = sx; prevSy = sy;
                        hasPrev = true;
                    } else {
                        hasPrev = false;
                    }
                }
            }
        }

        // Info panel
        if (show_panel) {
            float px = static_cast<float>(screenW) - 250.f;
            float py = 200.f;
            float lineH = fontSize * 0.75f + 2.f;

            int count = 0;
            for (int i = 1; i < 128; ++i)
                if (g_profiles[i].valid && cache[i].valid) count++;
            if (count == 0) return;

            float panelH = 28.f + count * lineH + 5.f;
            dl->AddRectFilled(ImVec2(px, py), ImVec2(px + 240.f, py + panelH),
                              IM_COL32(10, 10, 16, 200), 5.f);
            dl->AddRect(ImVec2(px, py), ImVec2(px + 240.f, py + panelH),
                        IM_COL32(0, 180, 216, 100), 5.f);
            dl->AddText(font, fontSize * 0.85f, ImVec2(px + 6.f, py + 3.f),
                        IM_COL32(0, 180, 216, 255), "PLAYER PROFILER");

            float y = py + 25.f;
            for (int i = 1; i < 128; ++i) {
                if (!g_profiles[i].valid || !cache[i].valid) continue;
                const char* name = cache[i].rpName[0] ? cache[i].rpName : cache[i].name;

                char buf[128];
                snprintf(buf, sizeof(buf), "%s %.0f/s P:%.0f%% %s",
                         name, g_profiles[i].avgSpeed,
                         g_profiles[i].predictability * 100.f,
                         g_profiles[i].isPatrolling ? "[PATROL]" : "");

                ImU32 col = g_profiles[i].predictability > 0.7f ?
                    IM_COL32(80, 255, 80, 200) : IM_COL32(200, 200, 200, 180);
                dl->AddText(font, fontSize * 0.7f, ImVec2(px + 8.f, y), col, buf);
                y += lineH;
            }
        }
    }

} // namespace player_profiler
