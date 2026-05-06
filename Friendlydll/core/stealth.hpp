#pragma once
#include "../includes.hpp"
#include "lua_scripts.hpp"

namespace stealth {

	inline bool g_screenshotCleanerInstalled = false;
	inline bool g_antiACInstalled = false;

	// ── 1. Panic toggle ────────────────────────────────────────────────

	inline void PanicCheck() {
		// Edge-detect: only fires on the transition (low bit == "just pressed")
		if (GetAsyncKeyState(config::panic_key) & 1) {
			bool cur = config::g_panic.load(std::memory_order_relaxed);
			config::g_panic.store(!cur, std::memory_order_relaxed);
			spdlog::info("[stealth] panic toggled -> {}", !cur);
		}
	}

	inline bool IsPanicked() {
		return config::g_panic.load(std::memory_order_relaxed);
	}

	inline bool ShouldDrawVisuals() {
		return !IsPanicked() && !config::recording_mode;
	}

	// ── 2. Spectator awareness ─────────────────────────────────────────

	inline void UpdateSpectatorState(int localIdx) {
		static bool s_autoPanicked = false;

		auto& bones = config::BoneRead();
		int count = 0;

		for (int i = 0; i < 128; ++i) {
			const auto& rec = bones[i];
			if (!rec.valid) continue;
			if (rec.observerTarget == localIdx && rec.observerMode > 0)
				++count;
		}

		config::g_spectatorCount.store(count, std::memory_order_relaxed);
		config::g_beingSpectated.store(count > 0, std::memory_order_relaxed);

		if (config::spectator_auto_disable) {
			if (count > 0 && !s_autoPanicked) {
				config::g_panic.store(true, std::memory_order_relaxed);
				s_autoPanicked = true;
				spdlog::info("[stealth] auto-panic: {} spectator(s) detected", count);
			}
			else if (count == 0 && s_autoPanicked) {
				config::g_panic.store(false, std::memory_order_relaxed);
				s_autoPanicked = false;
				spdlog::info("[stealth] auto-panic cleared: no spectators");
			}
		}
	}

	// ── 3. Screenshot cleaner ──────────────────────────────────────────

	inline void SetupScreenshotCleaner() {
		if (g_screenshotCleanerInstalled) return;

		bool ok = lualoader::Execute(R"lua(
			pcall(function()
				-- Save originals before overwriting
				_fdll_orig_render_Capture = _fdll_orig_render_Capture or render.Capture
				_fdll_orig_render_ReadPixels = _fdll_orig_render_ReadPixels or render.ReadPixels
				_fdll_orig_render_CapturePixels = _fdll_orig_render_CapturePixels or render.CapturePixels

				render.Capture = function(...) return nil end
				if render.ReadPixels then
					render.ReadPixels = function(...) return nil end
				end
				if render.CapturePixels then
					render.CapturePixels = function(...) return nil end
				end
				if render.SupportsPixelShaders_2_0 then
					local _origSup = render.SupportsPixelShaders_2_0
					render.SupportsPixelShaders_2_0 = function()
						return _origSup()
					end
				end
			end)
		)lua");
		if (ok) {
			g_screenshotCleanerInstalled = true;
			spdlog::info("[stealth] screenshot cleaner installed");
		}
	}

	inline void UninstallScreenshotCleaner() {
		if (!g_screenshotCleanerInstalled) return;

		bool ok = lualoader::Execute(R"lua(
			pcall(function()
				if _fdll_orig_render_Capture then
					render.Capture = _fdll_orig_render_Capture
					_fdll_orig_render_Capture = nil
				end
				if _fdll_orig_render_ReadPixels then
					render.ReadPixels = _fdll_orig_render_ReadPixels
					_fdll_orig_render_ReadPixels = nil
				end
				if _fdll_orig_render_CapturePixels then
					render.CapturePixels = _fdll_orig_render_CapturePixels
					_fdll_orig_render_CapturePixels = nil
				end
			end)
		)lua");
		if (ok) {
			g_screenshotCleanerInstalled = false;
			spdlog::info("[stealth] screenshot cleaner uninstalled");
		}
	}

