#pragma once
#include "../includes.hpp"
#include <mutex>
#include <vector>

namespace lualoader {
	inline HMODULE h_luashared = nullptr;
	inline std::recursive_mutex g_luaMutex;

	inline CLuaInterface* luaInterface = nullptr;
	inline uintptr_t gLuaState = 0;

	using _luaL_loadfile = int(__cdecl*)(lua_State* luaState, const char* file);
	inline _luaL_loadfile luaL_loadfile = nullptr;

	using _lua_tolstring = const char*(__cdecl*)(lua_State* luaState, int index, size_t* len);
	inline _lua_tolstring lua_tolstring = nullptr;

	using _lua_settop = void(__cdecl*)(lua_State* luaState, int index);
	inline _lua_settop lua_settop = nullptr;

	using _lua_gettop = int(__cdecl*)(lua_State* luaState);
	inline _lua_gettop lua_gettop = nullptr;

	using _lua_newthread = lua_State * (__cdecl*)(lua_State* luaState);
	inline _lua_newthread lua_newthread = nullptr;

	using _lua_pushvalue = void(__cdecl*)(lua_State* luaState, int index);
	inline _lua_pushvalue lua_pushvalue = nullptr;

	using _lua_xmove = void(__cdecl*)(lua_State* luaState, lua_State* luaThread, int index);
	inline _lua_xmove lua_xmove = nullptr;

	using _lua_setfenv = void(__cdecl*)(lua_State* luaState, int index);
	inline _lua_setfenv lua_setfenv = nullptr;

	using _luaL_loadstring = int(__cdecl*)(lua_State* luaState, const char* string);
	inline _luaL_loadstring luaL_loadstring = nullptr;

	using _lua_pcall = int(__cdecl*)(lua_State* luaState, int nargs, int nresults, int msgh);
	inline _lua_pcall lua_pcall = nullptr;

	using _lua_gc = int(__cdecl*)(lua_State* luaState, int what, int data);
	inline _lua_gc lua_gc = nullptr;

	using _luaL_loadbuffer = int(__cdecl*)(lua_State* luaState, const char* buffer, size_t sz, const char* name);
	inline _luaL_loadbuffer luaL_loadbuffer = nullptr;

	inline std::string g_pendingScript;
	inline std::string g_lastError;
	inline std::string g_lastOutput;
	inline std::atomic<bool> g_hasPending{false};
	inline std::atomic<bool> g_hasResult{false};

	inline void QueueScript(const std::string& script) {
		std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
		g_pendingScript = script;
		g_lastError.clear();
		g_lastOutput.clear();
		g_hasResult.store(false, std::memory_order_relaxed);
		g_hasPending.store(true, std::memory_order_release);
	}

	struct PendingRun {
		std::string script;
		std::string* resultDest;
	};
	inline std::vector<PendingRun> g_pendingRuns;

	inline void QueueRun(const std::string& script) {
		std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
		g_pendingRuns.push_back({script, nullptr});
	}

	inline void QueueQuery(const std::string& script, std::string* dest) {
		std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
		g_pendingRuns.push_back({script, dest});
	}

	void ProcessQueue() noexcept;
	void ProcessPendingRuns() noexcept;

	void Init() noexcept;
	void TryGrabLuaState() noexcept;
	void CleanupLua(uintptr_t state) noexcept;
	void ForceCleanupLua(uintptr_t state) noexcept;
	bool Execute(std::string script) noexcept;
	bool ExecuteFile(std::string path) noexcept;
	std::string ExecuteAndGetResult(const std::string& script) noexcept;
}