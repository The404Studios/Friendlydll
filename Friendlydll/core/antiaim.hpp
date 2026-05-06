#pragma once
#include "../includes.hpp"
#include <random>

namespace antiaim {

    inline std::mt19937& Rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }

    inline bool enabled = false;
    inline int yaw_mode = 0;      // 0=jitter, 1=spin, 2=backward, 3=random, 4=desync, 5=micro
    inline int pitch_mode = 0;    // 0=none, 1=down, 2=up, 3=jitter pitch, 4=zero
    inline float jitter_range = 120.f;
    inline float spin_speed = 15.f;
    inline float desync_offset = 58.f;

    inline int g_tick = 0;
    inline float g_lbyTimer = 0.f;
    inline bool g_lbySide = false;

    inline void Apply(CUserCmd* cmd, const Angle& origAngles) {
        if (!enabled) return;

        g_tick++;

        switch (yaw_mode) {
            case 0: // jitter
                cmd->viewangles.y += (g_tick & 1) ? jitter_range : -jitter_range;
                break;
            case 1: // spin
                cmd->viewangles.y = fmodf(static_cast<float>(g_tick) * spin_speed, 360.f);
                break;
            case 2: // backward
                cmd->viewangles.y += 180.f;
                break;
            case 3: { // random
                std::uniform_real_distribution<float> dist(0.f, 360.f);
                cmd->viewangles.y = dist(Rng());
                break;
            }
            case 4: { // desync — offset body yaw from real angle, flip sides periodically
                g_lbyTimer += 1.f;
                if (g_lbyTimer > 20.f) {
                    g_lbySide = !g_lbySide;
                    g_lbyTimer = 0.f;
                }
                float side = g_lbySide ? desync_offset : -desync_offset;
                cmd->viewangles.y += 180.f + side;
                // micro-move to force LBY update on our terms
                if (g_tick % 21 == 0)
                    cmd->sidemove = (g_lbySide ? 1.01f : -1.01f);
                break;
            }
            case 5: { // micro-movement — small random offsets that confuse resolvers
                float r = static_cast<float>(rand() % 1000) / 1000.f;
                cmd->viewangles.y += 180.f + (r * jitter_range - jitter_range * 0.5f);
                break;
            }
        }

        switch (pitch_mode) {
            case 1: cmd->viewangles.p = 89.f; break;
            case 2: cmd->viewangles.p = -89.f; break;
            case 3:
                cmd->viewangles.p = (g_tick & 1) ? 89.f : -89.f;
                break;
            case 4: cmd->viewangles.p = 0.f; break;
        }

        cmd->viewangles.FixAngles();
    }

} // namespace antiaim
