#pragma once
#include "../includes.hpp"
#include "antiaim.hpp"
#include "fakelag.hpp"

namespace rage_mode {

    inline bool active = false;
    inline int toggle_key = VK_F5;

    struct SavedConfig {
        bool aimbot, silent, autoshoot, rcs, triggerbot, backtrack, viewpunch_remove;
        float aimbot_fov, aim_smooth;
        int bone;
        bool antiaim_enabled, fakelag_enabled;
        int antiaim_yaw, antiaim_pitch;
    };

    inline SavedConfig g_saved{};
    inline bool g_hasSaved = false;

    inline void Activate() {
        if (active) return;
        active = true;

        g_saved.aimbot = config::aimbot;
        g_saved.silent = config::silent;
        g_saved.autoshoot = config::autoshoot;
        g_saved.rcs = config::rcs;
        g_saved.triggerbot = config::triggerbot;
        g_saved.backtrack = config::backtrack;
        g_saved.viewpunch_remove = config::viewpunch_remove;
        g_saved.aimbot_fov = config::aimbot_fov;
        g_saved.aim_smooth = config::aim_smooth;
        g_saved.bone = config::bone;
        g_saved.antiaim_enabled = antiaim::enabled;
        g_saved.fakelag_enabled = fakelag::enabled;
        g_saved.antiaim_yaw = antiaim::yaw_mode;
        g_saved.antiaim_pitch = antiaim::pitch_mode;
        g_hasSaved = true;

        config::aimbot = true;
        config::silent = true;
        config::autoshoot = true;
        config::rcs = true;
        config::triggerbot = true;
        config::backtrack = true;
        config::viewpunch_remove = true;
        config::aimbot_fov = 180.f;
        config::aim_smooth = 0.f;
        config::bone = Bones::bone_head;
        antiaim::enabled = true;
        antiaim::yaw_mode = 0; // jitter
        antiaim::pitch_mode = 1; // down
        fakelag::enabled = true;
    }

    inline void Deactivate() {
        if (!active) return;
        active = false;

        if (g_hasSaved) {
            config::aimbot = g_saved.aimbot;
            config::silent = g_saved.silent;
            config::autoshoot = g_saved.autoshoot;
            config::rcs = g_saved.rcs;
            config::triggerbot = g_saved.triggerbot;
            config::backtrack = g_saved.backtrack;
            config::viewpunch_remove = g_saved.viewpunch_remove;
            config::aimbot_fov = g_saved.aimbot_fov;
            config::aim_smooth = g_saved.aim_smooth;
            config::bone = g_saved.bone;
            antiaim::enabled = g_saved.antiaim_enabled;
            antiaim::yaw_mode = g_saved.antiaim_yaw;
            antiaim::pitch_mode = g_saved.antiaim_pitch;
            fakelag::enabled = g_saved.fakelag_enabled;
        }
    }

    inline void CheckToggle() {
        if (GetAsyncKeyState(toggle_key) & 1) {
            if (active)
                Deactivate();
            else
                Activate();
        }
    }

    inline void DrawIndicator(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!active) return;

        const char* text = "RAGE MODE";
        float titleSize = fontSize * 1.8f;
        ImVec2 sz = font->CalcTextSizeA(titleSize, FLT_MAX, 0.f, text);
        float tx = (static_cast<float>(screenW) - sz.x) * 0.5f;
        float ty = 40.f;

        float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 5.f) * 0.3f + 0.7f;
        int alpha = static_cast<int>(pulse * 255.f);

        dl->AddRectFilled(ImVec2(tx - 15.f, ty - 6.f), ImVec2(tx + sz.x + 15.f, ty + sz.y + 6.f),
                          IM_COL32(120, 0, 0, 180), 6.f);
        dl->AddRect(ImVec2(tx - 15.f, ty - 6.f), ImVec2(tx + sz.x + 15.f, ty + sz.y + 6.f),
                    IM_COL32(255, 0, 0, alpha), 6.f, 0, 2.f);
        dl->AddText(font, titleSize, ImVec2(tx + 2.f, ty + 2.f), IM_COL32(0, 0, 0, 200), text);
        dl->AddText(font, titleSize, ImVec2(tx, ty), IM_COL32(255, 40, 40, alpha), text);
    }

} // namespace rage_mode
