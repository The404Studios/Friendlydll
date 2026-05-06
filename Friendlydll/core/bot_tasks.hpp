#pragma once
#include "../includes.hpp"

namespace bot_tasks {

    // ========================================================================
    // Bot mode enum
    // ========================================================================
    enum class BotMode {
        Idle,
        Follow,
        Guard,
        Patrol,
        Farm,
        Flee
    };

    // ---- global state ----
    inline BotMode currentMode   = BotMode::Idle;
    inline BotMode previousMode  = BotMode::Idle;

    // ========================================================================
    // Shared trace helpers (same pattern as follow_bot)
    // ========================================================================
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

    // Shared: convert world direction to forwardmove/sidemove
    inline void DirectionToMove(Vector dir, float speed, CUserCmd* cmd) {
        float dirYaw = rad2deg(atan2f(dir.y, dir.x));
        float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
        cmd->forwardmove = cosf(delta) * speed;
        cmd->sidemove    = -sinf(delta) * speed;
    }

    // Shared: auto-jump when blocked by a step or gap
    inline void AutoJump(CUserCmd* cmd, C_BasePlayer* lp, Vector moveDir) {
        if (!(lp->GetFlags() & FL_ONGROUND)) return;

        Vector ownPos = lp->GetAbsOrigin();

        // Step-up detection at knee height
        Vector knee = { ownPos.x, ownPos.y, ownPos.z + 20.f };
        float kneeFrac = 0.f;
        bool kneeBlocked = TraceForward(knee, moveDir, 40.f, lp, &kneeFrac);

        if (kneeBlocked && kneeFrac < 0.5f) {
            Vector stepTop = { ownPos.x + moveDir.x * 40.f,
                               ownPos.y + moveDir.y * 40.f,
                               ownPos.z + 60.f };
            float groundZ = 0.f;
            if (TraceGround(stepTop, lp, &groundZ)) {
                float stepH = groundZ - ownPos.z;
                if (stepH > 2.f && stepH < 64.f) {
                    cmd->buttons |= CUserCmd::IN_JUMP;
                }
            }
        }

        // Gap detection ahead
        Vector ahead = { ownPos.x + moveDir.x * 50.f,
                         ownPos.y + moveDir.y * 50.f,
                         ownPos.z + 10.f };
        float gapZ = 0.f;
        bool hasGround = TraceGround(ahead, lp, &gapZ);
        if (!hasGround || (ownPos.z - gapZ) > 80.f) {
            cmd->buttons |= CUserCmd::IN_JUMP;
        }
    }

    // ---- ray-based steering (shared with all modes) ----
    static constexpr int STEER_RAYS = 16;
    static constexpr float STEER_FOV = 240.f;
    static constexpr float STEER_RANGE = 200.f;
    inline float g_smoothYaw = 0.f;
    inline bool  g_smoothInit = false;

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

    // Shared: walk toward a world position, returns normalized move direction
    inline Vector WalkToward(Vector ownPos, Vector target, float speed, CUserCmd* cmd, C_BasePlayer* lp) {
        float dx = target.x - ownPos.x;
        float dy = target.y - ownPos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 1.f) return { 0.f, 0.f, 0.f };

        float targetYaw = rad2deg(atan2f(dy, dx));
        Vector waist = { ownPos.x, ownPos.y, ownPos.z + 36.f };

        // Check if direct path is clear
        float fwdClear = RayClearance(waist, targetYaw, 80.f, lp);
        float chosenYaw = targetYaw;

        if (fwdClear < 0.5f) {
            // Multi-ray steering: find best direction
            float bestScore = -2.f;
            for (int i = 0; i < STEER_RAYS; i++) {
                float angle = targetYaw - STEER_FOV * 0.5f + STEER_FOV * (float)i / (float)(STEER_RAYS - 1);
                float cl = RayClearance(waist, angle, STEER_RANGE, lp);
                if (cl < 0.2f) continue;
                float angleDiff = fabsf(normalize_yaw(angle - targetYaw));
                float sc = (1.f - angleDiff / 180.f) * 0.6f + cl * 0.4f;
                if (sc > bestScore) { bestScore = sc; chosenYaw = angle; }
            }
        }

