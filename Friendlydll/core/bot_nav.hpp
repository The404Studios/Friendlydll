#pragma once
#include "../includes.hpp"

namespace bot_nav {

    // =========================================================================
    //  Constants
    // =========================================================================
    static constexpr int   MAX_NODES        = 512;
    static constexpr int   MAX_CONNECTIONS  = 8;
    static constexpr int   MAX_PATH_NODES   = 128;
    static constexpr int   MAX_ASTAR_ITERS  = 100;
    static constexpr float NODE_SPACING     = 100.f;   // min distance between recorded nodes
    static constexpr float NODE_CONNECT_DIST= 400.f;   // max distance to link two nodes
    static constexpr float STEP_HEIGHT      = 18.f;    // Source engine step height
    static constexpr float PLAYER_HEIGHT    = 72.f;
    static constexpr float CROUCH_HEIGHT    = 36.f;
    static constexpr float FALL_SAFE_DIST   = 256.f;   // first 256 units free
    static constexpr float FALL_DMG_PER_UNIT= 10.f / 256.f; // 10 damage per 256 units after safe
    static constexpr float LETHAL_FALL      = 580.f;   // ~100hp worth of fall + safe zone

    // =========================================================================
    //  Ground type enum
    // =========================================================================
    enum GroundType : int {
        GROUND_NORMAL = 0,
        GROUND_WATER,
        GROUND_LADDER
    };

    // =========================================================================
    //  Terrain analysis result
    // =========================================================================
    struct GroundInfo {
        bool  isGround;
        bool  isSlope;
        float slopeAngle;
        bool  isWater;
        bool  isLadder;
        float groundZ;
    };

    // =========================================================================
    //  Stair detection result
    // =========================================================================
    struct StairInfo {
        bool  detected;
        float totalHeight;   // accumulated step height
        int   stepCount;
        float dirX, dirY;    // stair direction (normalized)
    };

    // =========================================================================
    //  Stuck recovery
    // =========================================================================
    enum StuckStrategy : int {
        STUCK_NONE = 0,
        STUCK_STRAFE_LEFT,
        STUCK_STRAFE_RIGHT,
        STUCK_JUMP_STRAFE_LEFT,
        STUCK_JUMP_STRAFE_RIGHT,
        STUCK_BACKPEDAL,
        STUCK_DUCK_MOVE,
        STUCK_RANDOM_BURST,
        STUCK_NOCLIP_SPAM,
        STUCK_MAX
    };

    struct StuckState {
        bool           isStuck;
        float          stuckTime;        // how long we have been stuck
        float          strategyTimer;    // time on current strategy
        StuckStrategy  strategy;
        Vector         lastPos;
        float          lastMoveCheck;
        float          randomYaw;        // for random burst
        int            spamCounter;      // for noclip spam
    };

    // =========================================================================
    //  Nav node
    // =========================================================================
    struct NavNode {
        Vector     pos;
        GroundType type;
        int        connections[MAX_CONNECTIONS]; // indices into g_nodes, -1 = unused
        int        numConnections;
        float      timestamp;                   // curtime when recorded
        bool       active;
    };

    // =========================================================================
    //  A* open-list entry
    // =========================================================================
    struct AStarEntry {
        int   nodeIdx;
        int   parentIdx;
        float gCost;
        float fCost;
    };

    // =========================================================================
    //  Inline globals (persist across frames)
    // =========================================================================
    inline NavNode   g_nodes[MAX_NODES]{};
    inline int       g_nodeHead  = 0;   // ring buffer write cursor
    inline int       g_nodeCount = 0;
    inline StuckState g_stuck{};

