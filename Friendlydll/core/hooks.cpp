#include "hooks.hpp"
#include "resolver.hpp"
#include "combat.hpp"
#include "intel_lua.hpp"
#include "stealth.hpp"
#include "lua_scripts.hpp"
#include "sound_esp.hpp"
#include "movement.hpp"
#include "net_panel.hpp"
#include "misc_features.hpp"
#include "prediction.hpp"
#include "death_replay.hpp"
#include "antiaim.hpp"
#include "freecam.hpp"
#include "fakelag.hpp"
#include "aim_lines.hpp"
#include "damage_log.hpp"
#include "door_memory.hpp"
#include "spawn_detect.hpp"
#include "xray.hpp"
#include "waypoints.hpp"
#include "rage_mode.hpp"
#include "printer_monitor.hpp"
#include "threat_radar.hpp"
#include "player_profiler.hpp"
#include "auto_disguise.hpp"
#include "heatmap.hpp"
#include "killfeed.hpp"
#include "tick_exploits.hpp"
#include "voice_exploits.hpp"
#include "follow_bot.hpp"
#include "bot_nav.hpp"
#include "bot_combat.hpp"
#include "bot_tasks.hpp"
#include "../interfaces/cmovehelper.hpp"
#include "../interfaces/cbasecombatweapon.hpp"
#include <sstream>
#include <unordered_map>

using _CreateMove = bool(__thiscall*)(IClientModeNormal*, float, CUserCmd*);
_CreateMove ogCreateMove = nullptr;

using _LuaHook = lua_State * (__cdecl*)(void*, void*);
_LuaHook ogLuaHook = nullptr;

using _ReadPixels = void(__thiscall*)(IMatRenderContext*, int, int, int, int, unsigned char*, ImageFormat);
_ReadPixels ogReadPixels = nullptr;

using _ReadPixelsAndStretch = void(__thiscall*)(IMatRenderContext*, void*, void*, unsigned char*, ImageFormat, int);
_ReadPixelsAndStretch ogReadPixelsAndStretch = nullptr;

using _DrawModelExecute = void(__thiscall*)(CModelRender*, const DrawModelState_t&, const ModelRenderInfo_t&, Matrix3x4*);
_DrawModelExecute ogDrawModelExecute = nullptr;

static float cl_sidespeed = 10000;

CMoveData moveData;

static void FixMovement(CUserCmd* cmd, const Angle& oldAngles) {
	float delta = deg2rad(cmd->viewangles.y - oldAngles.y);
	float cs = cosf(delta), sn = sinf(delta);
	float oldFwd = cmd->forwardmove, oldSide = cmd->sidemove;
	cmd->forwardmove = cs * oldFwd + sn * oldSide;
	cmd->sidemove    = cs * oldSide - sn * oldFwd;
}

bool CanHit(C_BasePlayer* target, Vector from, Vector to)
{
	if (!localPlayer)
		return false;
	if (!target)
		return false;

	CTrace trace;
	TraceFilterSimple filter(localPlayer);

	Ray_t Ray;
	Ray.Init(from, to);

	interfaces::trace->TraceRay(Ray, MASK_SHOT, &filter, &trace);
	return trace.entity == target || trace.fraction >= 0.98f;
}

