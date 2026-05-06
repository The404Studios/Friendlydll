#pragma once
#include "../includes.hpp"

// ---------------------------------------------------------------------------
// combat.hpp  --  Inline combat helpers called from the CreateMove hook
// ---------------------------------------------------------------------------

// ---- Backtrack find-target result -----------------------------------------
struct BacktrackResult {
	int   entityIdx = -1;
	int   tickIdx   = -1;
	Vector aimPos{};
	float fov       = 999.f;
	float score     = -1.f;
	bool  found     = false;
	bool  visible   = false;
};

// ---------------------------------------------------------------------------
// 1.  Triggerbot
//     Traces along the current viewangle direction.  If the crosshair is
//     over a valid enemy, force IN_ATTACK.
// ---------------------------------------------------------------------------
inline void Triggerbot(CUserCmd* cmd, C_BasePlayer* lp)
{
	static DWORD s_triggerStart = 0;
	static bool  s_triggerPending = false;

	if (!config::triggerbot) { s_triggerPending = false; return; }
	if (!lp)                { s_triggerPending = false; return; }

	// Key check -- 0 means "always on"
	int key = config::triggerbot_key;
	if (key != 0) {
		if (!(GetAsyncKeyState(key) & 0x8000))
			return;
	}

	Vector forward = cmd->viewangles.Forward();
	Vector right = cmd->viewangles.Right();
	Vector up = cmd->viewangles.Up();

	Vector eyePos = lp->GetEyePosition();

	// Multi-point: trace center + 4 slight offsets for better hitbox coverage
	CTrace trace;
	TraceFilterSimple filter(lp);
	bool hitPlayer = false;

	const float offsets[] = { 0.f, 0.5f, -0.5f };
	for (int ox = 0; ox < 3 && !hitPlayer; ox++) {
		for (int oy = 0; oy < 3 && !hitPlayer; oy++) {
			Vector dir = forward + right * (offsets[ox] * 0.015f) + up * (offsets[oy] * 0.015f);
			float len = dir.Length();
			if (len > 0.f) { dir.x /= len; dir.y /= len; dir.z /= len; }

			Vector endPos = eyePos + dir * 8192.f;
			Ray_t ray;
			ray.Init(eyePos, endPos);
			interfaces::trace->TraceRay(ray, MASK_SHOT, &filter, &trace);

			if (trace.entity && trace.entity != lp &&
				trace.entity->IsPlayer() && trace.entity->IsAlive()) {
				hitPlayer = true;
			}
		}
	}

	if (!hitPlayer) { s_triggerPending = false; return; }

	if (config::triggerbot_head_only && trace.hitgroup != 1) { s_triggerPending = false; return; }

	// Figure out the entity index for the target-allowed check
	for (int i = 1; i <= interfaces::globalVars->maxClients; ++i) {
		C_BasePlayer* ent = (C_BasePlayer*)interfaces::entityList->GetClientEntity(i);
		if (ent == trace.entity) {
			if (!config::IsTargetAllowed(i))
				{ s_triggerPending = false; return; }
			break;
		}
	}

	// Triggerbot delay mechanism
	if (config::triggerbot_delay_ms <= 0) {
		// No delay — fire immediately (original behavior)
		cmd->buttons |= CUserCmd::IN_ATTACK;
		s_triggerPending = false;
	} else {
		if (!s_triggerPending) {
			// First frame we see a valid target — start the timer
			s_triggerStart = GetTickCount();
			s_triggerPending = true;
		} else if (GetTickCount() - s_triggerStart >= (DWORD)config::triggerbot_delay_ms) {
			// Delay elapsed — fire and reset
			cmd->buttons |= CUserCmd::IN_ATTACK;
			s_triggerPending = false;
		}
		// else: still waiting for delay, do nothing
	}
}

// ---------------------------------------------------------------------------
// 2.  Backtrack storage
//     Copies the current bone-write-buffer data into the backtrack ring
//     buffer so we can rewind later.
// ---------------------------------------------------------------------------
inline void BacktrackStore()
{
	if (!config::backtrack) return;

	// Read from the bone buffer that was just written (latest write index)
	const auto& bones = config::BoneRead();

	for (int i = 0; i < 128; ++i) {
		if (!bones[i].valid) continue;
		if (bones[i].noBones) continue;

		auto& tick = config::g_btBuf[i][config::g_btHead];
		memcpy(tick.bones, bones[i].bones, sizeof(tick.bones));
		tick.origin  = bones[i].absOrigin;
		tick.simtime = interfaces::globalVars->curtime;
		tick.valid   = true;
	}

	config::g_btHead = (config::g_btHead + 1) % config::BT_MAX_TICKS;
}

