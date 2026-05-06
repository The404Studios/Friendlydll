#pragma once
#include "../includes.hpp"
#include <cmath>
#include <algorithm>

namespace death_replay {

    // ---- config toggles ----
    inline bool enabled = false;
    inline float replay_seconds = 3.0f;

    // ---- per-player snapshot (lightweight: no full bone matrices) ----
    struct PlayerSnap {
        Vector headPos;
        Vector origin;
        bool   valid;
        int    health;
    };

    // ---- one tick of the whole server ----
    struct FrameSnap {
        PlayerSnap players[128];
        float      timestamp;
    };

    // ---- ring buffer ----
    static constexpr int MAX_FRAMES = 256;   // ~7-8 s at 33 tick
    inline FrameSnap g_frames[MAX_FRAMES]{};
    inline int g_head  = 0;                  // next write slot
    inline int g_count = 0;                  // how many valid frames stored

    // ---- replay state ----
    inline bool  g_replaying       = false;
    inline float g_replayStartTime = 0.f;
    inline int   g_replayFrameStart = 0;     // oldest frame index at moment of death
    inline int   g_replayFrameCount = 0;     // number of frames captured for this replay
    inline int   g_lastLocalHealth  = 100;

    // ---- cancel key (any of these dismiss the replay early) ----
    static constexpr int CANCEL_KEYS[] = {
        VK_ESCAPE, VK_SPACE, VK_RETURN, VK_LBUTTON, VK_RBUTTON
    };

    // =========================================================================
    //  Update  --  called from the game thread (CreateMove / similar)
    //
    //  localPlayerIdx : entity index of the local player (1-based)
    //  curtime        : engine curtime (e.g. interfaces::globalVars->curtime)
    // =========================================================================
    inline void Update(int localPlayerIdx, float curtime) {
        if (!enabled) return;

        const auto& bones = config::BoneRead();

        // ---- record current tick into the ring buffer ----
        FrameSnap& snap = g_frames[g_head];
        snap.timestamp = curtime;

        for (int i = 0; i < 128; ++i) {
            const auto& br = bones[i];
            auto& ps = snap.players[i];

            if (!br.valid || br.dormant) {
                ps.valid  = false;
                ps.health = 0;
                ps.headPos = {};
                ps.origin  = {};
                continue;
            }

            ps.valid  = true;
            ps.health = br.health;
            ps.origin = br.absOrigin;

            if (br.noBones) {
                ps.headPos = Vector(br.absOrigin.x, br.absOrigin.y, br.absOrigin.z + 72.f);
            } else {
                const auto& hb = br.bones[Bones::bone_head];
                ps.headPos = Vector(hb[0][3], hb[1][3], hb[2][3]);
            }
        }

        g_head = (g_head + 1) % MAX_FRAMES;
        if (g_count < MAX_FRAMES) ++g_count;

        // ---- detect local player death (health > 0  -->  <= 0) ----
        int curHealth = 0;
        if (localPlayerIdx >= 0 && localPlayerIdx < 128) {
            const auto& lp = bones[localPlayerIdx];
            if (lp.valid) curHealth = lp.health;
        }

        if (g_lastLocalHealth > 0 && curHealth <= 0 && !g_replaying && g_count > 0) {
            // trigger replay
            g_replaying        = true;
            g_replayStartTime  = curtime;
            g_replayFrameCount = g_count;
            // oldest frame is (g_head - g_count) wrapped
            g_replayFrameStart = (g_head - g_count + MAX_FRAMES) % MAX_FRAMES;
        }

        g_lastLocalHealth = curHealth;
    }