bool __stdcall detourCreateMove(void*, float flInputSampleTime, CUserCmd* cmd) {
	if (!interfaces::engine || !interfaces::engine->IsInGame()) {
		config::g_inGame.store(false, std::memory_order_release);
		config::luastate = nullptr;
		// Invalidate bone cache so render thread doesn't read stale data
		int wi = 1 - config::g_boneReadIdx.load(std::memory_order_relaxed);
		for (int i = 0; i < 128; ++i) config::g_boneBuffers[wi][i].valid = false;
		config::g_boneReadIdx.store(wi, std::memory_order_release);
		// Invalidate entity ESP
		int ei = 1 - config::g_entReadIdx.load(std::memory_order_relaxed);
		config::g_entCount[ei] = 0;
		config::g_entReadIdx.store(ei, std::memory_order_release);
		return ogCreateMove(interfaces::clientMode, flInputSampleTime, cmd);
	}

	localPlayer = (C_BasePlayer*)interfaces::entityList->GetClientEntity(interfaces::engine->GetLocalPlayer());
	config::g_inGame.store(true, std::memory_order_release);

	if (!config::luastate)
		lualoader::TryGrabLuaState();

	{
		int wi = 1 - config::g_viewReadIdx.load(std::memory_order_relaxed);
		const auto& src = interfaces::engine->WorldToScreenMatrix();
		memcpy(&config::g_viewMatrix[wi], &src, sizeof(VMatrix));
		interfaces::engine->GetScreenSize(config::g_screenW[wi], config::g_screenH[wi]);

		// Extract camera origin from view-projection matrix via Cramer's rule
		const auto& m = config::g_viewMatrix[wi];
		float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], b0 = -m[0][3];
		float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], b1 = -m[1][3];
		float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], b2 = -m[2][3];
		float det = a00*(a11*a22 - a12*a21) - a01*(a10*a22 - a12*a20) + a02*(a10*a21 - a11*a20);
		if (fabsf(det) > 1e-6f) {
			float inv = 1.f / det;
			config::g_cameraOrigin[wi].x = (b0*(a11*a22-a12*a21) - a01*(b1*a22-a12*b2) + a02*(b1*a21-a11*b2)) * inv;
			config::g_cameraOrigin[wi].y = (a00*(b1*a22-a12*b2) - b0*(a10*a22-a12*a20) + a02*(a10*b2-b1*a20)) * inv;
			config::g_cameraOrigin[wi].z = (a00*(a11*b2-b1*a21) - a01*(a10*b2-b1*a20) + b0*(a10*a21-a11*a20)) * inv;
		}

		config::g_viewReadIdx.store(wi, std::memory_order_release);
	}

	const auto result = ogCreateMove(interfaces::clientMode, flInputSampleTime, cmd);
	if (!cmd || !cmd->command_number)
		return result;

	// need flags and is on ground
	if (localPlayer) {
		float oldCurtime = interfaces::globalVars->curtime;
		float oldFrametime = interfaces::globalVars->frametime;

		interfaces::globalVars->curtime = localPlayer->GetTickBase() * interfaces::globalVars->interval_per_tick;
		interfaces::globalVars->frametime = interfaces::globalVars->interval_per_tick;

		config::currentvelocity = localPlayer->GetVelocity().Length();
		movement::EdgeJump(cmd, localPlayer);
		movement::FastStop(cmd, localPlayer);
		if (config::bunnyhop && !(localPlayer->GetFlags() & FL_ONGROUND)){
			cmd->buttons &= ~CUserCmd::IN_JUMP;

			if (config::autostrafe && config::autostrafe_legit) {
				if (cmd->mousedx > 0.f)
					cmd->sidemove = 10000.f;
				else if (cmd->mousedx < 0.f) cmd->sidemove = -10000.f;
			}
			else if (config::autostrafe && config::autostrafe_silent) {
				if (fabsf(cmd->mousedx) > 2.f) {
					cmd->sidemove = (cmd->mousedx < 0.f) ? -cl_sidespeed : cl_sidespeed;
				}

				const float speed2d_sil = localPlayer->GetVelocity().Length2D();
				if (speed2d_sil > 0.5f)
					cmd->forwardmove = std::clamp(5850.f / speed2d_sil, -99999.f, 99999.f);
				else
					cmd->forwardmove = 99999;

				const auto vel = localPlayer->GetVelocity();
				const float y_vel = rad2deg(atan2f(vel.y, vel.x));
				const float diff_ang = normalize_yaw(cmd->viewangles.y - y_vel);

				cmd->sidemove = (diff_ang > 0.0) ? -cl_sidespeed : cl_sidespeed;
				cmd->viewangles.y = normalize_yaw(cmd->viewangles.y - diff_ang);
			}
			else if (config::autostrafe && config::autostrafe_directional) {
				if (fabsf(cmd->mousedx) > 2.f) {
					cmd->sidemove = (cmd->mousedx < 0.f) ? -cl_sidespeed : cl_sidespeed;
				}

				if (GetAsyncKeyState('S') & 0x8000) {
					cmd->viewangles.y -= 180;
				}
				else if (GetAsyncKeyState('A') & 0x8000) {
					cmd->viewangles.y += 90;
				}
				else if (GetAsyncKeyState('D') & 0x8000) {
					cmd->viewangles.y -= 90;
				}

				const float speed2d_dir = localPlayer->GetVelocity().Length2D();
				if (speed2d_dir > 0.5f)
					cmd->forwardmove = std::clamp(5850.f / speed2d_dir, -99999.f, 99999.f);
				else
					cmd->forwardmove = 99999;

				const auto vel = localPlayer->GetVelocity();
				const float y_vel = rad2deg(atan2f(vel.y, vel.x));
				const float diff_ang = normalize_yaw(cmd->viewangles.y - y_vel);

				cmd->sidemove = (diff_ang > 0.0) ? -cl_sidespeed : cl_sidespeed;
				cmd->viewangles.y = normalize_yaw(cmd->viewangles.y - diff_ang);
			}
		}

		// Query DarkRP player + entity data via Lua every ~30 ticks
		static int s_luaQueryTick = 0;
		static std::unordered_map<int, intel_lua::PlayerLuaData> s_luaData;
		if (++s_luaQueryTick >= 30) {
			s_luaQueryTick = 0;
			intel_lua::RunIntelQuery(s_luaData);

			// Compute entity distances from local player
			if (localPlayer) {
				Vector lpPos = localPlayer->GetAbsOrigin();
				int ri = config::g_entReadIdx.load(std::memory_order_acquire);
				int cnt = config::g_entCount[ri];
				for (int ei = 0; ei < cnt; ++ei) {
					auto& ent = config::g_entBuf[ri][ei];
					if (ent.valid)
						ent.distance = (ent.pos - lpPos).Length();
				}
			}
		}

		// Stealth systems
		stealth::PanicCheck();
		stealth::UpdateFullbright();
		misc_features::UpdateThirdperson();
		misc_features::CheckDeathAndChat(localPlayer);
		stealth::StealthInit();

		lualoader::ProcessQueue();
		lualoader::ProcessPendingRuns();

		// Sound ESP + net panel + killfeed updates (Lua queries, throttled)
		static int s_auxQueryTick = 0;
		if (++s_auxQueryTick >= 30) {
			s_auxQueryTick = 0;
			sound_esp::Update();
			net_panel::UpdateNetLog();
			net_panel::UpdateChatLog();
			killfeed::UpdateKillFeed();
			killfeed::UpdateVoiceState();
			voice_exploits::Update();

			if (config::crosshair_info) {
				auto ci_result = luascripts::QueryLuaScript(luascripts::LUA_CROSSHAIR_INFO);
				int wi = 1 - config::g_crosshairInfoIdx.load(std::memory_order_acquire);
				strncpy_s(config::g_crosshairInfoBuf[wi], ci_result.c_str(), 255);
				config::g_crosshairInfoIdx.store(wi, std::memory_order_release);
			}
		}

		// Full radar install/uninstall
		{
			static bool s_radarInstalled = false;
			if (config::full_radar && !s_radarInstalled) {
				luascripts::RunLuaScript(luascripts::LUA_FULLRADAR_SETUP);
				s_radarInstalled = true;
			} else if (!config::full_radar && s_radarInstalled) {
				luascripts::RunLuaScript(luascripts::LUA_FULLRADAR_STOP);
				s_radarInstalled = false;
			}
		}

		// Force PVS refresh via cl_fullupdate
		if (config::force_pvs) {
			static float s_lastFullUpdate = 0.f;
			static int s_dormantNearby = 0;
			float now = interfaces::globalVars->curtime;

			// Count dormant players within 3000 units for aggressive mode
			if (config::pvs_aggressive) {
				s_dormantNearby = 0;
				Vector myPos = localPlayer->GetAbsOrigin();
				for (int di = 1; di <= interfaces::engine->GetMaxClients(); di++) {
					if (di == interfaces::engine->GetLocalPlayer()) continue;
					C_BasePlayer* de = (C_BasePlayer*)interfaces::entityList->GetClientEntity(di);
					if (!de || !de->IsDormant()) continue;
					Vector dpos = de->GetAbsOrigin();
					if (dpos.x == 0.f && dpos.y == 0.f && dpos.z == 0.f) continue;
					float ddist = (dpos - myPos).Length();
					if (ddist < 3000.f) s_dormantNearby++;
				}
			}

			float interval = config::force_pvs_interval;
			if (config::pvs_aggressive && s_dormantNearby > 0)
				interval = 2.f; // rapid refresh when dormant players are close

			if (now - s_lastFullUpdate > interval) {
				s_lastFullUpdate = now;
				interfaces::engine->ClientCmd("cl_fullupdate");
			}
		}

		// Per-tick updates for prediction, death replay, heatmap, damage, spawn detect
		prediction::Update(interfaces::globalVars->curtime);
		death_replay::Update(interfaces::engine->GetLocalPlayer(), interfaces::globalVars->curtime);
		heatmap::Update();
		damage_log::Update(localPlayer, interfaces::globalVars->curtime);
		spawn_detect::Update(interfaces::globalVars->curtime);
		printer_monitor::Update(interfaces::globalVars->curtime);
		xray::Toggle();
		rage_mode::CheckToggle();
		threat_radar::Update(localPlayer->GetEyePosition());
		player_profiler::Update(interfaces::globalVars->curtime);
		auto_disguise::Update();

		// Damage tracker and door memory Lua queries (throttled)
		static int s_dmgQueryTick = 0;
		if (++s_dmgQueryTick >= 60) {
			s_dmgQueryTick = 0;
			if (damage_log::enabled) {
				auto dmgResult = luascripts::QueryLuaScript(luascripts::LUA_DAMAGE_TRACKER_READ);
				if (!dmgResult.empty()) {
					std::istringstream ss(dmgResult);
					std::string line;
					while (std::getline(ss, line)) {
						int amt = 0; float x = 0, y = 0, z = 0, t = 0; int dealt = 0;
						if (sscanf_s(line.c_str(), "%d\t%f\t%f\t%f\t%f\t%d", &amt, &x, &y, &z, &t, &dealt) >= 5) {
							Vector pos = { x, y, z };
							if (dealt)
								damage_log::RecordDamageDealt(amt, pos, interfaces::globalVars->curtime);
						}
					}
				}
			}
			if (door_memory::enabled) {
				auto keyResult = luascripts::QueryLuaScript(luascripts::LUA_KEYPAD_SPY_ENHANCED_READ);
				if (!keyResult.empty()) {
					door_memory::ParseKeypadLog(keyResult);
				}
			}
		}

		// Cache bones on the game thread for the aimbot
		config::dbg_bc_total = 0; config::dbg_bc_null = 0; config::dbg_bc_notplayer = 0;
		config::dbg_bc_dead = 0; config::dbg_bc_dormant = 0; config::dbg_bc_bonefail = 0; config::dbg_bc_ok = 0;
		int writeIdx = 1 - config::g_boneReadIdx.load(std::memory_order_relaxed);
		auto& wb = config::g_boneBuffers[writeIdx];
		for (int ci = 0; ci < 128; ++ci) {
			C_BasePlayer* ct = (C_BasePlayer*)interfaces::entityList->GetClientEntity(ci);
			if (!ct || ct == localPlayer) { wb[ci].valid = false; ++config::dbg_bc_null; continue; }
			++config::dbg_bc_total;
			if (!ct->IsPlayer()) { wb[ci].valid = false; ++config::dbg_bc_notplayer; continue; }
			if (!ct->IsAlive()) { wb[ci].valid = false; ++config::dbg_bc_dead; continue; }
			bool isDormant = ct->IsDormant();
			if (isDormant) { ++config::dbg_bc_dormant; }
			bool ok = ct->SetupBones(wb[ci].bones, 128, 0x7FF00, interfaces::globalVars->curtime);
			wb[ci].noBones = !ok;
			wb[ci].dormant = isDormant;
			wb[ci].absOrigin = ct->GetAbsOrigin();
			wb[ci].health = ct->GetHealth();

			if (!isDormant) {
				config::g_dormantTrack[ci].velocity = ct->GetVelocity();
				config::g_dormantTrack[ci].lastActiveTime = interfaces::globalVars->curtime;
			} else {
				auto& trk = config::g_dormantTrack[ci];
				float dt = interfaces::globalVars->curtime - trk.lastActiveTime;
				if (dt > 0.f && dt < 30.f && trk.velocity.LengthSqr() > 1.f) {
					float fade = 1.f - (dt / 30.f);
					wb[ci].absOrigin = wb[ci].absOrigin + trk.velocity * dt * fade;
				}
			}
			{
				int ri = config::g_viewReadIdx.load(std::memory_order_acquire);
				wb[ci].distance = (wb[ci].absOrigin - config::g_cameraOrigin[ri]).Length();
			}
			player_info_s pinfo{};
			if (interfaces::engine->GetPlayerInfo(ci, &pinfo))
				strncpy_s(wb[ci].name, pinfo.name, 31);
			else
				wb[ci].name[0] = '\0';
			auto it = s_luaData.find(ci);
			if (it != s_luaData.end()) {
				memcpy(wb[ci].rpName, it->second.rpName, 64);
				memcpy(wb[ci].job, it->second.job, 64);
				memcpy(wb[ci].weapon, it->second.weapon, 64);
				memcpy(wb[ci].weaponList, it->second.weaponList, 128);
				memcpy(wb[ci].gang, it->second.gang, 64);
				wb[ci].isAdmin = it->second.isAdmin;
				wb[ci].isSuperAdmin = it->second.isSuperAdmin;
				wb[ci].observerMode = it->second.observerMode;
				wb[ci].observerTarget = it->second.observerTarget;
				wb[ci].money = it->second.money;
				wb[ci].isWanted = it->second.isWanted;
			} else {
				wb[ci].rpName[0] = '\0';
				wb[ci].job[0] = '\0';
				wb[ci].weapon[0] = '\0';
				wb[ci].weaponList[0] = '\0';
				wb[ci].gang[0] = '\0';
				wb[ci].isAdmin = false;
				wb[ci].isSuperAdmin = false;
				wb[ci].observerMode = 0;
				wb[ci].observerTarget = -1;
				wb[ci].money = 0;
				wb[ci].isWanted = false;
			}
			if (ok) ++config::dbg_bc_ok;
			else ++config::dbg_bc_bonefail;
			wb[ci].valid = true;
		}
		// Merge fullradar Lua data for entities not in the C++ entity list
		if (config::full_radar) {
			static int s_radarQueryTick = 0;
			static std::string s_radarBuf;
			if (++s_radarQueryTick >= 15) {
				s_radarQueryTick = 0;
				s_radarBuf = luascripts::QueryLuaScript(luascripts::LUA_FULLRADAR_READ);
			}
			if (!s_radarBuf.empty()) {
				std::istringstream rss(s_radarBuf);
				std::string rline;
				while (std::getline(rss, rline)) {
					int idx = 0; float rx = 0, ry = 0, rz = 0; int rhp = 0;
					int ralive = 0, rdormant = 0, rteam = 0;
					char rname[64]{};
					if (sscanf_s(rline.c_str(), "%d\t%f\t%f\t%f\t%d\t%d\t%d\t%d\t%63[^\n]",
						&idx, &rx, &ry, &rz, &rhp, &ralive, &rdormant, &rteam, rname, 64) >= 8) {
						if (idx < 1 || idx >= 128) continue;
						if (!ralive) continue;
						if (wb[idx].valid) {
							if (wb[idx].dormant) {
								wb[idx].absOrigin = { rx, ry, rz };
								wb[idx].health = rhp;
								if (rname[0] && !wb[idx].name[0])
									strncpy_s(wb[idx].name, rname, 31);
								int ri2 = config::g_viewReadIdx.load(std::memory_order_acquire);
								wb[idx].distance = (wb[idx].absOrigin - config::g_cameraOrigin[ri2]).Length();
							}
							continue;
						}
						wb[idx].valid = true;
						wb[idx].dormant = true;
						wb[idx].noBones = true;
						wb[idx].absOrigin = { rx, ry, rz };
						wb[idx].health = rhp;
						strncpy_s(wb[idx].name, rname, 31);
						wb[idx].rpName[0] = '\0';
						wb[idx].job[0] = '\0';
						wb[idx].weapon[0] = '\0';
						wb[idx].weaponList[0] = '\0';
						wb[idx].gang[0] = '\0';
						wb[idx].isAdmin = false;
						wb[idx].isSuperAdmin = false;
						wb[idx].observerMode = 0;
						wb[idx].observerTarget = -1;
						wb[idx].money = 0;
						wb[idx].isWanted = false;
						int ri2 = config::g_viewReadIdx.load(std::memory_order_acquire);
						wb[idx].distance = (wb[idx].absOrigin - config::g_cameraOrigin[ri2]).Length();
					}
				}
			}
		}

		config::g_boneReadIdx.store(writeIdx, std::memory_order_release);

		BacktrackStore();
		stealth::UpdateSpectatorState(interfaces::engine->GetLocalPlayer());

		if (stealth::IsPanicked()) {
			interfaces::globalVars->curtime = oldCurtime;
			interfaces::globalVars->frametime = oldFrametime;
			return result;
		}

		RemoveViewPunch(cmd, localPlayer);

		if (follow_bot::enabled) {
			// Combat aim runs first so viewangles are set before movement is computed
			bot_combat::Update(cmd, localPlayer, follow_bot::targetIdx);

			// Check if combat wants us to flee
			if (bot_combat::g_wantFlee && bot_tasks::currentMode != bot_tasks::BotMode::Flee) {
				bot_tasks::TriggerFlee(bot_combat::g_fleeFromPos);
			}
			bot_combat::g_wantFlee = false;

			if (bot_tasks::currentMode == bot_tasks::BotMode::Follow) {
				follow_bot::Update(cmd, localPlayer);
			} else {
				bot_tasks::Update(cmd, localPlayer);
			}
			bot_nav::RecordNode(localPlayer->GetAbsOrigin(), localPlayer);
		}

		bool aimActive = config::aimbot && interfaces::movehelper && interfaces::gamemovement && interfaces::prediction;
		if (aimActive && config::aimkey != 0)
			aimActive = (GetAsyncKeyState(config::aimkey) & 0x8000) != 0;

		if (aimActive) {
			interfaces::gamemovement->StartTrackPredictionErrors(localPlayer);

			interfaces::movehelper->SetHost(localPlayer);
			interfaces::prediction->SetupMove(localPlayer, cmd, interfaces::movehelper, &moveData);
			interfaces::gamemovement->ProcessMovement(localPlayer, &moveData);
			interfaces::prediction->FinishMove(localPlayer, cmd, &moveData);

			C_BasePlayer* bestTarget = nullptr;

			static int lockedTarget = -1;
			static int lockTicksLeft = 0;

			float bestFov = config::aimbot_fov;
			float bestScore = -1.f;
			Vector bestTargetPos{0,0,0};
			bool canHit = false;

			Vector eyePos = localPlayer->GetEyePosition();
			Angle origAngles = cmd->viewangles;

			auto& aimCache = config::BoneRead();
			for (int i = 0; i < 128; ++i) {
				if (!aimCache[i].valid) continue;
				if (aimCache[i].noBones) continue;

				C_BasePlayer* target = (C_BasePlayer*)interfaces::entityList->GetClientEntity(i);
				if (target == nullptr) continue;
				if (target == localPlayer) continue;
				if (!target->IsPlayer()) continue;
				if (!target->IsAlive()) continue;
				if (!config::IsTargetAllowed(i)) continue;

				const Matrix3x4* bones = aimCache[i].bones;

				if (config::bone < 0 || config::bone >= 128) continue;
				const Vector rawBonePos = bones[config::bone].GetOrigin();
				Vector bonePos = rawBonePos;

				if (config::resolver) {
					resolver::Update(i, target, bones);

					Vector resolved = resolver::ResolvePos(i, rawBonePos, target->GetAbsOrigin());
					if (CanHit(target, eyePos, resolved)) {
						bonePos = resolved;
					} else {
						for (const float off : resolver::bruteOffsets) {
							Vector candidate = resolver::RotateAround(rawBonePos, target->GetAbsOrigin(), off);
							if (CanHit(target, eyePos, candidate)) {
								resolver::g_states[i].resolvedOffset = off;
								bonePos = candidate;
								break;
							}
						}
					}
				}

				const Angle toTarget = Angle::FromVector(bonePos - eyePos);
				const float fovDiff = normalize_yaw(toTarget.y - origAngles.y);
				const float pitchDiff = toTarget.p - origAngles.p;
				const float totalFov = sqrtf(fovDiff * fovDiff + pitchDiff * pitchDiff);

				if (totalFov >= config::aimbot_fov) continue;

				if (config::aim_priority) {
					float fovScore = 1.f - (totalFov / config::aimbot_fov);
					float distRatio = aimCache[i].distance / 3000.f;
					float distScore = 1.f - (distRatio > 1.f ? 1.f : distRatio);
					float hpRatio = static_cast<float>(aimCache[i].health) / 100.f;
					float healthScore = 1.f - (hpRatio > 1.f ? 1.f : hpRatio);
					float threatScore = 0.f;
					const char* wep = aimCache[i].weapon;
					if (wep[0]) {
						std::string w(wep);
						if (w.find("awp") != std::string::npos || w.find("sniper") != std::string::npos) threatScore = 1.f;
						else if (w.find("shotgun") != std::string::npos || w.find("rpg") != std::string::npos) threatScore = 0.8f;
						else if (w.find("rifle") != std::string::npos || w.find("ak") != std::string::npos || w.find("m4") != std::string::npos) threatScore = 0.6f;
						else if (w.find("smg") != std::string::npos || w.find("mac") != std::string::npos) threatScore = 0.4f;
						else if (w.find("pistol") != std::string::npos || w.find("glock") != std::string::npos || w.find("deagle") != std::string::npos) threatScore = 0.3f;
					}
					float score = fovScore * config::aim_w_fov
					            + distScore * config::aim_w_distance
					            + healthScore * config::aim_w_health
					            + threatScore * config::aim_w_threat;
					if (score > bestScore) {
						bestScore = score;
						bestTarget = target;
						bestTargetPos = bonePos;
						canHit = CanHit(target, eyePos, bonePos);
					}
				} else {
					if (totalFov < bestFov) {
						bestFov = totalFov;
						bestTarget = target;
						bestTargetPos = bonePos;
						canHit = CanHit(target, eyePos, bonePos);
					}
				}
			}

			// Aim target lock: prefer the locked target if still valid
			if (config::aim_target_lock) {
				if (lockedTarget > 0 && lockTicksLeft > 0) {
					// Check if locked target is still valid
					C_BasePlayer* lockEnt = (C_BasePlayer*)interfaces::entityList->GetClientEntity(lockedTarget);
					if (lockEnt && lockEnt != localPlayer && lockEnt->IsPlayer() && lockEnt->IsAlive()
						&& config::IsTargetAllowed(lockedTarget) && aimCache[lockedTarget].valid) {
						const Matrix3x4* lockBones = aimCache[lockedTarget].bones;
						if (config::bone >= 0 && config::bone < 128) {
							Vector lockBonePos = lockBones[config::bone].GetOrigin();
							bestTarget = lockEnt;
							bestTargetPos = lockBonePos;
							canHit = CanHit(lockEnt, eyePos, lockBonePos);
						}
					} else {
						// Locked target no longer valid
						lockedTarget = -1;
						lockTicksLeft = 0;
					}
					--lockTicksLeft;
					if (lockTicksLeft <= 0) {
						lockedTarget = -1;
					}
				}

				// If we found a new target and are not locked, lock onto it
				if (bestTarget && lockedTarget < 0) {
					for (int li = 1; li < 128; ++li) {
						C_BasePlayer* le = (C_BasePlayer*)interfaces::entityList->GetClientEntity(li);
						if (le == bestTarget) { lockedTarget = li; break; }
					}
					lockTicksLeft = config::aim_lock_ticks;
				}
			} else {
				lockedTarget = -1;
				lockTicksLeft = 0;
			}

			// Backtrack: if enabled and normal aimbot found no target, try historical ticks
			int btTickIdx = -1;
			if (config::backtrack && !bestTarget) {
				BacktrackResult bt = BacktrackFindTarget(localPlayer, cmd);
				if (bt.found && bt.fov < config::aimbot_fov) {
					C_BasePlayer* btEnt = (C_BasePlayer*)interfaces::entityList->GetClientEntity(bt.entityIdx);
					if (btEnt) {
						bestTarget = btEnt;
						bestTargetPos = bt.aimPos;
						canHit = CanHit(btEnt, eyePos, bt.aimPos);
						btTickIdx = bt.tickIdx;
					}
				}
			}

			if (bestTarget && (canHit || config::longRangeMelee)) {
				const Vector aimVec = bestTargetPos - eyePos;
				if (aimVec.LengthSqr() >= 0.01f) {
					Angle aimAngles = Angle::FromVector(aimVec);

					if (config::rcs) {
						const Angle& punch = localPlayer->GetViewPunch();
						// Weapon-aware RCS: heavier compensation for high-recoil weapons
						float rcsMul = 2.f;
						if (bestTarget) {
							int bestIdx = -1;
							for (int ri = 1; ri < 128; ++ri) {
								if ((C_BasePlayer*)interfaces::entityList->GetClientEntity(ri) == localPlayer)
									{ bestIdx = ri; break; }
							}
							if (bestIdx > 0) {
								const char* wep = aimCache[bestIdx].weapon;
								if (wep[0]) {
									std::string w(wep);
									if (w.find("ak") != std::string::npos || w.find("galil") != std::string::npos) rcsMul = 2.1f;
									else if (w.find("m4") != std::string::npos || w.find("m16") != std::string::npos) rcsMul = 1.95f;
									else if (w.find("smg") != std::string::npos || w.find("mac") != std::string::npos || w.find("mp") != std::string::npos) rcsMul = 1.8f;
									else if (w.find("pistol") != std::string::npos || w.find("glock") != std::string::npos) rcsMul = 1.6f;
									else if (w.find("deagle") != std::string::npos || w.find("revolver") != std::string::npos) rcsMul = 2.3f;
									else if (w.find("sniper") != std::string::npos || w.find("awp") != std::string::npos) rcsMul = 1.0f;
									else if (w.find("shotgun") != std::string::npos) rcsMul = 1.5f;
								}
							}
						}
						aimAngles.p -= punch.p * rcsMul;
						aimAngles.y -= punch.y * rcsMul;
					}

					aimAngles.FixAngles();

					if (std::isfinite(aimAngles.p) && std::isfinite(aimAngles.y)) {
						if (config::silent) {
							cmd->viewangles = aimAngles;
							FixMovement(cmd, origAngles);
							interfaces::prediction->SetLocalViewAngles(origAngles);
						}
						else {
							if (config::aim_smooth > 0.f) {
								float factor = 1.f / (config::aim_smooth + 1.f);
								float dy = normalize_yaw(aimAngles.y - origAngles.y);
								float dp = aimAngles.p - origAngles.p;
								aimAngles.y = origAngles.y + dy * factor;
								aimAngles.p = origAngles.p + dp * factor;
								aimAngles.FixAngles();
							}
							cmd->viewangles = aimAngles;
							interfaces::prediction->SetLocalViewAngles(cmd->viewangles);
							interfaces::engine->SetViewAngles(cmd->viewangles);
						}

						// Backtrack tick_count: tell server to resolve hit at historical simtime
						if (btTickIdx >= 0 && btTickIdx < config::BT_MAX_TICKS) {
							for (int bi = 1; bi < 128; ++bi) {
								C_BasePlayer* be = (C_BasePlayer*)interfaces::entityList->GetClientEntity(bi);
								if (be == bestTarget) {
									double st = config::g_btBuf[bi][btTickIdx].simtime;
									if (st > 0.0 && interfaces::globalVars->interval_per_tick > 0.f) {
										cmd->tick_count = static_cast<int>(0.5 + st / (double)interfaces::globalVars->interval_per_tick);
									}
									break;
								}
							}
						}

						if (config::autoshoot) {
							cmd->buttons |= CUserCmd::ButtonFlags::IN_ATTACK;
						}
					}
				}
			}

			interfaces::gamemovement->FinishTrackPredictionErrors(localPlayer);
			interfaces::movehelper->SetHost(0);
		}

		Triggerbot(cmd, localPlayer);

		// Anti-aim: only when not actively attacking
		if (antiaim::enabled && !(cmd->buttons & CUserCmd::IN_ATTACK)) {
			Angle preAA = cmd->viewangles;
			antiaim::Apply(cmd, preAA);
			FixMovement(cmd, preAA);
		}

		// Freecam key toggle check + sync speed slider to Lua
		freecam::CheckToggle();
		{
			static float s_lastFcSpeed = 0.f;
			if (freecam::g_active && freecam::speed != s_lastFcSpeed) {
				freecam::UpdateSpeed();
				s_lastFcSpeed = freecam::speed;
			}
		}

		// Anti-aim: desync server-side angles so headshots miss
		if (config::anti_aim && !config::aimbot) {
			static float spinAngle = 0.f;
			switch (config::anti_aim_mode) {
			case 0: // jitter
				cmd->viewangles.y += (rand() % 2 == 0) ? 90.f : -90.f;
				cmd->viewangles.p = (rand() % 2 == 0) ? 89.f : -89.f;
				break;
			case 1: // spin
				spinAngle += 30.f;
				if (spinAngle > 360.f) spinAngle -= 360.f;
				cmd->viewangles.y = spinAngle;
				cmd->viewangles.p = 89.f;
				break;
			case 2: // backwards
				cmd->viewangles.y += 180.f;
				cmd->viewangles.p = 89.f;
				break;
			case 3: // down
				cmd->viewangles.p = 89.f;
				break;
			}
		}

		// Speed hack: multiply movement values
		if (config::speed_hack) {
			cmd->forwardmove *= config::speed_multiplier;
			cmd->sidemove *= config::speed_multiplier;
		}

		// Tick exploits: lag switch, doubletap, speedhack
		tick_exploits::DoubletapCharge(cmd);
		tick_exploits::DoubletapFire(cmd, localPlayer);
		if (tick_exploits::speedhack_enabled)
			tick_exploits::SpeedhackModify(cmd);

		// Fake lag: choke packets to desync model position
		bool choking = fakelag::ShouldChoke(localPlayer, cmd);
		if (!choking) choking = tick_exploits::LagSwitchShouldChoke();
		if (!choking) choking = tick_exploits::DoubletapShouldChoke();

		interfaces::globalVars->curtime = oldCurtime;
		interfaces::globalVars->frametime = oldFrametime;

		if (choking) return false;
	}

	return result;
}