// ---------------------------------------------------------------------------
// 3.  Backtrack find-target
//     Scans all stored backtrack ticks for every player and returns the one
//     closest to the crosshair (lowest FOV).
// ---------------------------------------------------------------------------
inline BacktrackResult BacktrackFindTarget(C_BasePlayer* lp, CUserCmd* cmd)
{
	BacktrackResult best;
	if (!config::backtrack) return best;
	if (!lp)                return best;

	Vector eyePos    = lp->GetEyePosition();
	Angle  viewAngle = cmd->viewangles;

	int ticksToScan = (config::backtrack_ticks < config::BT_MAX_TICKS) ? config::backtrack_ticks : config::BT_MAX_TICKS;
	float curtime = interfaces::globalVars->curtime;

	// Multi-bone: scan head, chest, pelvis for best visible hit
	const int scanBones[] = { Bones::bone_head, 6 /*spine2/chest*/, Bones::bone_pelvis };
	int numScanBones = (config::bone == Bones::bone_head) ? 3 : 1;

	for (int i = 1; i <= interfaces::globalVars->maxClients; ++i) {
		C_BasePlayer* ent = (C_BasePlayer*)interfaces::entityList->GetClientEntity(i);
		if (!ent || ent == lp)     continue;
		if (!ent->IsPlayer())      continue;
		if (!ent->IsAlive())       continue;
		if (!config::IsTargetAllowed(i)) continue;

		for (int t = 0; t < ticksToScan; ++t) {
			const auto& tick = config::g_btBuf[i][t];
			if (!tick.valid) continue;

			// Reject ticks older than 200ms (server won't accept them)
			float age = static_cast<float>(curtime - tick.simtime);
			if (age < 0.f || age > 0.2f) continue;

			for (int bi = 0; bi < numScanBones; bi++) {
				int boneIdx = (numScanBones > 1) ? scanBones[bi] : config::bone;
				if (boneIdx < 0 || boneIdx >= 128) continue;

				Vector bonePos = tick.bones[boneIdx].GetOrigin();
				Angle  toTarget = Angle::FromVector(bonePos - eyePos);

				float dyaw  = normalize_yaw(toTarget.y - viewAngle.y);
				float dpitch = toTarget.p - viewAngle.p;
				float fov   = sqrtf(dyaw * dyaw + dpitch * dpitch);

				if (fov >= config::aimbot_fov) continue;

				// Composite score: FOV (60%) + freshness (25%) + visibility (15%)
				float fovScore = 1.f - (fov / config::aimbot_fov);
				float ageScore = 1.f - (age / 0.2f);
				bool vis = false;
				{
					CTrace vtr; TraceFilterSimple vflt(lp);
					Ray_t vray; vray.Init(eyePos, bonePos);
					interfaces::trace->TraceRay(vray, MASK_SHOT, &vflt, &vtr);
					vis = (vtr.entity == ent || vtr.fraction >= 0.98f);
				}
				float visScore = vis ? 1.f : 0.2f;

				float score = fovScore * 0.6f + ageScore * 0.25f + visScore * 0.15f;

				if (score > best.score) {
					best.entityIdx = i;
					best.tickIdx   = t;
					best.aimPos    = bonePos;
					best.fov       = fov;
					best.score     = score;
					best.found     = true;
					best.visible   = vis;
				}
			}
		}
	}

	return best;
}

// ---------------------------------------------------------------------------
// 4.  View-punch removal
//     Subtracts the engine's view-punch from the command angles so the
//     crosshair stays steady while firing.
// ---------------------------------------------------------------------------
inline void RemoveViewPunch(CUserCmd* cmd, C_BasePlayer* lp)
{
	if (!config::viewpunch_remove) return;
	if (!lp)                       return;

	const Angle& punch = lp->GetViewPunch();
	cmd->viewangles.p -= punch.p * 2.f;
	cmd->viewangles.y -= punch.y * 2.f;
}

