#include "lua_loader.hpp"

#define lua_pop(L,n) lua_settop(L, -(n)-1)

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCSETPAUSE		6
#define LUA_GCSETSTEPMUL	7

#define LUA_REGISTRYINDEX -10002
#define LUA_GLOBALSINDEX (-10002)

uintptr_t thread;

void lualoader::TryGrabLuaState() noexcept
{
	if (config::luastate) return;
	if (!interfaces::cluaShared) return;

	CLuaInterface* cli = interfaces::cluaShared->GetLuaInterface(LUAINTERFACE_CLIENT);
	if (!cli) return;

	uintptr_t* vtable = *(uintptr_t**)cli;
	uint8_t* topFn = (uint8_t*)vtable[0];

	// Follow jmp thunk if present (incremental linking)
	if (topFn[0] == 0xE9) {
		int32_t rel = *(int32_t*)(topFn + 1);
		topFn = topFn + 5 + rel;
	}

	// Disassemble Top() to find the offset where it loads lua_State* from this (rcx)
	// Looking for: REX.W MOV reg, [rcx + offset]  →  48 8B ?? ??
	int stateOffset = -1;
	for (int i = 0; i < 48 && stateOffset < 0; ++i) {
		if ((topFn[i] & 0xF9) != 0x48) continue; // REX.W without REX.B
		if (topFn[i + 1] != 0x8B) continue;       // MOV

		uint8_t modrm = topFn[i + 2];
		uint8_t mod = (modrm >> 6) & 3;
		uint8_t rm  = modrm & 7;

		if (rm != 1) continue; // base register must be rcx (this)

		if (mod == 1) {
			stateOffset = (int)(int8_t)topFn[i + 3];
			break;
		}
		else if (mod == 2) {
			stateOffset = *(int32_t*)(topFn + i + 3);
			break;
		}
	}

	if (stateOffset < 0) {
		spdlog::warn("[lua] Could not find lua_State offset in CLuaInterface::Top()");
		return;
	}

	lua_State* state = *(lua_State**)((uintptr_t)cli + stateOffset);
	if (!state) {
		spdlog::warn("[lua] lua_State at CLuaInterface+{:#x} is null", stateOffset);
		return;
	}

	config::luastate = state;
	spdlog::info("[lua] Grabbed client Lua state from CLuaInterface+{:#x}: {}", stateOffset, (void*)state);
}

void lualoader::Init() noexcept
{
	h_luashared = GetModuleHandleA("lua_shared.dll");

	luaL_loadfile = reinterpret_cast<_luaL_loadfile>(GetProcAddress(h_luashared, "luaL_loadfile"));
	lua_tolstring = reinterpret_cast<_lua_tolstring>(GetProcAddress(h_luashared, "lua_tolstring"));
	lua_settop = reinterpret_cast<_lua_settop>(GetProcAddress(h_luashared, "lua_settop"));
	lua_gettop = reinterpret_cast<_lua_gettop>(GetProcAddress(h_luashared, "lua_gettop"));
	lua_newthread = reinterpret_cast<_lua_newthread>(GetProcAddress(h_luashared, "lua_newthread"));
	lua_pushvalue = reinterpret_cast<_lua_pushvalue>(GetProcAddress(h_luashared, "lua_pushvalue"));
	lua_xmove = reinterpret_cast<_lua_xmove>(GetProcAddress(h_luashared, "lua_xmove"));
	lua_setfenv = reinterpret_cast<_lua_setfenv>(GetProcAddress(h_luashared, "lua_setfenv"));
	luaL_loadstring = reinterpret_cast<_luaL_loadstring>(GetProcAddress(h_luashared, "luaL_loadstring"));
	lua_pcall = reinterpret_cast<_lua_pcall>(GetProcAddress(h_luashared, "lua_pcall"));
	lua_gc = reinterpret_cast<_lua_gc>(GetProcAddress(h_luashared, "lua_gc"));
	luaL_loadbuffer = reinterpret_cast<_luaL_loadbuffer>(GetProcAddress(h_luashared, "luaL_loadbuffer"));
}