	// ── 4. Fullbright ──────────────────────────────────────────────────

	inline void UpdateFullbright() {
		static bool s_wasEnabled = false;

		if (config::fullbright && !s_wasEnabled) {
			lualoader::Execute(R"lua(
				pcall(function()
					render.SetLightingMode(2)
				end)
			)lua");
			s_wasEnabled = true;
		}
		else if (!config::fullbright && s_wasEnabled) {
			lualoader::Execute(R"lua(
				pcall(function()
					render.SetLightingMode(0)
				end)
			)lua");
			s_wasEnabled = false;
		}
	}

	// ── 5. Anti-anticheat ──────────────────────────────────────────────

	inline void SetupAntiAC() {
		if (g_antiACInstalled) return;

		bool ok = lualoader::Execute(R"lua(
			pcall(function()
				local blocked = {"anticheat","anti_cheat","ac_","screengrab","screenshot","cheatdetect","cac_"}

				local function isBlocked(name)
					local low = string.lower(tostring(name))
					for _, kw in ipairs(blocked) do
						if string.find(low, kw, 1, true) then
							return true
						end
					end
					return false
				end

				-- remove existing AC hooks
				local tbl = hook.GetTable()
				if tbl then
					for event, hooks in pairs(tbl) do
						for id, _ in pairs(hooks) do
							if isBlocked(id) then
								hook.Remove(event, id)
							end
						end
					end
				end

				-- detour hook.Add to block future AC hooks
				local _origAdd = hook.Add
				hook.Add = function(event, id, fn, ...)
					if isBlocked(id) then
						return
					end
					return _origAdd(event, id, fn, ...)
				end
			end)
		)lua");
		if (ok) {
			g_antiACInstalled = true;
			spdlog::info("[stealth] anti-anticheat hooks stripped and detoured");
		}
	}

	// ── 6. Expanded anti-screenshot (intercepts render.Capture + surface.GetTextureID) ──

	inline bool g_antiScreenshotInstalled = false;

	inline void SetupAntiScreenshot() {
		if (g_antiScreenshotInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_ANTI_SCREENSHOT);
		if (ok) {
			g_antiScreenshotInstalled = true;
			spdlog::info("[stealth] expanded anti-screenshot installed");
		}
	}

	inline void UninstallAntiScreenshot() {
		if (!g_antiScreenshotInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_ANTI_SCREENSHOT_REMOVE);
		if (ok) {
			g_antiScreenshotInstalled = false;
			spdlog::info("[stealth] expanded anti-screenshot removed");
		}
	}

	// ── 7. Anti-kick ──────────────────────────────────────────────────────

	inline bool g_antiKickInstalled = false;

	inline void SetupAntiKick() {
		if (g_antiKickInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_ANTI_KICK);
		if (ok) {
			g_antiKickInstalled = true;
			spdlog::info("[stealth] anti-kick installed");
		}
	}

	inline void UninstallAntiKick() {
		if (!g_antiKickInstalled) return;
		lualoader::Execute(luascripts::LUA_ANTI_KICK_STOP);
		g_antiKickInstalled = false;
		spdlog::info("[stealth] anti-kick removed");
	}

	// ── 8. Name-steal cycle ───────────────────────────────────────────────

	inline bool g_nameStealInstalled = false;

	inline void SetupNameSteal() {
		if (g_nameStealInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_NAME_STEAL_CYCLE);
		if (ok) {
			g_nameStealInstalled = true;
			spdlog::info("[stealth] name-steal cycle installed");
		}
	}

	inline void UninstallNameSteal() {
		if (!g_nameStealInstalled) return;
		lualoader::Execute(luascripts::LUA_NAME_STEAL_STOP);
		g_nameStealInstalled = false;
		spdlog::info("[stealth] name-steal removed");
	}

	// ── 9. Fake death ─────────────────────────────────────────────────────

	inline bool g_fakeDeathInstalled = false;

	inline void SetupFakeDeath() {
		if (g_fakeDeathInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_FAKE_DEATH_PERSISTENT_SETUP);
		if (ok) {
			g_fakeDeathInstalled = true;
			spdlog::info("[stealth] fake-death timer installed");
		}
	}

