#pragma once
#include "../includes.hpp"
#include "bot_nav.hpp"

namespace follow_bot {

    // ---- config ----
    inline bool enabled = false;
    inline int  targetIdx = -1;
    inline float follow_distance = 150.f;   // ideal spacing from target (source units)
    inline float stop_distance = 100.f;     // stop moving when closer than this
    inline float sprint_distance = 500.f;   // use full speed when farther than this
    inline float max_speed = 450.f;
    inline bool  auto_jump = true;
    inline bool  auto_door = true;           // +USE on doors when blocked
    inline bool  silent_move = true;         // don't change view angles
    inline bool  draw_path = true;           // draw line to target on screen
    inline bool  mimic_mode = false;         // copy target's movement style

    // ---- internal state ----
    inline Vector g_lastOwnPos{};
    inline float  g_stuckTimer = 0.f;
    inline int    g_stuckJumps = 0;
    inline float  g_strafeDir = 1.f;        // 1 or -1 for obstacle avoidance direction
    inline float  g_avoidTimer = 0.f;
    inline Vector g_lastTargetPos{};
    inline bool   g_hasPath = false;

    // ---- breadcrumb trail for smooth following ----
    static constexpr int MAX_CRUMBS = 64;
    struct Crumb { Vector pos; float time; };
    inline Crumb g_crumbs[MAX_CRUMBS]{};
    inline int   g_crumbHead = 0;
    inline int   g_crumbCount = 0;

