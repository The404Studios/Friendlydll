#pragma once
#include "../includes.hpp"
#include <unordered_set>

namespace config {
	// movement
	inline bool bunnyhop = false;
	inline bool autostrafe = false;
	inline bool autostrafe_legit = true;
	inline bool autostrafe_silent = false;
	inline bool autostrafe_directional = false;

	// aiming
	inline bool aimbot = false;
	inline bool silent = false;
	inline bool autoshoot = false;
	inline bool resolver = false;
	inline int bone = Bones::bone_head;
	inline float aimbot_fov = 10.f;
	inline float fov = 100;
	inline int aimkey = 0;           // 0=always on, else VK_ code to hold
	inline float aim_smooth = 0.f;   // 0=instant snap, 1+=interpolation divisor
	inline bool rcs = false;         // recoil compensation
	inline bool aim_priority = false;
	inline float aim_w_fov = 1.0f;
	inline float aim_w_distance = 0.3f;
	inline float aim_w_health = 0.2f;
	inline float aim_w_threat = 0.5f;

	// esp
	inline bool snapline = false;
	inline bool squareesp = false;
	inline bool chams = false;
	inline bool boneskeleton = false;
	inline float esp_min_dist = 0.f;
	inline float esp_max_dist = 50000.f;
	inline bool esp_debug = false;
	inline bool show_dormant = true;
	inline bool full_radar = false;
	inline bool force_pvs = true;
	inline float force_pvs_interval = 5.f;
	inline bool pvs_aggressive = true; // shorter interval when dormant players detected nearby
	inline float snapline_color[3] = { 0.f, 0.f, 0.f };
	inline float squareesp_color[3] = { 0.f, 0.f, 0.f };
	inline float skeleton_color[3] = { 1.f, 1.f, 1.f };
	inline float chams_visible_color[3] = { 0.f, 1.f, 0.f };
	inline float chams_hidden_color[3]  = { 1.f, 0.f, 0.f };

	// player list
	inline bool whitelistMode = false;
	inline std::unordered_set<int> playerListChecked;

	inline bool IsTargetAllowed(int entIdx) {
		if (playerListChecked.empty()) return true;
		bool inList = playerListChecked.count(entIdx) > 0;
		return whitelistMode ? inList : !inList;
	}

	// combat
	inline bool triggerbot = false;
	inline int triggerbot_key = 0;          // 0=use aimkey, else VK_ code
	inline int triggerbot_delay_ms = 0;      // ms delay before triggerbot fires (0=instant)
	inline bool triggerbot_head_only = false; // only fire when crosshair is on head hitbox
	inline bool backtrack = false;
	inline int backtrack_ticks = 6;
	inline bool backtrack_visualize = false;  // draw backtrack tick positions on screen
	inline bool viewpunch_remove = false;
	inline bool aim_target_lock = false;      // lock onto target for N ticks once acquired
	inline int aim_lock_ticks = 10;           // how many ticks to stay locked

	// esp intel badges
	inline bool esp_intel_badges = true;

	// entity esp
	inline bool entity_esp = false;
	inline float entity_esp_timer_max = 300.f; // 5 min countdown for shipments
	inline bool entity_esp_printers = true;
	inline bool entity_esp_shipments = true;
	inline bool entity_esp_drugs = true;
	inline bool entity_esp_doors = false;
	inline float entity_esp_color_printer[3] = { 0.f, 1.f, 0.f };
	inline float entity_esp_color_shipment[3] = { 1.f, 0.5f, 0.f };
	inline float entity_esp_color_drug[3] = { 1.f, 0.f, 1.f };
	inline float entity_esp_color_door[3] = { 0.f, 0.78f, 1.f };
	inline bool entity_esp_weapons = true;
	inline bool entity_esp_money = true;
	inline bool entity_esp_vehicles = false;
	inline bool entity_esp_health_bars = true;
	inline float entity_esp_color_weapon[3] = { 0.3f, 0.6f, 1.f };
	inline float entity_esp_color_money[3] = { 1.f, 0.85f, 0.f };
	inline float entity_esp_color_vehicle[3] = { 0.8f, 0.8f, 0.8f };

	// hud
	inline bool minimap = false;
	inline float minimap_zoom = 2000.f;
	inline bool fov_circle = false;
	inline float fov_circle_color[3] = { 0.f, 0.7f, 0.85f };
	inline bool fullbright = false;
	inline bool sound_esp = false;