    // =========================================================================
    //  Trace helpers (match follow_bot conventions)
    // =========================================================================
    inline bool TraceVis(Vector from, Vector to, C_BasePlayer* skip) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        ray.Init(from, to);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        return trace.fraction > 0.97f;
    }

    inline bool TraceDown(Vector pos, float depth, C_BasePlayer* skip,
                          float* outZ, int* outContents = nullptr) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        Vector down = { pos.x, pos.y, pos.z - depth };
        ray.Init(pos, down);
        interfaces::trace->TraceRay(ray, MASK_ALL, &filter, &trace);
        if (outZ) *outZ = trace.endPos.z;
        if (outContents) *outContents = trace.contents;
        return trace.fraction < 1.f;
    }

    inline bool TraceLine(Vector from, Vector to, C_BasePlayer* skip,
                          float* outFrac, int* outContents = nullptr) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        ray.Init(from, to);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        if (outFrac) *outFrac = trace.fraction;
        if (outContents) *outContents = trace.contents;
        return trace.fraction < 1.f;
    }

    inline bool TraceHull(Vector from, Vector to, Vector mins, Vector maxs,
                          C_BasePlayer* skip, float* outFrac) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray(from, to, mins, maxs);
        interfaces::trace->TraceRay(ray, MASK_PLAYERSOLID, &filter, &trace);
        if (outFrac) *outFrac = trace.fraction;
        return trace.fraction < 1.f;
    }

    // =========================================================================
    //  1. Terrain Analysis
    // =========================================================================
    inline GroundInfo AnalyzeGround(Vector pos, C_BasePlayer* skip) {
        GroundInfo info{};
        info.isGround  = false;
        info.isSlope   = false;
        info.slopeAngle= 0.f;
        info.isWater   = false;
        info.isLadder  = false;
        info.groundZ   = pos.z;

        if (!interfaces::trace) return info;

        // Trace straight down for ground
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        Vector down = { pos.x, pos.y, pos.z - 200.f };
        ray.Init(pos, down);
        interfaces::trace->TraceRay(ray, MASK_ALL, &filter, &trace);

        if (trace.fraction < 1.f) {
            info.isGround = true;
            info.groundZ  = trace.endPos.z;

            // Slope: check plane normal deviation from vertical
            float dotUp = trace.plane.normal.z; // 1.0 = flat, 0.0 = vertical wall
            if (dotUp < 0.98f && dotUp > 0.1f) {
                info.isSlope   = true;
                info.slopeAngle = rad2deg(acosf(dotUp));
            }
        }

        // Check for water at this position
        CTrace waterTrace;
        TraceFilterSimple waterFilter(skip);
        Ray_t waterRay;
        Vector waterUp = { pos.x, pos.y, pos.z + 1.f };
        waterRay.Init(pos, waterUp);
        interfaces::trace->TraceRay(waterRay, CONTENTS_WATER, &waterFilter, &waterTrace);
        if (waterTrace.startSolid || (waterTrace.contents & CONTENTS_WATER)) {
            info.isWater = true;
        }

        // Check for ladder
        CTrace ladderTrace;
        TraceFilterSimple ladderFilter(skip);
        Ray_t ladderRay;
        Vector ladderUp = { pos.x, pos.y, pos.z + 1.f };
        ladderRay.Init(pos, ladderUp);
        interfaces::trace->TraceRay(ladderRay, MASK_ALL, &ladderFilter, &ladderTrace);
        if (ladderTrace.contents & CONTENTS_LADDER) {
            info.isLadder = true;
        }

        return info;
    }

    inline float CheckFallDamage(Vector from, Vector to, C_BasePlayer* skip) {
        if (!interfaces::trace) return 0.f;

        // Trace down from the destination to find the ground
        float groundZ = to.z;
        bool hasGround = TraceDown(to, 4096.f, skip, &groundZ);
        if (!hasGround) return 9999.f; // bottomless pit

        float fallHeight = to.z - groundZ;
        if (fallHeight <= 0.f) return 0.f;

        // Source engine: first 256 units are safe, then 10 damage per 256 units
        if (fallHeight <= FALL_SAFE_DIST) return 0.f;
        return (fallHeight - FALL_SAFE_DIST) * FALL_DMG_PER_UNIT;
    }

    inline Vector FindSafeDropPoint(Vector pos, Vector dir, float maxDist,
                                    C_BasePlayer* skip) {
        if (!interfaces::trace) return pos;

        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.001f) return pos;
        float nx = dir.x / len;
        float ny = dir.y / len;

        Vector bestPoint = pos;
        float  stepSize  = 20.f;

        for (float d = stepSize; d <= maxDist; d += stepSize) {
            Vector test = { pos.x + nx * d, pos.y + ny * d, pos.z };

            // Find ground below this point
            float groundZ = test.z;
            bool hasGround = TraceDown(test, 4096.f, skip, &groundZ);
            if (!hasGround) break; // no ground, stop

            float fallH = test.z - groundZ;
            if (fallH > LETHAL_FALL) break; // lethal, stop before this point

            // Check we can walk to this point (no wall)
            float frac = 0.f;
            Vector waist = { pos.x, pos.y, pos.z + 36.f };
            Vector testWaist = { test.x, test.y, test.z + 36.f };
            TraceLine(waist, testWaist, skip, &frac);
            if (frac < 0.9f) break; // wall in the way

            bestPoint = test;
            bestPoint.z = groundZ; // snap to ground
        }

        return bestPoint;
    }

    inline StairInfo DetectStairs(Vector pos, Vector dir, C_BasePlayer* skip) {
        StairInfo info{};
        info.detected    = false;
        info.totalHeight = 0.f;
        info.stepCount   = 0;
        info.dirX        = dir.x;
        info.dirY        = dir.y;

        if (!interfaces::trace) return info;

        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.001f) return info;
        float nx = dir.x / len;
        float ny = dir.y / len;

        float prevZ = pos.z;

        // Sample ground heights ahead in small steps
        for (int i = 1; i <= 8; i++) {
            float d = (float)i * 24.f; // 24 units per sample
            Vector sample = { pos.x + nx * d, pos.y + ny * d, pos.z + 80.f };

            float groundZ = 0.f;
            bool hasGround = TraceDown(sample, 160.f, skip, &groundZ);
            if (!hasGround) break;

            float stepUp = groundZ - prevZ;

            // A stair step is typically 8-18 units up
            if (stepUp >= 4.f && stepUp <= STEP_HEIGHT + 2.f) {
                info.stepCount++;
                info.totalHeight += stepUp;
            } else if (stepUp < -4.f) {
                break; // going down, not stairs up
            }

            prevZ = groundZ;
        }

        // Need at least 2 sequential step-ups to call it stairs
        if (info.stepCount >= 2) {
            info.detected = true;
        }

        return info;
    }

    // =========================================================================
    //  2. Water / Swimming Handler
    // =========================================================================
    inline bool IsInWater(Vector pos, C_BasePlayer* skip) {
        if (!interfaces::trace) return false;

        // Point-trace at position to check for water contents
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        Vector end = { pos.x, pos.y, pos.z + 1.f };
        ray.Init(pos, end);
        interfaces::trace->TraceRay(ray, CONTENTS_WATER, &filter, &trace);
        return trace.startSolid || (trace.contents & CONTENTS_WATER);
    }

    inline void SwimToward(Vector target, CUserCmd* cmd, C_BasePlayer* lp) {
        if (!lp) return;

        Vector ownPos = lp->GetAbsOrigin();
        float dx = target.x - ownPos.x;
        float dy = target.y - ownPos.y;
        float dz = target.z - ownPos.z;
        float dist2D = sqrtf(dx * dx + dy * dy);

        // Swim direction
        float dirYaw = rad2deg(atan2f(dy, dx));
        float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));

        float speed = 300.f; // water movement speed
        cmd->forwardmove = cosf(delta) * speed;
        cmd->sidemove    = -sinf(delta) * speed;

        // If target is above us, hold jump to swim up
        if (dz > 10.f) {
            cmd->buttons |= CUserCmd::IN_JUMP;
        }
        // If target is below and we are at the surface, duck to dive
        else if (dz < -30.f) {
            cmd->buttons |= CUserCmd::IN_DUCK;
        }

        // Hold jump periodically to keep treading water / ascending
        // We alternate on a frame counter to avoid the engine eating jumps
        if (cmd->command_number % 4 < 2) {
            cmd->buttons |= CUserCmd::IN_JUMP;
        }
    }

    // =========================================================================
    //  3. Navigation Graph
    // =========================================================================
    inline GroundType ClassifyGround(Vector pos, C_BasePlayer* skip) {
        if (IsInWater(pos, skip)) return GROUND_WATER;

        // Check ladder at position
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        Vector end = { pos.x, pos.y, pos.z + 2.f };
        ray.Init(pos, end);
        interfaces::trace->TraceRay(ray, MASK_ALL, &filter, &trace);
        if (trace.contents & CONTENTS_LADDER) return GROUND_LADDER;

        return GROUND_NORMAL;
    }

    inline int FindNearestNode(Vector pos, float maxDist = 999999.f) {
        int   best     = -1;
        float bestDist = maxDist * maxDist;

        int count = (g_nodeCount < MAX_NODES) ? g_nodeCount : MAX_NODES;
        for (int i = 0; i < count; i++) {
            if (!g_nodes[i].active) continue;
            float dx = g_nodes[i].pos.x - pos.x;
            float dy = g_nodes[i].pos.y - pos.y;
            float dz = g_nodes[i].pos.z - pos.z;
            float dsq = dx * dx + dy * dy + dz * dz;
            if (dsq < bestDist) {
                bestDist = dsq;
                best = i;
            }
        }
        return best;
    }

    inline void ConnectNode(int idx, C_BasePlayer* lp) {
        NavNode& node = g_nodes[idx];
        node.numConnections = 0;

        int count = (g_nodeCount < MAX_NODES) ? g_nodeCount : MAX_NODES;
        for (int i = 0; i < count && node.numConnections < MAX_CONNECTIONS; i++) {
            if (i == idx) continue;
            if (!g_nodes[i].active) continue;

            float dx = g_nodes[i].pos.x - node.pos.x;
            float dy = g_nodes[i].pos.y - node.pos.y;
            float dz = g_nodes[i].pos.z - node.pos.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);

            if (dist > NODE_CONNECT_DIST) continue;

            // Check walkable visibility between nodes (waist height)
            Vector from = { node.pos.x, node.pos.y, node.pos.z + 36.f };
            Vector to   = { g_nodes[i].pos.x, g_nodes[i].pos.y, g_nodes[i].pos.z + 36.f };
            if (TraceVis(from, to, lp)) {
                node.connections[node.numConnections++] = i;
            }
        }
    }

    inline void RecordNode(Vector pos, C_BasePlayer* lp) {
        if (!interfaces::trace) return;

        // Don't place a node too close to any existing node
        int nearest = FindNearestNode(pos, NODE_SPACING);
        if (nearest >= 0) return;

        // Write into ring buffer
        int idx = g_nodeHead;
        NavNode& node = g_nodes[idx];

        // If overwriting an active node, clear connections pointing to it
        if (node.active) {
            int count = (g_nodeCount < MAX_NODES) ? g_nodeCount : MAX_NODES;
            for (int i = 0; i < count; i++) {
                if (i == idx || !g_nodes[i].active) continue;
                for (int c = 0; c < g_nodes[i].numConnections; c++) {
                    if (g_nodes[i].connections[c] == idx) {
                        // Shift remaining connections down
                        for (int k = c; k < g_nodes[i].numConnections - 1; k++) {
                            g_nodes[i].connections[k] = g_nodes[i].connections[k + 1];
                        }
                        g_nodes[i].numConnections--;
                        c--; // re-check this slot
                    }
                }
            }
        }

        node.pos     = pos;
        node.type    = ClassifyGround(pos, lp);
        node.active  = true;
        node.timestamp = interfaces::globalVars ? interfaces::globalVars->curtime : 0.f;
        node.numConnections = 0;
        for (int i = 0; i < MAX_CONNECTIONS; i++) node.connections[i] = -1;

        g_nodeHead = (g_nodeHead + 1) % MAX_NODES;
        if (g_nodeCount < MAX_NODES) g_nodeCount++;

        // Build connections for this new node
        ConnectNode(idx, lp);

        // Also add this node as a connection to its newly-connected neighbors
        for (int c = 0; c < node.numConnections; c++) {
            int neighbor = node.connections[c];
            if (neighbor < 0 || !g_nodes[neighbor].active) continue;
            NavNode& nb = g_nodes[neighbor];
            if (nb.numConnections < MAX_CONNECTIONS) {
                // Check it isn't already connected
                bool already = false;
                for (int k = 0; k < nb.numConnections; k++) {
                    if (nb.connections[k] == idx) { already = true; break; }
                }
                if (!already) {
                    nb.connections[nb.numConnections++] = idx;
                }
            }
        }
    }

    // A* pathfinding through the nav graph
    inline std::vector<Vector> FindPath(Vector start, Vector end, C_BasePlayer* lp) {
        std::vector<Vector> result;
        if (!interfaces::trace) return result;

        // Direct visibility? Skip graph entirely
        Vector startWaist = { start.x, start.y, start.z + 36.f };
        Vector endWaist   = { end.x, end.y, end.z + 36.f };
        if (TraceVis(startWaist, endWaist, lp)) {
            result.push_back(end);
            return result;
        }

        // Find nearest nodes to start and end
        int startNode = FindNearestNode(start, NODE_CONNECT_DIST);
        int endNode   = FindNearestNode(end,   NODE_CONNECT_DIST);
        if (startNode < 0 || endNode < 0) return result;
        if (startNode == endNode) {
            result.push_back(g_nodes[startNode].pos);
            result.push_back(end);
            return result;
        }

        // A* with fixed-size arrays to avoid heap churn
        // openList: sorted by fCost (simple insertion sort, bounded)
        AStarEntry openList[MAX_PATH_NODES];
        int openCount = 0;

        // Per-node bookkeeping (only for nodes in [0, nodeCount))
        int   nodeCount = (g_nodeCount < MAX_NODES) ? g_nodeCount : MAX_NODES;
        // We use parallel arrays to avoid allocations
        // closed[i] = true when node i has been finalized
        // gCost[i]  = best known cost to reach node i
        // parent[i] = parent node index in the path (-1 = none)
        bool  closed[MAX_NODES]{};
        float gCost[MAX_NODES];
        int   parent[MAX_NODES];
        for (int i = 0; i < MAX_NODES; i++) {
            gCost[i]  = 999999.f;
            parent[i] = -1;
        }

        // Heuristic: 3D euclidean distance to end node
        auto heuristic = [&](int idx) -> float {
            float dx = g_nodes[idx].pos.x - g_nodes[endNode].pos.x;
            float dy = g_nodes[idx].pos.y - g_nodes[endNode].pos.y;
            float dz = g_nodes[idx].pos.z - g_nodes[endNode].pos.z;
            return sqrtf(dx * dx + dy * dy + dz * dz);
        };

        // Insert into open list (sorted ascending by fCost)
        auto openInsert = [&](int nodeIdx, int par, float g, float f) {
            if (openCount >= MAX_PATH_NODES) return;
            // Find insertion point
            int insertAt = openCount;
            for (int i = 0; i < openCount; i++) {
                if (f < openList[i].fCost) {
                    insertAt = i;
                    break;
                }
            }
            // Shift elements right
            for (int i = openCount; i > insertAt; i--) {
                openList[i] = openList[i - 1];
            }
            openList[insertAt] = { nodeIdx, par, g, f };
            openCount++;
        };

        // Seed with start node
        gCost[startNode] = 0.f;
        openInsert(startNode, -1, 0.f, heuristic(startNode));

        int iterations = 0;
        bool found = false;

        while (openCount > 0 && iterations < MAX_ASTAR_ITERS) {
            iterations++;

            // Pop the node with lowest fCost (first element)
            AStarEntry current = openList[0];
            for (int i = 0; i < openCount - 1; i++) {
                openList[i] = openList[i + 1];
            }
            openCount--;

            int curIdx = current.nodeIdx;
            if (closed[curIdx]) continue;
            closed[curIdx] = true;
            parent[curIdx] = current.parentIdx;

            // Reached goal?
            if (curIdx == endNode) {
                found = true;
                break;
            }

            // Expand neighbors
            const NavNode& curNode = g_nodes[curIdx];
            for (int c = 0; c < curNode.numConnections; c++) {
                int nbIdx = curNode.connections[c];
                if (nbIdx < 0 || nbIdx >= nodeCount) continue;
                if (!g_nodes[nbIdx].active) continue;
                if (closed[nbIdx]) continue;

                float dx = g_nodes[nbIdx].pos.x - curNode.pos.x;
                float dy = g_nodes[nbIdx].pos.y - curNode.pos.y;
                float dz = g_nodes[nbIdx].pos.z - curNode.pos.z;
                float edgeCost = sqrtf(dx * dx + dy * dy + dz * dz);

                // Penalize water and ladder edges slightly
                if (g_nodes[nbIdx].type == GROUND_WATER)  edgeCost *= 1.5f;
                if (g_nodes[nbIdx].type == GROUND_LADDER) edgeCost *= 1.2f;

                float tentG = current.gCost + edgeCost;
                if (tentG < gCost[nbIdx]) {
                    gCost[nbIdx] = tentG;
                    float f = tentG + heuristic(nbIdx);
                    openInsert(nbIdx, curIdx, tentG, f);
                }
            }
        }

        if (!found) return result; // no path

        // Reconstruct path from endNode back to startNode
        int pathIndices[MAX_PATH_NODES];
        int pathLen = 0;
        int cur = endNode;
        while (cur != -1 && pathLen < MAX_PATH_NODES) {
            pathIndices[pathLen++] = cur;
            cur = parent[cur];
        }

        // Reverse into result
        result.reserve(pathLen + 1);
        for (int i = pathLen - 1; i >= 0; i--) {
            result.push_back(g_nodes[pathIndices[i]].pos);
        }
        result.push_back(end); // final destination

        return result;
    }

    // =========================================================================
    //  4. Path Smoother
    // =========================================================================
    inline void SmoothPath(std::vector<Vector>& path, C_BasePlayer* skip) {
        if (!interfaces::trace || path.size() < 3) return;

        // Iteratively remove unnecessary waypoints if direct visibility exists
        // between the node before and after the candidate
        size_t i = 0;
        while (i + 2 < path.size()) {
            Vector from = { path[i].x, path[i].y, path[i].z + 36.f };
            Vector to   = { path[i + 2].x, path[i + 2].y, path[i + 2].z + 36.f };

            if (TraceVis(from, to, skip)) {
                // Also check at feet height to ensure the floor is walkable
                Vector fromFeet = { path[i].x, path[i].y, path[i].z + 4.f };
                Vector toFeet   = { path[i + 2].x, path[i + 2].y, path[i + 2].z + 4.f };

                if (TraceVis(fromFeet, toFeet, skip)) {
                    path.erase(path.begin() + i + 1);
                    continue; // re-check from same index
                }
            }
            i++;
        }
    }

    // =========================================================================
    //  5. Smart Stuck Recovery
    // =========================================================================
    inline void ResetStuck() {
        g_stuck.isStuck       = false;
        g_stuck.stuckTime     = 0.f;
        g_stuck.strategyTimer = 0.f;
        g_stuck.strategy      = STUCK_NONE;
        g_stuck.lastPos       = {};
        g_stuck.lastMoveCheck = 0.f;
        g_stuck.randomYaw     = 0.f;
        g_stuck.spamCounter   = 0;
    }

    // Time to spend on each escalating strategy before moving to the next
    inline float StrategyDuration(StuckStrategy s) {
        switch (s) {
            case STUCK_STRAFE_LEFT:       return 0.5f;
            case STUCK_STRAFE_RIGHT:      return 0.5f;
            case STUCK_JUMP_STRAFE_LEFT:  return 0.6f;
            case STUCK_JUMP_STRAFE_RIGHT: return 0.6f;
            case STUCK_BACKPEDAL:         return 0.5f;
            case STUCK_DUCK_MOVE:         return 0.5f;
            case STUCK_RANDOM_BURST:      return 0.8f;
            case STUCK_NOCLIP_SPAM:       return 1.0f;
            default:                      return 0.5f;
        }
    }

    // Returns true if we are stuck
    inline bool UpdateStuck(Vector pos, float dt) {
        float dx = pos.x - g_stuck.lastPos.x;
        float dy = pos.y - g_stuck.lastPos.y;
        float moved = sqrtf(dx * dx + dy * dy);

        g_stuck.lastPos = pos;

        if (moved < 2.f) {
            g_stuck.stuckTime += dt;
        } else {
            // We moved, clear stuck state
            if (g_stuck.stuckTime > 0.f) {
                g_stuck.stuckTime     = 0.f;
                g_stuck.strategyTimer = 0.f;
                g_stuck.strategy      = STUCK_NONE;
                g_stuck.spamCounter   = 0;
            }
            g_stuck.isStuck = false;
            return false;
        }

        // Consider stuck after 0.4 seconds of no movement
        if (g_stuck.stuckTime < 0.4f) {
            g_stuck.isStuck = false;
            return false;
        }

        g_stuck.isStuck = true;
        g_stuck.strategyTimer += dt;

        // Start with first strategy
        if (g_stuck.strategy == STUCK_NONE) {
            g_stuck.strategy      = STUCK_STRAFE_LEFT;
            g_stuck.strategyTimer = 0.f;
            // Generate a random yaw for later random burst
            g_stuck.randomYaw = (float)(g_stuck.spamCounter * 73 % 360);
        }

        // Escalate if current strategy has expired
        float duration = StrategyDuration(g_stuck.strategy);
        if (g_stuck.strategyTimer >= duration) {
            g_stuck.strategyTimer = 0.f;
            int next = (int)g_stuck.strategy + 1;
            if (next >= STUCK_MAX) {
                // Wrap around to start, but increment spam counter
                next = (int)STUCK_STRAFE_LEFT;
                g_stuck.spamCounter++;
                g_stuck.randomYaw = (float)((g_stuck.spamCounter * 137 + 53) % 360);
            }
            g_stuck.strategy = (StuckStrategy)next;
        }

        return true;
    }

    inline void ApplyUnstuck(CUserCmd* cmd, Vector moveDir) {
        if (!g_stuck.isStuck || g_stuck.strategy == STUCK_NONE) return;

        float speed = 450.f;

        // Compute perpendicular directions relative to moveDir
        // left = rotate moveDir 90 degrees CCW
        // right = rotate moveDir 90 degrees CW
        float leftX  = -moveDir.y;
        float leftY  =  moveDir.x;

        switch (g_stuck.strategy) {
            case STUCK_STRAFE_LEFT: {
                Vector dir = { leftX, leftY, 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_STRAFE_RIGHT: {
                Vector dir = { -leftX, -leftY, 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_JUMP_STRAFE_LEFT: {
                cmd->buttons |= CUserCmd::IN_JUMP;
                Vector dir = { leftX, leftY, 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_JUMP_STRAFE_RIGHT: {
                cmd->buttons |= CUserCmd::IN_JUMP;
                Vector dir = { -leftX, -leftY, 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_BACKPEDAL: {
                Vector dir = { -moveDir.x, -moveDir.y, 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_DUCK_MOVE: {
                cmd->buttons |= CUserCmd::IN_DUCK;
                // Move forward while crouching
                float dirYaw = rad2deg(atan2f(moveDir.y, moveDir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed * 0.5f;
                cmd->sidemove    = -sinf(delta) * speed * 0.5f;
                // Also try +USE in case we're blocked by a door
                cmd->buttons |= CUserCmd::IN_USE;
                break;
            }
            case STUCK_RANDOM_BURST: {
                cmd->buttons |= CUserCmd::IN_JUMP;
                float yawRad = deg2rad(g_stuck.randomYaw);
                Vector dir = { cosf(yawRad), sinf(yawRad), 0.f };
                float dirYaw = rad2deg(atan2f(dir.y, dir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            case STUCK_NOCLIP_SPAM: {
                // Rapid alternating jump + duck to try to clip through or get unstuck
                if (cmd->command_number % 2 == 0) {
                    cmd->buttons |= CUserCmd::IN_JUMP;
                } else {
                    cmd->buttons |= CUserCmd::IN_DUCK;
                }
                // Also press USE for doors/props
                cmd->buttons |= CUserCmd::IN_USE;
                // Move in the intended direction
                float dirYaw = rad2deg(atan2f(moveDir.y, moveDir.x));
                float delta  = deg2rad(normalize_yaw(dirYaw - cmd->viewangles.y));
                cmd->forwardmove = cosf(delta) * speed;
                cmd->sidemove    = -sinf(delta) * speed;
                break;
            }
            default:
                break;
        }
    }

    // =========================================================================
    //  6. Master Reset
    // =========================================================================
    inline void Reset() {
        // Clear nav graph
        for (int i = 0; i < MAX_NODES; i++) {
            g_nodes[i].active = false;
            g_nodes[i].numConnections = 0;
        }
        g_nodeHead  = 0;
        g_nodeCount = 0;

        // Clear stuck state
        ResetStuck();
    }

} // namespace bot_nav