    // ---- helpers ----
    inline bool TraceVisible(Vector from, Vector to, C_BasePlayer* skip) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        ray.Init(from, to);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        return trace.fraction > 0.97f;
    }

    inline bool TraceGround(Vector pos, C_BasePlayer* skip, float* outZ = nullptr) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        Vector down = pos;
        down.z -= 200.f;
        ray.Init(pos, down);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        if (outZ) *outZ = trace.endPos.z;
        return trace.fraction < 1.f;
    }

    inline bool TraceForward(Vector pos, Vector dir, float dist, C_BasePlayer* skip, float* outFrac = nullptr) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Vector end = { pos.x + dir.x * dist, pos.y + dir.y * dist, pos.z };
        Ray_t ray;
        ray.Init(pos, end);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        if (outFrac) *outFrac = trace.fraction;
        return trace.fraction < 0.9f;
    }

    // Record target's position periodically for breadcrumb trailing
    inline void RecordCrumb(Vector pos, float curtime) {
        if (g_crumbCount > 0) {
            int last = (g_crumbHead - 1 + MAX_CRUMBS) % MAX_CRUMBS;
            float dx = pos.x - g_crumbs[last].pos.x;
            float dy = pos.y - g_crumbs[last].pos.y;
            if (dx * dx + dy * dy < 30.f * 30.f) return;
        }
        g_crumbs[g_crumbHead] = { pos, curtime };
        g_crumbHead = (g_crumbHead + 1) % MAX_CRUMBS;
        if (g_crumbCount < MAX_CRUMBS) g_crumbCount++;
    }

    // Find the best crumb to walk toward (closest to us that is farther than stop_distance from target)
    inline Vector FindTrailTarget(Vector ownPos, Vector targetPos) {
        if (g_crumbCount < 2) return targetPos;

        float bestDist = 999999.f;
        Vector bestPos = targetPos;

        for (int i = 0; i < g_crumbCount; i++) {
            int idx = (g_crumbHead - 1 - i + MAX_CRUMBS * 2) % MAX_CRUMBS;
            const auto& c = g_crumbs[idx];

            float dxT = c.pos.x - targetPos.x;
            float dyT = c.pos.y - targetPos.y;
            float distToTarget = sqrtf(dxT * dxT + dyT * dyT);

            if (distToTarget < follow_distance * 0.5f) continue;

            float dxO = c.pos.x - ownPos.x;
            float dyO = c.pos.y - ownPos.y;
            float distToUs = sqrtf(dxO * dxO + dyO * dyO);

            if (distToUs < bestDist) {
                bestDist = distToUs;
                bestPos = c.pos;
            }
        }
        return bestPos;
    }

    // Convert world direction to forwardmove/sidemove relative to view angles
    inline void DirectionToMove(Vector dir, float speed, CUserCmd* cmd) {
        float dirYaw = rad2deg(atan2f(dir.y, dir.x));
        float delta = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
        cmd->forwardmove = cosf(delta) * speed;
        cmd->sidemove = -sinf(delta) * speed;
    }

    // ---- cached A* path ----
    inline std::vector<Vector> g_navPath;
    inline int   g_navPathIdx = 0;
    inline float g_navPathTime = 0.f;
    inline bool  g_useNavPath = false;

    // ---- ray-based steering state ----
    static constexpr int STEER_RAYS = 24;
    static constexpr float STEER_FOV = 240.f;
    static constexpr float STEER_RANGE = 200.f;
    inline float g_smoothYaw = 0.f;
    inline bool  g_smoothInit = false;
    inline float g_wallFollowYaw = 0.f;
    inline bool  g_wallFollowing = false;
    inline float g_wallFollowTime = 0.f;
    inline float g_wallFollowSide = 1.f; // 1 = follow right wall, -1 = left

    inline float RayClearance(Vector origin, float yawDeg, float range, C_BasePlayer* skip) {
        float rad = deg2rad(yawDeg);
        Vector dir = { cosf(rad), sinf(rad), 0.f };
        Vector end = { origin.x + dir.x * range, origin.y + dir.y * range, origin.z };
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        ray.Init(origin, end);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        return trace.fraction;
    }

    inline float SteerScore(float rayYaw, float targetYaw, float clearance) {
        float angleDiff = fabsf(normalize_yaw(rayYaw - targetYaw));
        float directionScore = 1.f - (angleDiff / 180.f);
        float clearScore = clearance;
        if (clearance < 0.2f) return -1.f;
        return directionScore * 0.6f + clearScore * 0.4f;
    }

    // ---- main update (called from CreateMove) ----
    inline void Update(CUserCmd* cmd, C_BasePlayer* localPlayer) {
        if (!enabled || targetIdx < 0 || targetIdx >= 128) return;
        if (!localPlayer) return;
        if (!interfaces::trace) return;

        auto& bones = config::BoneRead();
        const auto& target = bones[targetIdx];
        if (!target.valid || target.dormant) return;

        Vector ownPos = localPlayer->GetAbsOrigin();
        Vector targetPos = target.absOrigin;
        float curtime = interfaces::globalVars->curtime;
        float dt = interfaces::globalVars->interval_per_tick;

        RecordCrumb(targetPos, curtime);

        float dx = targetPos.x - ownPos.x;
        float dy = targetPos.y - ownPos.y;
        float dist2D = sqrtf(dx * dx + dy * dy);

        if (dist2D < stop_distance) {
            g_hasPath = true;
            bot_nav::ResetStuck();
            return;
        }

        Vector eyePos = localPlayer->GetEyePosition();
        Vector targetEye = { targetPos.x, targetPos.y, targetPos.z + 64.f };
        bool directVis = TraceVisible(eyePos, targetEye, localPlayer);

        float targetYaw = rad2deg(atan2f(dy, dx));

        // ---- Speed ramping ----
        float speed;
        if (dist2D > sprint_distance)
            speed = max_speed;
        else {
            float t = (dist2D - stop_distance) / (sprint_distance - stop_distance);
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            speed = max_speed * (0.3f + 0.7f * t);
        }

        // ---- Water detection ----
        if (bot_nav::IsInWater(ownPos, localPlayer)) {
            Vector navT = targetPos;
            bot_nav::SwimToward(navT, cmd, localPlayer);
            g_hasPath = true;
            g_lastTargetPos = targetPos;
            g_lastOwnPos = ownPos;
            return;
        }

        // ---- Mimic mode speed override ----
        if (mimic_mode) {
            C_BasePlayer* targetPly = (C_BasePlayer*)interfaces::entityList->GetClientEntity(targetIdx);
            if (targetPly && targetPly->IsPlayer()) {
                Vector tvel = targetPly->GetVelocity();
                float tspeed = tvel.Length2D();
                if (tspeed < 5.f) {
                    cmd->forwardmove = 0.f;
                    cmd->sidemove = 0.f;
                    g_hasPath = true;
                    g_lastTargetPos = targetPos;
                    return;
                }
                speed = fminf(tspeed, max_speed);
                if (targetPly->GetFlags() & FL_DUCKING)
                    cmd->buttons |= CUserCmd::IN_DUCK;
            }
        }

        float chosenYaw = targetYaw;
        Vector waist = { ownPos.x, ownPos.y, ownPos.z + 36.f };

        if (directVis) {
            // Direct line of sight — walk straight but still avoid immediate obstacles
            g_wallFollowing = false;
            float fwdClear = RayClearance(waist, targetYaw, 80.f, localPlayer);
            if (fwdClear > 0.5f) {
                chosenYaw = targetYaw;
            } else {
                // Obstacle between us and visible target, use steering
                float bestScore = -2.f;
                for (int i = 0; i < STEER_RAYS; i++) {
                    float angle = targetYaw - STEER_FOV * 0.5f + STEER_FOV * (float)i / (float)(STEER_RAYS - 1);
                    float cl = RayClearance(waist, angle, STEER_RANGE, localPlayer);
                    float sc = SteerScore(angle, targetYaw, cl);
                    if (sc > bestScore) { bestScore = sc; chosenYaw = angle; }
                }
            }
        } else {
            // No direct visibility — use ray-based steering with wall following

            // First try A* if nav graph has nodes
            bool usedNav = false;
            {
                float tMoveDx = targetPos.x - g_lastTargetPos.x;
                float tMoveDy = targetPos.y - g_lastTargetPos.y;
                float tMoveDist = sqrtf(tMoveDx * tMoveDx + tMoveDy * tMoveDy);

                if (!g_useNavPath || tMoveDist > 200.f || curtime - g_navPathTime > 2.f) {
                    g_navPath = bot_nav::FindPath(ownPos, targetPos, localPlayer);
                    bot_nav::SmoothPath(g_navPath, localPlayer);
                    g_navPathIdx = 0;
                    g_navPathTime = curtime;
                    g_useNavPath = !g_navPath.empty();
                }

                if (g_useNavPath && g_navPathIdx < (int)g_navPath.size()) {
                    Vector wp = g_navPath[g_navPathIdx];
                    float wpDx = wp.x - ownPos.x;
                    float wpDy = wp.y - ownPos.y;
                    if (sqrtf(wpDx * wpDx + wpDy * wpDy) < 40.f) {
                        g_navPathIdx++;
                        if (g_navPathIdx < (int)g_navPath.size())
                            wp = g_navPath[g_navPathIdx];
                    }
                    if (g_navPathIdx < (int)g_navPath.size()) {
                        float wpYaw = rad2deg(atan2f(wp.y - ownPos.y, wp.x - ownPos.x));
                        targetYaw = wpYaw;
                        usedNav = true;
                    }
                }
            }

            // Multi-ray steering: cast STEER_RAYS across a fan, score each direction
            float bestScore = -2.f;
            float bestYaw = targetYaw;
            bool anyGoodRay = false;

            for (int i = 0; i < STEER_RAYS; i++) {
                float angle = targetYaw - STEER_FOV * 0.5f + STEER_FOV * (float)i / (float)(STEER_RAYS - 1);
                float cl = RayClearance(waist, angle, STEER_RANGE, localPlayer);
                float sc = SteerScore(angle, targetYaw, cl);
                if (cl > 0.3f) anyGoodRay = true;
                if (sc > bestScore) { bestScore = sc; bestYaw = angle; }
            }

            if (anyGoodRay && bestScore > 0.f) {
                chosenYaw = bestYaw;
                g_wallFollowing = false;
            } else {
                // All directions blocked or poor — engage wall following
                if (!g_wallFollowing) {
                    g_wallFollowing = true;
                    g_wallFollowTime = curtime;
                    // Pick side: check which side of the wall has more space
                    float leftClear = RayClearance(waist, targetYaw + 90.f, STEER_RANGE, localPlayer);
                    float rightClear = RayClearance(waist, targetYaw - 90.f, STEER_RANGE, localPlayer);
                    g_wallFollowSide = (leftClear >= rightClear) ? 1.f : -1.f;
                    g_wallFollowYaw = targetYaw + 90.f * g_wallFollowSide;
                }

                // Wall follow: slide along the wall keeping it to one side
                // Find the direction along the wall that is clear
                float wallYaw = g_wallFollowYaw;
                float wallClear = RayClearance(waist, wallYaw, STEER_RANGE, localPlayer);
                if (wallClear < 0.3f) {
                    // Wall ahead on follow path, turn more
                    g_wallFollowYaw += 30.f * g_wallFollowSide;
                    wallYaw = g_wallFollowYaw;
                }
                chosenYaw = wallYaw;

                // Check if we can see target again to exit wall following
                float toTargetClear = RayClearance(waist, targetYaw, STEER_RANGE, localPlayer);
                if (toTargetClear > 0.7f) {
                    g_wallFollowing = false;
                }
                // Timeout wall following after 5 seconds, try something new
                if (curtime - g_wallFollowTime > 5.f) {
                    g_wallFollowing = false;
                    g_wallFollowSide = -g_wallFollowSide; // try other side next time
                }
            }
        }

        // ---- Smooth the steering to avoid jittery movement ----
        if (!g_smoothInit) {
            g_smoothYaw = chosenYaw;
            g_smoothInit = true;
        }
        float yawDiff = normalize_yaw(chosenYaw - g_smoothYaw);
        float maxTurn = 300.f * dt; // degrees per tick
        if (fabsf(yawDiff) > maxTurn)
            g_smoothYaw += (yawDiff > 0 ? maxTurn : -maxTurn);
        else
            g_smoothYaw = chosenYaw;
        g_smoothYaw = normalize_yaw(g_smoothYaw);

        float finalRad = deg2rad(g_smoothYaw);
        Vector moveDir = { cosf(finalRad), sinf(finalRad), 0.f };

        // ---- Fall damage avoidance ----
        {
            Vector ahead = { ownPos.x + moveDir.x * 60.f, ownPos.y + moveDir.y * 60.f, ownPos.z };
            float fallDmg = bot_nav::CheckFallDamage(ownPos, ahead, localPlayer);
            if (fallDmg > 30.f) {
                Vector safe = bot_nav::FindSafeDropPoint(ownPos, moveDir, 200.f, localPlayer);
                float sdx = safe.x - ownPos.x, sdy = safe.y - ownPos.y;
                float slen = sqrtf(sdx * sdx + sdy * sdy);
                if (slen > 1.f) {
                    moveDir.x = sdx / slen;
                    moveDir.y = sdy / slen;
                }
            }
        }

        // ---- Step / jump detection ----
        if (auto_jump && (localPlayer->GetFlags() & FL_ONGROUND)) {
            Vector knee = { ownPos.x, ownPos.y, ownPos.z + 20.f };
            float kneeFrac = 0.f;
            bool kneeBlocked = TraceForward(knee, moveDir, 40.f, localPlayer, &kneeFrac);
            if (kneeBlocked && kneeFrac < 0.5f) {
                Vector stepTop = { ownPos.x + moveDir.x * 40.f, ownPos.y + moveDir.y * 40.f, ownPos.z + 60.f };
                float groundZ = 0.f;
                if (TraceGround(stepTop, localPlayer, &groundZ)) {
                    float stepH = groundZ - ownPos.z;
                    if (stepH > 2.f && stepH < 64.f)
                        cmd->buttons |= CUserCmd::IN_JUMP;
                }
            }

            auto stairs = bot_nav::DetectStairs(ownPos, moveDir, localPlayer);
            if (stairs.detected && stairs.totalHeight > 18.f)
                cmd->buttons |= CUserCmd::IN_JUMP;

            Vector ahead = { ownPos.x + moveDir.x * 50.f, ownPos.y + moveDir.y * 50.f, ownPos.z + 10.f };
            float gapZ = 0.f;
            bool hasGround = TraceGround(ahead, localPlayer, &gapZ);
            if (!hasGround || (ownPos.z - gapZ) > 80.f)
                cmd->buttons |= CUserCmd::IN_JUMP;
        }

        // ---- Stuck recovery ----
        if (bot_nav::UpdateStuck(ownPos, dt)) {
            bot_nav::ApplyUnstuck(cmd, moveDir);
            if (auto_door) cmd->buttons |= CUserCmd::IN_USE;
            g_lastOwnPos = ownPos;
            g_lastTargetPos = targetPos;
            g_hasPath = true;
            return;
        }
        g_lastOwnPos = ownPos;

        // ---- Ladder detection ----
        if (auto_jump) {
            Vector ladderCheck = { ownPos.x + moveDir.x * 30.f, ownPos.y + moveDir.y * 30.f, ownPos.z + 40.f };
            CTrace ladderTrace;
            TraceFilterSimple ladderFilter(localPlayer);
            Ray_t ladderRay;
            ladderRay.Init(ownPos, ladderCheck);
            interfaces::trace->TraceRay(ladderRay, MASK_ALL, &ladderFilter, &ladderTrace);
            if (ladderTrace.contents & 0x20000000) {
                cmd->buttons |= CUserCmd::IN_FORWARD;
                cmd->forwardmove = max_speed;
            }
        }

        // ---- Auto door ----
        if (auto_door) {
            float doorFrac = 0.f;
            if (TraceForward(waist, moveDir, 50.f, localPlayer, &doorFrac) && doorFrac < 0.4f)
                cmd->buttons |= CUserCmd::IN_USE;
        }

        DirectionToMove(moveDir, speed, cmd);
        g_hasPath = true;
        g_lastTargetPos = targetPos;
    }

    // ---- Draw path line to target (called from render thread) ----
    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize) {
        if (!enabled || !draw_path || targetIdx < 0) return;

        auto& bones = config::BoneRead();
        if (!bones[targetIdx].valid) return;

        int localIdx = interfaces::engine->GetLocalPlayer();
        if (localIdx < 0 || localIdx >= 128 || !bones[localIdx].valid) return;

        Vector ownPos = bones[localIdx].absOrigin;
        ownPos.z += 36.f;
        Vector targetPos = bones[targetIdx].absOrigin;
        targetPos.z += 36.f;

        float sx1, sy1, sx2, sy2;
        if (!config::WorldToScreen(ownPos, sx1, sy1)) return;
        if (!config::WorldToScreen(targetPos, sx2, sy2)) return;

        // Dashed line from us to target
        float ddx = sx2 - sx1, ddy = sy2 - sy1;
        float lineLen = sqrtf(ddx * ddx + ddy * ddy);
        if (lineLen < 2.f) return;

        float dirX = ddx / lineLen, dirY = ddy / lineLen;
        float drawn = 0.f;
        bool on = true;
        while (drawn < lineLen) {
            float seg = on ? 8.f : 5.f;
            if (drawn + seg > lineLen) seg = lineLen - drawn;
            if (on) {
                dl->AddLine(
                    ImVec2(sx1 + dirX * drawn, sy1 + dirY * drawn),
                    ImVec2(sx1 + dirX * (drawn + seg), sy1 + dirY * (drawn + seg)),
                    IM_COL32(0, 200, 255, 180), 1.5f);
            }
            drawn += seg;
            on = !on;
        }

        // Breadcrumb dots
        for (int i = 0; i < g_crumbCount; i++) {
            int idx = (g_crumbHead - 1 - i + MAX_CRUMBS * 2) % MAX_CRUMBS;
            float cx, cy;
            Vector cp = g_crumbs[idx].pos;
            cp.z += 10.f;
            if (config::WorldToScreen(cp, cx, cy)) {
                float alpha = 1.f - (float)i / (float)g_crumbCount;
                dl->AddCircleFilled(ImVec2(cx, cy), 2.f,
                    IM_COL32(0, 200, 255, (int)(alpha * 120.f)), 6);
            }
        }

        // Distance and status text at target
        float dist = sqrtf(
            (bones[targetIdx].absOrigin.x - bones[localIdx].absOrigin.x) *
            (bones[targetIdx].absOrigin.x - bones[localIdx].absOrigin.x) +
            (bones[targetIdx].absOrigin.y - bones[localIdx].absOrigin.y) *
            (bones[targetIdx].absOrigin.y - bones[localIdx].absOrigin.y));

        char buf[64];
        snprintf(buf, sizeof(buf), "FOLLOWING [%.0fm]", dist / 52.49f);
        ImVec2 ts = font->CalcTextSizeA(fontSize * 0.8f, FLT_MAX, 0.f, buf);
        dl->AddText(font, fontSize * 0.8f,
            ImVec2(sx2 - ts.x * 0.5f, sy2 + 12.f),
            IM_COL32(0, 220, 255, 200), buf);

        // Follow distance ring at target feet
        const char* tname = bones[targetIdx].rpName[0] ? bones[targetIdx].rpName : bones[targetIdx].name;
        if (tname[0]) {
            ImVec2 ns = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, tname);
            dl->AddText(font, fontSize * 0.7f,
                ImVec2(sx2 - ns.x * 0.5f, sy2 + 12.f + fontSize),
                IM_COL32(200, 200, 200, 160), tname);
        }
    }

    inline void Reset() {
        targetIdx = -1;
        g_stuckTimer = 0.f;
        g_stuckJumps = 0;
        g_avoidTimer = 0.f;
        g_crumbHead = 0;
        g_crumbCount = 0;
        g_hasPath = false;
    }

} // namespace follow_bot