	// intel
	inline bool admin_alert = true;
	inline bool spectator_alert = true;
	inline bool money_tracker = false;

	// stealth
	inline int panic_key = VK_END;
	inline bool spectator_auto_disable = false;
	inline bool screenshot_cleaner = true;
	inline bool recording_mode = false;
	inline bool anti_anticheat = false;
	inline bool anti_screenshot = false;   // expanded: intercept render.Capture, surface.GetTextureID, etc.
	inline bool anti_kick = false;         // auto-vote-no on kick votes + block kick concommand
	inline bool name_steal_cycle = false;  // cycle through player names every 30s
	inline bool fake_death = false;        // periodic fake death events while alive
	inline bool admin_bypass = false;      // hide from ULX/SAM/FAdmin/ServerGuard player lists
	inline bool ac_bypass = false;         // bypass gAC, CAC, StackAC scanning hooks
	inline bool spectator_cloak = false;   // disable visible features per-spectator (selective, not full panic)

	// misc
	inline bool chatondeath = false;
	inline bool allowcslua = false;
	inline bool allowcheats = false;
	inline bool longRangeMelee = false;
	inline bool net_sniffer = false;

	// world tab
	inline bool crosshair_info = false;
	inline bool killstreak_enabled = false;

	// exploit/world toggles (persisted)
	inline bool nightVision = false;
	inline bool hitmarker = false;
	inline bool advertRunning = false;
	inline bool antiAfk = false;
	inline bool lagExploit = false;
	inline bool silentWalk = false;
	inline bool slideWalk = false;
	inline bool lockpickAuto = false;
	inline bool propAlert = false;
	inline bool voteBot = false;
	inline bool propFly = false;
	inline bool killSound = false;
	inline bool entityMagnet = false;
	inline bool ghostMode = false;
	inline bool infiniteAmmo = false;
	inline bool noRecoilLua = false;
	inline bool antiCrash = false;
	inline bool propKill = false;
	inline bool serverCrash = false;
	inline bool puppetRecording = false;
	inline bool nameSteal = false;
	inline bool antiKick = false;
	inline bool fakeDeath = false;
	inline bool matWallhack = false;
	inline bool lowGrav = false;
	inline bool soundSpam = false;
	inline bool autoBuy = false;

	// new exploits
	inline bool anti_aim = false;
	inline int anti_aim_mode = 0; // 0=jitter 1=spin 2=backwards 3=down
	inline bool speed_hack = false;
	inline float speed_multiplier = 2.5f;
	inline bool rapid_fire = false;
	inline bool no_fall_damage = false;
	inline bool entity_steal = false;
	inline bool chat_spam = false;
	inline char chat_spam_msg[128] = "Friendlydll on top";
	inline float chat_spam_delay = 1.5f;
	inline bool player_crasher = false;
	inline int crasher_target = -1;
	inline bool keypad_cracker = false;
	inline bool door_exploit = false;

	// original exploits v2
	inline bool server_dump = false;
	inline bool spec_mirror = false;
	inline bool movement_predict = false;
	inline bool chat_intel = false;
	inline bool aim_humanizer = false;
	inline bool macro_system = false;
	inline int macro_selected = 0;

	// duplication exploits
	inline bool dupe_net_capture = false;
	inline bool dupe_buy_capture = false;
	inline bool dupe_auto_loop = false;
	inline int dupe_burst_count = 5;
	inline float dupe_burst_delay = 0.02f;
	inline float dupe_auto_interval = 1.0f;

	// advanced exploits v3
	inline bool hdr_stack = false;
	inline bool mat_glow = false;
	inline bool render_override = false;
	inline bool phys_predict = false;
	inline bool error_sanitizer = false;
	inline bool crc_spoof = false;
	inline bool stack_spoof = false;
	inline bool hook_sanitizer = false;
	inline bool vehicle_boost = false;
	inline bool anim_override = false;
	inline int anim_mode = 0;
	inline bool sound_mask = false;
	inline bool death_cam = false;

	// darkrp exploit toggles
	inline bool warrantShield = false;
	inline bool antiArrestEnabled = false;
	inline bool baseAlarmEnabled = false;
	inline bool autoBountyEnabled = false;
	inline bool doorAutoClose = false;
	inline bool proximityAlertEnabled = false;
	inline bool lootVacuumEnabled = false;

	inline char g_crosshairInfoBuf[2][256]{};
	inline std::atomic<int> g_crosshairInfoIdx{0};