	inline void UninstallFakeDeath() {
		if (!g_fakeDeathInstalled) return;
		lualoader::Execute(luascripts::LUA_FAKE_DEATH_PERSISTENT_STOP);
		g_fakeDeathInstalled = false;
		spdlog::info("[stealth] fake-death removed");
	}

	// ── 10. Admin bypass ──────────────────────────────────────────────────

	inline bool g_adminBypassInstalled = false;

	inline void SetupAdminBypass() {
		if (g_adminBypassInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_ADMIN_BYPASS);
		if (ok) {
			g_adminBypassInstalled = true;
			spdlog::info("[stealth] admin bypass installed");
		}
	}

	inline void UninstallAdminBypass() {
		if (!g_adminBypassInstalled) return;
		lualoader::Execute(luascripts::LUA_ADMIN_BYPASS_STOP);
		g_adminBypassInstalled = false;
		spdlog::info("[stealth] admin bypass removed");
	}

	// ── 11. AC bypass ─────────────────────────────────────────────────────

	inline void SetupACBypass() {
		if (g_antiACInstalled) return;  // reuse existing flag for base AC
		bool ok = lualoader::Execute(luascripts::LUA_AC_BYPASS);
		if (ok) {
			g_antiACInstalled = true;
			spdlog::info("[stealth] full AC bypass (gAC/CAC/Stack) installed");
		}
	}

	// ── 12. Spectator cloak (selective per-feature) ───────────────────────

	inline bool g_spectatorCloakInstalled = false;

	inline void SetupSpectatorCloak() {
		if (g_spectatorCloakInstalled) return;
		bool ok = lualoader::Execute(luascripts::LUA_SPECTATOR_CLOAK);
		if (ok) {
			g_spectatorCloakInstalled = true;
			spdlog::info("[stealth] spectator cloak installed");
		}
	}

	inline void UninstallSpectatorCloak() {
		if (!g_spectatorCloakInstalled) return;
		lualoader::Execute(luascripts::LUA_SPECTATOR_CLOAK_STOP);
		g_spectatorCloakInstalled = false;
		spdlog::info("[stealth] spectator cloak removed");
	}

	// ── 13. Reactive init (call every frame, installs lazily on toggle) ───

	inline void StealthInit() {
		// --- existing ---
		if (config::screenshot_cleaner)
			SetupScreenshotCleaner();
		else if (!config::screenshot_cleaner && g_screenshotCleanerInstalled)
			UninstallScreenshotCleaner();

		// base AC hook stripper (existing SetupAntiAC)
		if (config::anti_anticheat && !config::ac_bypass)
			SetupAntiAC();

		// --- new expanded screenshot cleaner ---
		if (config::anti_screenshot)
			SetupAntiScreenshot();
		else if (!config::anti_screenshot && g_antiScreenshotInstalled)
			UninstallAntiScreenshot();

		// --- anti-kick ---
		if (config::anti_kick)
			SetupAntiKick();
		else if (!config::anti_kick && g_antiKickInstalled)
			UninstallAntiKick();

		// --- name steal cycle ---
		if (config::name_steal_cycle)
			SetupNameSteal();
		else if (!config::name_steal_cycle && g_nameStealInstalled)
			UninstallNameSteal();

		// --- fake death timer ---
		if (config::fake_death)
			SetupFakeDeath();
		else if (!config::fake_death && g_fakeDeathInstalled)
			UninstallFakeDeath();

		// --- admin bypass ---
		if (config::admin_bypass)
			SetupAdminBypass();
		else if (!config::admin_bypass && g_adminBypassInstalled)
			UninstallAdminBypass();

		// --- full AC bypass (gAC/CAC/Stack) ---
		if (config::ac_bypass)
			SetupACBypass();

		// --- spectator cloak ---
		if (config::spectator_cloak)
			SetupSpectatorCloak();
		else if (!config::spectator_cloak && g_spectatorCloakInstalled)
			UninstallSpectatorCloak();
	}

} // namespace stealth