lua_State* __stdcall detourLuaHook(void* luaAlloc, void* ud) {
	lua_State* luaState = ogLuaHook(luaAlloc, ud);
	if (config::g_requestEject.load(std::memory_order_acquire))
		return luaState;
	config::luastate = luaState;
	spdlog::default_logger()->info("Hooked new LUA state: {}", (void*)luaState);

	// Reset ALL Lua installation guards so features re-install on the new state
	net_panel::g_snifferInstalled = false;
	net_panel::g_chatSpyInstalled = false;
	net_panel::g_deepHookInstalled = false;
	net_panel::g_payloadSnifferInstalled = false;
	sound_esp::g_hookInstalled = false;
	killfeed::g_killfeedInstalled = false;
	killfeed::g_voiceSpyInstalled = false;
	xray::g_installed = false;
	freecam::g_installed = false;
	freecam::g_active = false;
	stealth::g_screenshotCleanerInstalled = false;
	stealth::g_antiACInstalled = false;
	voice_exploits::g_installed = false;
	killstreak::g_current = 0;
	killstreak::g_best = 0;
	killstreak::g_lastKillTime = 0.f;
	killstreak::g_displayUntil = 0.f;
	config::g_crosshairInfoBuf[0][0] = '\0';
	config::g_crosshairInfoBuf[1][0] = '\0';

	spdlog::info("[hooks] Reset all Lua installation guards for new state");
	return luaState;
}

