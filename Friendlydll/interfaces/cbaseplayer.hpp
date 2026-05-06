#pragma once
#include "../includes.hpp"

// ValveBiped skeleton indices for standard GMod/HL2 player models (Bip01 hierarchy)
enum Bones : int {
	bone_pelvis    = 1,   // Bip01_Pelvis
	bone_spine_2   = 2,   // Bip01_Spine
	bone_spine_3   = 3,   // Bip01_Spine1
	bone_spine_1   = 4,   // Bip01_Spine2
	bone_neck      = 6,   // Bip01_Neck1
	bone_head      = 7,   // Bip01_Head1
	bone_arm_top_l = 9,   // Bip01_L_UpperArm
	bone_arm_bot_l = 10,  // Bip01_L_Forearm
	bone_hand_l    = 11,  // Bip01_L_Hand
	bone_arm_top_r = 13,  // Bip01_R_UpperArm
	bone_arm_bot_r = 14,  // Bip01_R_Forearm
	bone_hand_r    = 15,  // Bip01_R_Hand
	bone_leg_top_l = 16,  // Bip01_L_Thigh
	bone_leg_bot_l = 17,  // Bip01_L_Calf
	bone_ANKLE_l   = 18,  // Bip01_L_Foot
	bone_leg_top_r = 20,  // Bip01_R_Thigh
	bone_leg_bot_r = 21,  // Bip01_R_Calf
	bone_ANKLE_r   = 22,  // Bip01_R_Foot
};

class C_BasePlayer {
public:

	static int off_iHealth() {
		static int o = []() {
			int v = netvars::Get("DT_BaseEntity", "m_iHealth");
			if (v) spdlog::info("[netvars] m_iHealth resolved to 0x{:X}", v);
			return v ? v : 0xD8;
		}();
		return o;
	}
	static int off_fFlags() {
		static int o = []() {
			int v = netvars::Get("DT_BasePlayer", "m_fFlags");
			if (v) spdlog::info("[netvars] m_fFlags resolved to 0x{:X}", v);
			return v ? v : 0x450;
		}();
		return o;
	}
	static int off_vecViewOffset() {
		static int o = []() {
			int v = netvars::Get("DT_LocalPlayerExclusive", "m_vecViewOffset[0]");
			if (v) spdlog::info("[netvars] m_vecViewOffset resolved to 0x{:X}", v);
			return v ? v : 0x144;
		}();
		return o;
	}
	static int off_vecVelocity() {
		static int o = []() {
			int v = netvars::Get("DT_BaseEntity", "m_vecVelocity[0]");
			if (v) spdlog::info("[netvars] m_vecVelocity resolved to 0x{:X}", v);
			return v ? v : 0x150;
		}();
		return o;
	}
	static int off_nTickBase() {
		static int o = []() {
			int v = netvars::Get("DT_LocalPlayerExclusive", "m_nTickBase");
			if (v) spdlog::info("[netvars] m_nTickBase resolved to 0x{:X}", v);
			return v ? v : 0x2D30;
		}();
		return o;
	}
	static int off_bDormant() {
		static int o = 0x1FA; // legacy fallback, prefer vtable call
		return o;
	}

	const int& GetHealth() noexcept {
		return *reinterpret_cast<int*>(uintptr_t(this) + off_iHealth());
	}

	const int& GetFlags() noexcept {
		return *reinterpret_cast<int*>(uintptr_t(this) + off_fFlags());
	}

	const Vector& GetAbsOrigin() noexcept {
		return mem::Call<const Vector&>(this, 9);
	}

	const bool IsAlive() noexcept {
		return mem::Call<bool>(this, 130);
	}

	const bool IsPlayer() noexcept {
		return mem::Call<bool>(this, 131);
	}

	const Vector& GetViewOffset() noexcept {
		return *reinterpret_cast<Vector*>(uintptr_t(this) + off_vecViewOffset());
	}

	const Vector& GetVelocity() noexcept {
		return *reinterpret_cast<Vector*>(uintptr_t(this) + off_vecVelocity());
	}

	// IClientRenderable sub-object is at this+0x8 in x64 GMod
	bool SetupBones(Matrix3x4* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime) noexcept {
		return mem::Call<bool>(this + 0x8, 16, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
	}

	void SetMaterialOverridePointer(void* mat) noexcept {
		mem::Call<void>(this, 161, mat);
	}

	// IClientNetworkable sub-object is at this+0x10 in x64 GMod
	const bool IsDormant(void) noexcept {
		return mem::Call<bool>(this + 0x10, 8);
	}

	Vector GetEyePosition() noexcept {
		return GetAbsOrigin() + GetViewOffset();
	}

	const int GetTickBase() noexcept {
		return *reinterpret_cast<int*>(uintptr_t(this) + off_nTickBase());
	}

	const Angle& GetViewPunch() noexcept {
		static int off = []() {
			int o = netvars::Get("DT_Local", "m_vecPunchAngle");
			return o ? o : 0x2998;
		}();
		return *reinterpret_cast<Angle*>(uintptr_t(this) + off);
	}

	const Angle& GetAimPunch() noexcept {
		static int off = []() {
			int o = netvars::Get("DT_Local", "m_vecPunchAngleVel");
			if (!o) o = netvars::Get("DT_Local", "m_vecPunchAngle");
			return o ? o : 0x2998;
		}();
		return *reinterpret_cast<Angle*>(uintptr_t(this) + off);
	}

	const void* GetActiveWeapon() noexcept {
		return mem::Call<void*>(this, 280);
	}
};

inline C_BasePlayer* localPlayer = nullptr;