	// access
	inline std::atomic<bool> g_inGame{false};
	inline std::atomic<bool> g_panic{false};
	inline std::atomic<bool> g_requestEject{false};
	inline std::atomic<bool> g_beingSpectated{false};
	inline std::atomic<int>  g_spectatorCount{0};
	inline float currentvelocity = 0.f;
	inline lua_State* luastate = nullptr;

	// cached view matrix + screen size (written by CreateMove, read by Present)
	inline VMatrix g_viewMatrix[2]{};
	inline int g_screenW[2]{};
	inline int g_screenH[2]{};
	inline Vector g_cameraOrigin[2]{};
	inline std::atomic<int> g_viewReadIdx{0};

	inline bool WorldToScreen(const Vector& world, float& outX, float& outY) {
		int ri = g_viewReadIdx.load(std::memory_order_acquire);
		const auto& vm = g_viewMatrix[ri];
		int sw = g_screenW[ri];
		int sh = g_screenH[ri];
		if (sw <= 0 || sh <= 0) return false;

		float w = vm[3][0] * world.x + vm[3][1] * world.y + vm[3][2] * world.z + vm[3][3];
		if (w < 0.001f) return false;

		float x = vm[0][0] * world.x + vm[0][1] * world.y + vm[0][2] * world.z + vm[0][3];
		float y = vm[1][0] * world.x + vm[1][1] * world.y + vm[1][2] * world.z + vm[1][3];

		outX = (sw * 0.5f) + (x / w) * (sw * 0.5f);
		outY = (sh * 0.5f) - (y / w) * (sh * 0.5f);
		return true;
	}

	// per-frame debug counters (written by Present, read by overlay)
	inline int dbg_skip_notplayer = 0;
	inline int dbg_skip_dead      = 0;
	inline int dbg_skip_nobones   = 0;
	inline int dbg_cache_valid    = 0;

	// bone cache loop debug counters (written by CreateMove)
	inline int dbg_bc_total      = 0;
	inline int dbg_bc_null       = 0;
	inline int dbg_bc_notplayer  = 0;
	inline int dbg_bc_dead       = 0;
	inline int dbg_bc_dormant    = 0;
	inline int dbg_bc_bonefail   = 0;
	inline int dbg_bc_ok         = 0;

	struct DormantTrack {
		Vector velocity{};
		float lastActiveTime = 0.f;
	};
	inline DormantTrack g_dormantTrack[128]{};

	struct BoneRecord {
		Matrix3x4 bones[128];
		Vector absOrigin;
		int health = 0;
		char name[32]{};
		char rpName[64]{};
		char job[64]{};
		char weapon[64]{};
		char weaponList[128]{};
		char gang[64]{};
		float distance = 0.f;
		bool valid = false;
		bool noBones = false;
		bool dormant = false;
		bool isAdmin = false;
		bool isSuperAdmin = false;
		int observerMode = 0;
		int observerTarget = -1;
		int money = 0;
		bool isWanted = false;
	};
	inline std::array<BoneRecord, 128> g_boneBuffers[2]{};
	inline std::atomic<int> g_boneReadIdx{0};
	inline auto& BoneRead() { return g_boneBuffers[g_boneReadIdx.load(std::memory_order_acquire)]; }

	// backtrack ring buffer
	constexpr int BT_MAX_TICKS = 12;
	struct BacktrackTick {
		Matrix3x4 bones[128];
		Vector origin;
		double simtime = 0.0;
		bool valid = false;
	};
	inline std::array<std::array<BacktrackTick, BT_MAX_TICKS>, 128> g_btBuf{};
	inline int g_btHead = 0;

	// entity esp records (double-buffered like bones)
	struct EntRecord {
		Vector pos;
		char label[64]{};
		char owner[32]{};
		int type = 0; // 0=printer 1=shipment 2=drug 3=door 4=weapon 5=money 6=vehicle
		int money = 0;
		int health = 0;
		int maxHealth = 0;
		int entIndex = 0;
		float distance = 0.f;
		bool valid = false;
	};
	inline EntRecord g_entBuf[2][512]{};
	inline int g_entCount[2]{};
	inline std::atomic<int> g_entReadIdx{0};
	inline auto& EntRead() { return g_entBuf[g_entReadIdx.load(std::memory_order_acquire)]; }
	inline int EntReadCount() { return g_entCount[g_entReadIdx.load(std::memory_order_acquire)]; }
}