void __stdcall detourReadPixels(void*, int x, int y, int width, int height, unsigned char* data, ImageFormat dstFormat) {
	spdlog::default_logger()->info("Hooked MatRenderContext::ReadPixels (screengrab)! Width: {} Height: {} Destination format: {}", width, height, (int)dstFormat);

	ogReadPixels(interfaces::matrenderctx, x, y, width, height, data, dstFormat);
}

void __stdcall detourReadPixelsAndStretch(void*, void* pSrcRect, void* pDstRect, unsigned char* pBuffer, ImageFormat dstFormat, int nDstStride) {
	spdlog::default_logger()->info("Hooked MatRenderContext::ReadPixelsAndStretch (screengrab)! dstFormat: {}",(int)(dstFormat));

	ogReadPixelsAndStretch(interfaces::matrenderctx, pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride);
}

static IMaterial* s_chamsMat = nullptr;

void __stdcall detourDrawModelExecute(void*, DrawModelState_t& state, ModelRenderInfo_t& pInfo, Matrix3x4* pCustomBoneToWorld = NULL) {
	if (!interfaces::engine || !interfaces::engine->IsInGame()) {
		ogDrawModelExecute(interfaces::modelrender, state, pInfo, pCustomBoneToWorld);
		return;
	}
	if (config::chams && pInfo.entity_index >= 1 && pInfo.entity_index <= interfaces::globalVars->maxClients && config::IsTargetAllowed(pInfo.entity_index)) {
		C_BasePlayer* entity = (C_BasePlayer*)interfaces::entityList->GetClientEntity(pInfo.entity_index);

		if (entity && entity != localPlayer)
		{
			if (!s_chamsMat) {
				s_chamsMat = interfaces::matsystem->FindMaterial("debug/debugdrawflat", TEXTURE_GROUP_MODEL);
				if (!s_chamsMat || s_chamsMat->IsErrorMaterial())
					s_chamsMat = interfaces::matsystem->FindMaterial("models/wireframe", TEXTURE_GROUP_MODEL);
				if (!s_chamsMat) goto normal_draw;
				s_chamsMat->AddRef();
				s_chamsMat->SetMaterialVarFlag(MATERIAL_VAR_SELFILLUM, true);
				s_chamsMat->SetMaterialVarFlag(MATERIAL_VAR_NOFOG, true);
			}

			// Hidden pass: draw through walls
			s_chamsMat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, true);
			s_chamsMat->ColorModulate(config::chams_hidden_color[0], config::chams_hidden_color[1], config::chams_hidden_color[2]);
			interfaces::modelrender->ForcedMaterialOverride(s_chamsMat, 0);
			ogDrawModelExecute(interfaces::modelrender, state, pInfo, pCustomBoneToWorld);

			// Visible pass: normal Z-test on top
			s_chamsMat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);
			s_chamsMat->ColorModulate(config::chams_visible_color[0], config::chams_visible_color[1], config::chams_visible_color[2]);
			interfaces::modelrender->ForcedMaterialOverride(s_chamsMat, 0);
			ogDrawModelExecute(interfaces::modelrender, state, pInfo, pCustomBoneToWorld);

			interfaces::modelrender->ForcedMaterialOverride(nullptr, 0);
			return;
		}
	}