    // =========================================================================
    //  Draw  --  called from the render thread (Present / EndScene)
    //
    //  dl       : ImGui overlay draw list
    //  font     : loaded ImFont*
    //  fontSize : base text size
    //  screenW/H: current viewport
    //  curtime  : engine curtime
    // =========================================================================
    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize,
                     int screenW, int screenH, float curtime)
    {
        if (!enabled) return;
        if (!g_replaying) return;
        if (!dl) return;

        // ---- check for cancel keys ----
        for (int vk : CANCEL_KEYS) {
            if (GetAsyncKeyState(vk) & 0x8000) {
                g_replaying = false;
                return;
            }
        }

        // ---- timing ----
        float elapsed    = curtime - g_replayStartTime;
        float totalTime  = replay_seconds;
        float progress   = elapsed / totalTime;   // 0..1

        if (progress >= 1.f) {
            g_replaying = false;
            return;
        }

        // Which frame index (within our captured window) should be the "current" one?
        int curFrame = static_cast<int>(progress * g_replayFrameCount);
        curFrame = (std::min)(curFrame, g_replayFrameCount - 1);

        // Number of trailing "ghost" frames to render behind the current one
        constexpr int TRAIL_LEN = 12;

        // ---- dim the background slightly ----
        dl->AddRectFilled(ImVec2(0, 0),
                          ImVec2(static_cast<float>(screenW), static_cast<float>(screenH)),
                          IM_COL32(0, 0, 0, 100));

        // ---- render trail + current frame ----
        int trailStart = (std::max)(0, curFrame - TRAIL_LEN);
        for (int fi = trailStart; fi <= curFrame; ++fi) {
            int ringIdx = (g_replayFrameStart + fi) % MAX_FRAMES;
            const FrameSnap& frame = g_frames[ringIdx];

            // alpha: newest = 255, oldest trail = 40
            float age01 = 1.f - static_cast<float>(curFrame - fi) / static_cast<float>(TRAIL_LEN + 1);
            int alpha = 40 + static_cast<int>(215.f * age01);
            alpha = (std::min)(alpha, 255);

            float radius = (fi == curFrame) ? 5.f : 3.f;

            for (int pi = 0; pi < 128; ++pi) {
                const PlayerSnap& ps = frame.players[pi];
                if (!ps.valid) continue;
                if (ps.health <= 0) continue;

                float sx, sy;
                if (!config::WorldToScreen(ps.headPos, sx, sy)) continue;

                // living players are red (enemy assumption in death context)
                ImU32 col = IM_COL32(255, 60, 60, alpha);

                dl->AddCircleFilled(ImVec2(sx, sy), radius, col, 12);

                // for the current (newest) frame, also draw a small vertical line
                // from origin to head so the silhouette reads better
                if (fi == curFrame) {
                    float ox, oy;
                    if (config::WorldToScreen(ps.origin, ox, oy)) {
                        dl->AddLine(ImVec2(ox, oy), ImVec2(sx, sy),
                                    IM_COL32(255, 100, 100, alpha / 2), 1.5f);
                    }
                }
            }
        }

        // ---- "DEATH REPLAY" title ----
        {
            const char* title = "DEATH REPLAY";
            float titleSize = fontSize * 2.2f;
            ImVec2 ts = font->CalcTextSizeA(titleSize, FLT_MAX, 0.f, title);
            float tx = (static_cast<float>(screenW) - ts.x) * 0.5f;
            float ty = 40.f;

            // shadow
            dl->AddText(font, titleSize, ImVec2(tx + 2.f, ty + 2.f),
                        IM_COL32(0, 0, 0, 200), title);
            // main text (pulsing red)
            int pulse = 180 + static_cast<int>(75.f * sinf(curtime * 5.f));
            dl->AddText(font, titleSize, ImVec2(tx, ty),
                        IM_COL32(pulse, 30, 30, 255), title);
        }

        // ---- elapsed time counter ----
        {
            char timeBuf[32];
            snprintf(timeBuf, sizeof(timeBuf), "-%.1fs", totalTime - elapsed);
            ImVec2 ts = font->CalcTextSizeA(fontSize * 1.4f, FLT_MAX, 0.f, timeBuf);
            float tx = (static_cast<float>(screenW) - ts.x) * 0.5f;
            float ty = 90.f;
            dl->AddText(font, fontSize * 1.4f, ImVec2(tx, ty),
                        IM_COL32(220, 220, 220, 200), timeBuf);
        }

        // ---- progress bar at the bottom ----
        {
            float barH   = 6.f;
            float barPad = 40.f;
            float barY   = static_cast<float>(screenH) - 30.f;
            float barW   = static_cast<float>(screenW) - barPad * 2.f;

            // background
            dl->AddRectFilled(ImVec2(barPad, barY),
                              ImVec2(barPad + barW, barY + barH),
                              IM_COL32(40, 40, 40, 180), 3.f);
            // filled portion
            dl->AddRectFilled(ImVec2(barPad, barY),
                              ImVec2(barPad + barW * progress, barY + barH),
                              IM_COL32(255, 60, 60, 220), 3.f);
            // border
            dl->AddRect(ImVec2(barPad, barY),
                        ImVec2(barPad + barW, barY + barH),
                        IM_COL32(255, 80, 80, 120), 3.f);
        }

        // ---- small hint text ----
        {
            const char* hint = "Press ESC / SPACE / CLICK to dismiss";
            ImVec2 hs = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.f, hint);
            float hx = (static_cast<float>(screenW) - hs.x) * 0.5f;
            float hy = static_cast<float>(screenH) - 50.f;
            dl->AddText(font, fontSize * 0.85f, ImVec2(hx, hy),
                        IM_COL32(180, 180, 180, 160), hint);
        }
    }

} // namespace death_replay