auto getallthreads() {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	std::vector<DWORD> threads = {};

	if (snapshot == INVALID_HANDLE_VALUE)
		return threads;

	THREADENTRY32 te32 = {};
	te32.dwSize = sizeof(THREADENTRY32);

	if (Thread32First(snapshot, &te32)) {
		do {
			if (te32.th32OwnerProcessID == GetCurrentProcessId()) {
				threads.push_back(te32.th32ThreadID);
			}
		} while (Thread32Next(snapshot, &te32));
	}

	CloseHandle(snapshot);
	return threads;
}

void callbacktest(const void*) {
	spdlog::default_logger()->info("Callback called!");
}

void lualoader::ProcessQueue() noexcept
{
	if (!g_hasPending.load(std::memory_order_acquire)) return;
	g_hasPending.store(false, std::memory_order_relaxed);

	if (!config::luastate) {
		g_lastError = "No Lua state";
		g_hasResult.store(true, std::memory_order_release);
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
	const auto& script = g_pendingScript;
	if (luaL_loadbuffer(config::luastate, script.c_str(), script.size(), "fdll_console") != 0) {
		size_t len = 0;
		const char* err = lua_tolstring(config::luastate, -1, &len);
		g_lastError = err ? std::string(err, len) : "load error";
		lua_pop(config::luastate, 1);
		g_hasResult.store(true, std::memory_order_release);
		return;
	}
	if (lua_pcall(config::luastate, 0, 1, 0) != 0) {
		size_t len = 0;
		const char* err = lua_tolstring(config::luastate, -1, &len);
		g_lastError = err ? std::string(err, len) : "runtime error";
		lua_pop(config::luastate, 1);
		g_hasResult.store(true, std::memory_order_release);
		return;
	}
	size_t len = 0;
	const char* res = lua_tolstring(config::luastate, -1, &len);
	g_lastOutput = res ? std::string(res, len) : "(ok)";
	lua_pop(config::luastate, 1);
	g_lastError.clear();
	g_hasResult.store(true, std::memory_order_release);
}

void lualoader::ProcessPendingRuns() noexcept
{
	std::vector<PendingRun> batch;
	{
		std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
		if (g_pendingRuns.empty()) return;
		batch.swap(g_pendingRuns);
	}
	for (auto& pr : batch) {
		if (pr.resultDest) {
			*pr.resultDest = ExecuteAndGetResult(pr.script);
		} else {
			Execute(pr.script);
		}
	}
}

lua_State* execThread = nullptr;
bool lualoader::Execute(std::string script) noexcept
{
	if (!config::luastate)
		return false;

	if (!interfaces::engine || !interfaces::engine->IsInGame())
		return false;

	std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
	if (!config::luastate) return false;

	int top = lua_gettop(config::luastate);
	if (luaL_loadbuffer(config::luastate, script.c_str(), script.size(), "Friendlydll_lua_loader") != 0) {
		lua_settop(config::luastate, top);
		return false;
	}
	if (lua_pcall(config::luastate, 0, -1, 0) != 0) {
		lua_settop(config::luastate, top);
		return false;
	}

	return true;
}

std::string lualoader::ExecuteAndGetResult(const std::string& script) noexcept
{
	if (!config::luastate) return "";
	if (!interfaces::engine || !interfaces::engine->IsInGame()) return "";
	std::lock_guard<std::recursive_mutex> lock(g_luaMutex);
	if (!config::luastate) return "";
	int top = lua_gettop(config::luastate);
	if (luaL_loadbuffer(config::luastate, script.c_str(), script.size(), "fdll_query") != 0) {
		lua_settop(config::luastate, top);
		return "";
	}
	if (lua_pcall(config::luastate, 0, 1, 0) != 0) {
		lua_settop(config::luastate, top);
		return "";
	}
	size_t len = 0;
	const char* result = lua_tolstring(config::luastate, -1, &len);
	std::string ret = result ? std::string(result, len) : "";
	lua_settop(config::luastate, top);
	return ret;
}

bool lualoader::ExecuteFile(std::string path) noexcept
{
	if (!config::luastate)
		return false;

	if (interfaces::engine->IsInGame()) {
		if (luaL_loadfile(config::luastate, path.c_str()) != 0) {
			lua_pop(config::luastate, 1);
			return false;
		}
		if (lua_pcall(config::luastate, 0, -1, 0) != 0) {
			lua_pop(config::luastate, 1);
			return false;
		}

		return true;
	}
	else {
		return false;
	}
}