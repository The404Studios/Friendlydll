#pragma once
#include "../includes.hpp"
#include <string>
#include <vector>
#include <cstdlib>

namespace misc_features {

    // ── Config (local to avoid conflicts with parallel edits to config.hpp) ──
    inline bool thirdperson = false;
    inline float thirdperson_dist = 150.f;
    inline bool custom_crosshair = false;
    inline int crosshair_style = 0; // 0=cross, 1=dot, 2=circle
    inline float crosshair_size = 4.f;
    inline float crosshair_color[3] = { 0.f, 1.f, 0.f };
    inline float crosshair_thickness = 1.5f;

    // Chat on death messages
    inline const char* kDeathMessages[] = {
        "gg",
        "nice shot",
        "wow",
        "unlucky",
        "lag",
        "bruh",
        "outplayed",
        "fair enough",
        "rip",
        "good fight"
    };
    inline constexpr int kDeathMessageCount = 10;

    // ── Thirdperson ──────────────────────────────────────────────────────────
    inline void UpdateThirdperson() {
        static bool s_wasEnabled = false;
        static float s_lastDist = 0.f;

        if (thirdperson && !s_wasEnabled) {
            auto cmd = std::format(
                "pcall(function() "
                "hook.Add('CalcView','_fdll_tp',function(p,pos,ang,fov) "
                "local tr=util.TraceLine({{start=pos,endpos=pos-ang:Forward()*{:.0f},filter=p}}) "
                "return {{origin=tr.HitPos+tr.HitNormal*10,angles=ang,fov=fov}} "
                "end) end)", thirdperson_dist);
            lualoader::Execute(cmd);
            s_wasEnabled = true;
            s_lastDist = thirdperson_dist;
        }
        else if (thirdperson && s_wasEnabled && thirdperson_dist != s_lastDist) {
            // Distance changed while active — reinstall hook with new distance
            auto cmd = std::format(
                "pcall(function() "
                "hook.Add('CalcView','_fdll_tp',function(p,pos,ang,fov) "
                "local tr=util.TraceLine({{start=pos,endpos=pos-ang:Forward()*{:.0f},filter=p}}) "
                "return {{origin=tr.HitPos+tr.HitNormal*10,angles=ang,fov=fov}} "
                "end) end)", thirdperson_dist);
            lualoader::Execute(cmd);
            s_lastDist = thirdperson_dist;
        }
        else if (!thirdperson && s_wasEnabled) {
            lualoader::Execute("pcall(function() hook.Remove('CalcView','_fdll_tp') end)");
            s_wasEnabled = false;
        }
    }

    // ── Custom Crosshair ─────────────────────────────────────────────────────
    inline void DrawCrosshair(ImDrawList* dl, int screenW, int screenH) {
        if (!custom_crosshair) return;

        float cx = static_cast<float>(screenW) * 0.5f;
        float cy = static_cast<float>(screenH) * 0.5f;

        ImU32 col = ImColor(crosshair_color[0], crosshair_color[1], crosshair_color[2]);
        float sz = crosshair_size;
        float th = crosshair_thickness;

        switch (crosshair_style) {
            case 0: // cross
                dl->AddLine(ImVec2(cx - sz, cy), ImVec2(cx + sz, cy), col, th);
                dl->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy + sz), col, th);
                break;
            case 1: // dot
                dl->AddCircleFilled(ImVec2(cx, cy), sz * 0.5f, col);
                break;
            case 2: // circle
                dl->AddCircle(ImVec2(cx, cy), sz, col, 24, th);
                break;
            case 3: // cross + dot
                dl->AddLine(ImVec2(cx - sz, cy), ImVec2(cx - 2.f, cy), col, th);
                dl->AddLine(ImVec2(cx + 2.f, cy), ImVec2(cx + sz, cy), col, th);
                dl->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy - 2.f), col, th);
                dl->AddLine(ImVec2(cx, cy + 2.f), ImVec2(cx, cy + sz), col, th);
                dl->AddCircleFilled(ImVec2(cx, cy), 1.5f, col);
                break;
        }
    }

    // ── Chat on Death ────────────────────────────────────────────────────────
    inline void SendDeathMessage() {
        if (!config::chatondeath) return;
        int idx = std::rand() % kDeathMessageCount;
        auto cmd = std::format(
            "pcall(function() RunConsoleCommand('say','{}') end)",
            kDeathMessages[idx]);
        lualoader::Execute(cmd);
    }

    // Call this from CreateMove when player health transitions to 0
    inline void CheckDeathAndChat(C_BasePlayer* lp) {
        if (!config::chatondeath) return;
        if (!lp) return;

        static bool wasAlive = true;
        bool alive = lp->IsAlive();

        if (wasAlive && !alive) {
            SendDeathMessage();
        }
        wasAlive = alive;
    }

} // namespace misc_features