        // Smooth steering
        float dt = interfaces::globalVars->interval_per_tick;
        if (!g_smoothInit) { g_smoothYaw = chosenYaw; g_smoothInit = true; }
        float yawDiff = normalize_yaw(chosenYaw - g_smoothYaw);
        float maxTurn = 300.f * dt;
        if (fabsf(yawDiff) > maxTurn)
            g_smoothYaw += (yawDiff > 0 ? maxTurn : -maxTurn);
        else
            g_smoothYaw = chosenYaw;
        g_smoothYaw = normalize_yaw(g_smoothYaw);

        float finalRad = deg2rad(g_smoothYaw);
        Vector moveDir = { cosf(finalRad), sinf(finalRad), 0.f };

        DirectionToMove(moveDir, speed, cmd);
        AutoJump(cmd, lp, moveDir);

        // Auto-door when blocked
        float doorFrac = 0.f;
        if (TraceForward(waist, moveDir, 50.f, lp, &doorFrac) && doorFrac < 0.4f)
            cmd->buttons |= CUserCmd::IN_USE;

        return moveDir;
    }

    // ========================================================================
    // Mode name helper
    // ========================================================================
    inline const char* GetModeName() {
        switch (currentMode) {
            case BotMode::Idle:   return "Idle";
            case BotMode::Follow: return "Follow";
            case BotMode::Guard:  return "Guard";
            case BotMode::Patrol: return "Patrol";
            case BotMode::Farm:   return "Farm";
            case BotMode::Flee:   return "Flee";
        }
        return "Unknown";
    }

    // ========================================================================
    // IDLE MODE
    // ========================================================================
    inline float idleLookTimer     = 0.f;
    inline float idleLookInterval  = 3.f;
    inline Angle idleLookTarget{};

    inline void IdleInit() {
        idleLookTimer = 0.f;
        idleLookTarget = Angle(0.f, 0.f, 0.f);
    }

    inline void IdleUpdate(CUserCmd* cmd, C_BasePlayer* lp) {
        // Stand still
        cmd->forwardmove = 0.f;
        cmd->sidemove    = 0.f;

        float curtime = interfaces::globalVars->curtime;

        // Slow random look sway
        if (curtime >= idleLookTimer) {
            idleLookTimer = curtime + idleLookInterval;
            // Pick a small random yaw offset from current view
            float randYaw  = (float)(rand() % 60) - 30.f;  // -30 to +30
            float randPitch = (float)(rand() % 20) - 10.f;  // -10 to +10
            idleLookTarget = Angle(
                cmd->viewangles.p + randPitch,
                cmd->viewangles.y + randYaw,
                0.f
            );
            idleLookTarget.FixAngles();
        }

        // Smoothly interpolate toward the idle look target
        float dp = normalize_yaw(idleLookTarget.p - cmd->viewangles.p);
        float dy = normalize_yaw(idleLookTarget.y - cmd->viewangles.y);
        float t  = interfaces::globalVars->interval_per_tick * 2.f;
        cmd->viewangles.p += dp * t;
        cmd->viewangles.y += dy * t;
        cmd->viewangles.FixAngles();
    }

    // ========================================================================
    // GUARD MODE
    // ========================================================================
    inline Vector guardPos{};
    inline float  guardRadius     = 300.f;
    inline float  guardReturnDist = 50.f;
    inline float  guardSpeed      = 250.f;
    inline int    guardThreatIdx  = -1;

    inline void SetGuardPos(Vector pos) {
        guardPos = pos;
        guardThreatIdx = -1;
    }

    inline void GuardInit(C_BasePlayer* lp) {
        if (guardPos.x == 0.f && guardPos.y == 0.f && guardPos.z == 0.f) {
            if (lp) guardPos = lp->GetAbsOrigin();
        }
        guardThreatIdx = -1;
    }

    inline void GuardUpdate(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp) return;

        Vector ownPos  = lp->GetAbsOrigin();
        Vector eyePos  = lp->GetEyePosition();
        float  curtime = interfaces::globalVars->curtime;
        auto&  bones   = config::BoneRead();

        // Scan for threats inside guard radius
        guardThreatIdx = -1;
        float closestThreatDist = guardRadius;

        for (int i = 1; i <= interfaces::globalVars->maxClients; ++i) {
            if (!bones[i].valid || bones[i].dormant) continue;
            if (!config::IsTargetAllowed(i)) continue;

            int localIdx = interfaces::engine->GetLocalPlayer();
            if (i == localIdx) continue;

            float dx = bones[i].absOrigin.x - guardPos.x;
            float dy = bones[i].absOrigin.y - guardPos.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < closestThreatDist) {
                // Check visibility from our eyes to threat
                Vector threatEye = { bones[i].absOrigin.x,
                                     bones[i].absOrigin.y,
                                     bones[i].absOrigin.z + 64.f };
                if (TraceVisible(eyePos, threatEye, lp)) {
                    closestThreatDist = dist;
                    guardThreatIdx = i;
                }
            }
        }

        // If we have a threat, turn to face them
        if (guardThreatIdx >= 0 && guardThreatIdx < 128 && bones[guardThreatIdx].valid) {
            Vector threatPos = bones[guardThreatIdx].absOrigin;
            threatPos.z += 64.f;
            Vector delta = threatPos - eyePos;
            Angle aim = Angle::FromVector(delta);
            aim.FixAngles();

            // Snap look toward threat
            float dp = normalize_yaw(aim.p - cmd->viewangles.p);
            float dy = normalize_yaw(aim.y - cmd->viewangles.y);
            cmd->viewangles.p += dp * 0.5f;
            cmd->viewangles.y += dy * 0.5f;
            cmd->viewangles.FixAngles();

            // Stand ground, don't move
            cmd->forwardmove = 0.f;
            cmd->sidemove    = 0.f;
            return;
        }

        // No threats -- check if we need to return to guardPos
        float dxG = guardPos.x - ownPos.x;
        float dyG = guardPos.y - ownPos.y;
        float distToPost = sqrtf(dxG * dxG + dyG * dyG);

        if (distToPost > guardReturnDist) {
            // Walk back to guard position
            WalkToward(ownPos, guardPos, guardSpeed, cmd, lp);
        } else {
            // At post, idle sway
            cmd->forwardmove = 0.f;
            cmd->sidemove    = 0.f;
        }
    }

    // ========================================================================
    // PATROL MODE
    // ========================================================================
    inline Vector patrolPoints[16]{};
    inline int    patrolCount     = 0;
    inline int    patrolIdx       = 0;
    inline bool   patrolLoop      = true;
    inline bool   patrolPingPong  = false;
    inline int    patrolDirection  = 1;      // 1 or -1 for ping-pong
    inline float  patrolSpeed     = 250.f;
    inline float  patrolReachDist = 50.f;    // how close before advancing

    inline void AddPatrolPoint(Vector pos) {
        if (patrolCount >= 16) return;
        patrolPoints[patrolCount++] = pos;
    }

    inline void ClearPatrol() {
        patrolCount     = 0;
        patrolIdx       = 0;
        patrolDirection = 1;
    }

    inline void PatrolInit() {
        patrolIdx       = 0;
        patrolDirection = 1;
    }

    inline void PatrolUpdate(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp || patrolCount < 1) return;

        // Bounds safety
        if (patrolIdx < 0) patrolIdx = 0;
        if (patrolIdx >= patrolCount) patrolIdx = patrolCount - 1;

        Vector ownPos = lp->GetAbsOrigin();
        Vector target = patrolPoints[patrolIdx];

        float dx = target.x - ownPos.x;
        float dy = target.y - ownPos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        // Reached current waypoint -- advance
        if (dist < patrolReachDist) {
            if (patrolPingPong) {
                patrolIdx += patrolDirection;
                if (patrolIdx >= patrolCount) {
                    patrolDirection = -1;
                    patrolIdx = patrolCount - 2;
                } else if (patrolIdx < 0) {
                    patrolDirection = 1;
                    patrolIdx = 1;
                }
            } else if (patrolLoop) {
                patrolIdx = (patrolIdx + 1) % patrolCount;
            } else {
                if (patrolIdx < patrolCount - 1)
                    patrolIdx++;
            }

            // Final clamp
            if (patrolIdx < 0) patrolIdx = 0;
            if (patrolIdx >= patrolCount) patrolIdx = patrolCount - 1;

            target = patrolPoints[patrolIdx];
        }

        WalkToward(ownPos, target, patrolSpeed, cmd, lp);
    }

    // ========================================================================
    // FARM MODE  (DarkRP)
    // ========================================================================
    struct FarmTarget {
        Vector pos;
        int    entIdx;
        int    priority;   // 0 = money, 1 = printer, 2 = weapon, 3 = misc
        float  dist;
    };

    inline float      farmRadius     = 500.f;
    inline float      farmTimer      = 0.f;
    inline float      farmInterval   = 0.5f;
    inline bool       farmMoney      = true;
    inline bool       farmItems      = true;
    inline bool       farmPrinters   = false;
    inline float      farmSpeed      = 300.f;
    inline float      farmPickupDist = 100.f;

    inline FarmTarget farmTargets[32]{};
    inline int        farmTargetCount = 0;
    inline int        farmCurrentTarget = -1;     // index into farmTargets
    inline float      farmScanTimer   = 0.f;
    inline float      farmScanInterval = 0.5f;
    inline float      farmUseTimer    = 0.f;
    inline float      farmUseCooldown = 0.3f;

    // Simple bubble sort for the small fixed-size target list
    inline void SortFarmTargets() {
        for (int i = 0; i < farmTargetCount - 1; ++i) {
            for (int j = 0; j < farmTargetCount - 1 - i; ++j) {
                bool swap = false;
                // Sort by priority first (lower = higher priority), then by distance
                if (farmTargets[j].priority > farmTargets[j + 1].priority) {
                    swap = true;
                } else if (farmTargets[j].priority == farmTargets[j + 1].priority &&
                           farmTargets[j].dist > farmTargets[j + 1].dist) {
                    swap = true;
                }
                if (swap) {
                    FarmTarget tmp    = farmTargets[j];
                    farmTargets[j]    = farmTargets[j + 1];
                    farmTargets[j + 1] = tmp;
                }
            }
        }
    }

    // Scan nearby entities and build the target list
    inline void FarmScanTargets(C_BasePlayer* lp) {
        farmTargetCount = 0;
        if (!lp || !interfaces::entityList) return;

        Vector ownPos = lp->GetAbsOrigin();
        Vector eyePos = lp->GetEyePosition();

        int maxEnts = interfaces::entityList->GetHighestEntityIndex();
        if (maxEnts <= 0) return;
        if (maxEnts > 2048) maxEnts = 2048;

        for (int i = interfaces::globalVars->maxClients + 1; i <= maxEnts; ++i) {
            if (farmTargetCount >= 32) break;

            C_BasePlayer* bp = (C_BasePlayer*)interfaces::entityList->GetClientEntity(i);
            if (!bp) continue;

            Vector entPos = bp->GetAbsOrigin();
            if (entPos.x == 0.f && entPos.y == 0.f && entPos.z == 0.f) continue;

            float dx = entPos.x - ownPos.x;
            float dy = entPos.y - ownPos.y;
            float dz = entPos.z - ownPos.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);

            if (dist > farmRadius || dist < 1.f) continue;

            // Visibility check
            Vector entCenter = { entPos.x, entPos.y, entPos.z + 16.f };
            if (!TraceVisible(eyePos, entCenter, lp)) continue;

            int priority = 3;
            if (farmMoney && dz < 20.f && dz > -20.f) {
                priority = 0;
            } else if (farmPrinters && dz > 10.f && dz < 60.f) {
                priority = 1;
            } else if (farmItems) {
                priority = 2;
            } else {
                continue;
            }

            farmTargets[farmTargetCount] = { entPos, i, priority, dist };
            farmTargetCount++;
        }

        SortFarmTargets();
        farmCurrentTarget = (farmTargetCount > 0) ? 0 : -1;
    }

    inline void FarmInit() {
        farmTargetCount   = 0;
        farmCurrentTarget = -1;
        farmScanTimer     = 0.f;
        farmUseTimer      = 0.f;
    }

    inline void FarmUpdate(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp || !interfaces::entityList) return;

        float curtime = interfaces::globalVars->curtime;
        Vector ownPos = lp->GetAbsOrigin();

        // Periodic target scan
        if (curtime >= farmScanTimer) {
            farmScanTimer = curtime + farmScanInterval;
            FarmScanTargets(lp);
        }

        // No targets available, stand idle
        if (farmTargetCount < 1 || farmCurrentTarget < 0 || farmCurrentTarget >= farmTargetCount) {
            cmd->forwardmove = 0.f;
            cmd->sidemove    = 0.f;
            return;
        }

        FarmTarget& ft = farmTargets[farmCurrentTarget];

        // Validate entity still exists
        C_BasePlayer* bp = (C_BasePlayer*)interfaces::entityList->GetClientEntity(ft.entIdx);
        if (!bp) {
            farmCurrentTarget++;
            if (farmCurrentTarget >= farmTargetCount)
                farmCurrentTarget = -1;
            return;
        }

        ft.pos = bp->GetAbsOrigin();
        if (ft.pos.x == 0.f && ft.pos.y == 0.f && ft.pos.z == 0.f) {
            farmCurrentTarget++;
            if (farmCurrentTarget >= farmTargetCount)
                farmCurrentTarget = -1;
            return;
        }

        float dx = ft.pos.x - ownPos.x;
        float dy = ft.pos.y - ownPos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < farmPickupDist) {
            cmd->forwardmove = 0.f;
            cmd->sidemove    = 0.f;

            if (curtime >= farmUseTimer) {
                farmUseTimer = curtime + farmUseCooldown;
                cmd->buttons |= CUserCmd::IN_USE;
            }

            farmCurrentTarget++;
            if (farmCurrentTarget >= farmTargetCount)
                farmCurrentTarget = -1;
        } else {
            WalkToward(ownPos, ft.pos, farmSpeed, cmd, lp);
        }
    }

    // ========================================================================
    // FLEE MODE
    // ========================================================================
    inline Vector fleeFrom{};
    inline float  fleeDuration  = 5.f;
    inline float  fleeTimer     = 0.f;
    inline float  fleeSpeed     = 450.f;
    inline float  fleeStartTime = 0.f;

    inline void TriggerFlee(Vector dangerPos) {
        previousMode = currentMode;
        currentMode  = BotMode::Flee;
        fleeFrom     = dangerPos;
        fleeTimer    = 0.f;
        fleeStartTime = interfaces::globalVars->curtime;
    }

    inline void FleeInit() {
        fleeTimer     = 0.f;
        fleeStartTime = interfaces::globalVars->curtime;
    }

    inline void FleeUpdate(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp) return;

        float curtime = interfaces::globalVars->curtime;
        float elapsed = curtime - fleeStartTime;

        if (elapsed >= fleeDuration) {
            currentMode = previousMode;
            return;
        }

        Vector ownPos = lp->GetAbsOrigin();

        float dx = ownPos.x - fleeFrom.x;
        float dy = ownPos.y - fleeFrom.y;
        float dist = sqrtf(dx * dx + dy * dy);
        float fleeYaw;
        if (dist > 1.f)
            fleeYaw = rad2deg(atan2f(dy, dx));
        else
            fleeYaw = 0.f;

        // Use ray-based steering to flee
        Vector fleeTarget = { ownPos.x + cosf(deg2rad(fleeYaw)) * 500.f,
                              ownPos.y + sinf(deg2rad(fleeYaw)) * 500.f,
                              ownPos.z };
        WalkToward(ownPos, fleeTarget, fleeSpeed, cmd, lp);
        cmd->buttons |= CUserCmd::IN_SPEED;
    }

    // ========================================================================
    // Mode transitions
    // ========================================================================
    // forward declarations of inits above allow SetMode to call them
    inline void SetMode(BotMode mode) {
        if (mode == currentMode) return;

        previousMode = currentMode;
        currentMode  = mode;

        // Per-mode initialization
        switch (mode) {
            case BotMode::Idle:   IdleInit();       break;
            case BotMode::Follow: /* handled by follow_bot externally */ break;
            case BotMode::Guard:  GuardInit(nullptr); break;
            case BotMode::Patrol: PatrolInit();     break;
            case BotMode::Farm:   FarmInit();       break;
            case BotMode::Flee:   FleeInit();       break;
        }
    }

    // Overload that accepts a local player pointer for modes that need it
    inline void SetMode(BotMode mode, C_BasePlayer* lp) {
        if (mode == currentMode) return;

        previousMode = currentMode;
        currentMode  = mode;

        switch (mode) {
            case BotMode::Idle:   IdleInit();       break;
            case BotMode::Follow: break;
            case BotMode::Guard:  GuardInit(lp);    break;
            case BotMode::Patrol: PatrolInit();     break;
            case BotMode::Farm:   FarmInit();       break;
            case BotMode::Flee:   FleeInit();       break;
        }
    }

    // ========================================================================
    // Main Update  -- called from CreateMove each tick
    // ========================================================================
    inline void Update(CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp) return;
        if (!interfaces::trace) return;
        if (!interfaces::engine->IsInGame()) return;

        switch (currentMode) {
            case BotMode::Idle:
                IdleUpdate(cmd, lp);
                break;

            case BotMode::Follow:
                // Follow mode is driven by follow_bot::Update externally.
                // This entry is here so the state machine is aware of it,
                // but movement is handled by the existing follow_bot namespace.
                break;

            case BotMode::Guard:
                GuardUpdate(cmd, lp);
                break;

            case BotMode::Patrol:
                PatrolUpdate(cmd, lp);
                break;

            case BotMode::Farm:
                FarmUpdate(cmd, lp);
                break;

            case BotMode::Flee:
                FleeUpdate(cmd, lp);
                break;
        }
    }

    // ========================================================================
    // Reset -- clear all state
    // ========================================================================
    inline void Reset() {
        currentMode  = BotMode::Idle;
        previousMode = BotMode::Idle;

        // Idle
        idleLookTimer    = 0.f;
        idleLookTarget   = Angle(0.f, 0.f, 0.f);

        // Guard
        guardPos         = Vector(0.f, 0.f, 0.f);
        guardRadius      = 300.f;
        guardReturnDist  = 50.f;
        guardSpeed       = 250.f;
        guardThreatIdx   = -1;

        // Patrol
        ClearPatrol();

        // Farm
        farmRadius       = 500.f;
        farmTimer        = 0.f;
        farmInterval     = 0.5f;
        farmMoney        = true;
        farmItems        = true;
        farmPrinters     = false;
        FarmInit();

        // Flee
        fleeFrom         = Vector(0.f, 0.f, 0.f);
        fleeDuration     = 5.f;
        fleeTimer        = 0.f;
        fleeSpeed        = 450.f;
        fleeStartTime    = 0.f;
    }

} // namespace bot_tasks