normal_draw:
	ogDrawModelExecute(interfaces::modelrender, state, pInfo, pCustomBoneToWorld);
}

void hooks::Init() noexcept
{
	MH_CreateHook(
		mem::Get(interfaces::clientMode, 21),
		&detourCreateMove,
		reinterpret_cast<void**>(&ogCreateMove)
	);

	MH_CreateHook(
		reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("lua_shared.dll"), "lua_newstate")),
		&detourLuaHook,
		reinterpret_cast<void**>(&ogLuaHook)
	);

	/*MH_CreateHook(
		mem::Get(interfaces::matrenderctx, 12), // 11 or 12 
		&detourReadPixels,
		reinterpret_cast<void**>(&ogReadPixels)
	);

	MH_CreateHook(
		mem::Get(interfaces::matrenderctx, 99), //  99 or 98
		&detourReadPixelsAndStretch,
		reinterpret_cast<void**>(&ogReadPixelsAndStretch)
	);*/

	MH_CreateHook(
		mem::Get(interfaces::modelrender, 20), //  99 or 98
		&detourDrawModelExecute,
		reinterpret_cast<void**>(&ogDrawModelExecute)
	);

	MH_EnableHook(MH_ALL_HOOKS);
}

void hooks::Shutdown() noexcept
{
	config::g_inGame.store(false, std::memory_order_relaxed);
	if (s_chamsMat) {
		s_chamsMat->Release();
		s_chamsMat = nullptr;
	}
	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);
}