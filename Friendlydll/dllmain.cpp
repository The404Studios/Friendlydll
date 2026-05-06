// dllmain.cpp : Defines the entry point for the DLL application.
#include "includes.hpp"
#include "interfaces/gameevents.hpp"

static FILE* file = {};

static BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_CLOSE_EVENT || event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        config::g_requestEject.store(true, std::memory_order_release);
        Sleep(2000);
        return TRUE;
    }
    return FALSE;
}

int EntryPoint(HMODULE hModule) {
    DamageEvent* dmgevent = nullptr;

    AllocConsole();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    SetConsoleTitleA("Friendlydll-GMOD Console");

    freopen_s(&file, "CONIN$", "r", stdin);
    freopen_s(&file, "CONOUT$", "w", stdout);

    auto errorLogSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Friendlydll_error.log");
    errorLogSink->set_level(spdlog::level::err);

    debug::Init();

    MH_Initialize();

    d3d9hook::Init();
    interfaces::Prepare();
    netvars::Init();
    netvars::Dump(); // writes gmod_netvars.txt + logs key offsets to console

    // Validate key offsets against hardcoded fallbacks
    {
        struct { const char* tbl; const char* prop; int fallback; const char* name; } checks[] = {
            { "DT_BaseEntity",           "m_iHealth",          0xD8,   "m_iHealth" },
            { "DT_BasePlayer",           "m_fFlags",           0x450,  "m_fFlags" },
            { "DT_LocalPlayerExclusive", "m_vecViewOffset[0]", 0x144,  "m_vecViewOffset" },
            { "DT_BaseEntity",           "m_vecVelocity[0]",   0x150,  "m_vecVelocity" },
            { "DT_LocalPlayerExclusive", "m_nTickBase",        0x2D30, "m_nTickBase" },
        };
        for (auto& c : checks) {
            int resolved = netvars::Get(c.tbl, c.prop);
            if (resolved && resolved != c.fallback)
                spdlog::warn("[offsets] {} SHIFTED: 0x{:X} -> 0x{:X}", c.name, c.fallback, resolved);
            else if (!resolved)
                spdlog::warn("[offsets] {} NOT FOUND, using fallback 0x{:X}", c.name, c.fallback);
            else
                spdlog::info("[offsets] {} OK at 0x{:X}", c.name, c.fallback);
        }
    }

    hooks::Init();
    lualoader::Init();
    lualoader::TryGrabLuaState();
    scheduler_system->init();

    scheduler_system->queueRepeating([] {
        //spdlog::default_logger()->info("[scheduler] ConVar tick!");

        const auto sv_cheats = interfaces::cvar->FindCommandBase("sv_cheats");
        const auto sv_allowcslua = interfaces::cvar->FindCommandBase("sv_allowcslua");
        const auto fov_desired = interfaces::cvar->FindCommandBase("fov_desired");

        if (fov_desired) fov_desired->InternalSetValue(config::fov);

        if (sv_cheats) {
            if (config::allowcheats) {
                sv_cheats->InternalSetValue(1);
            }
            else {
                sv_cheats->InternalSetValue(0);
            }
        }

        if (sv_allowcslua) {
            if (config::allowcslua) {
                sv_allowcslua->InternalSetValue(1);
            }
            else {
                sv_allowcslua->InternalSetValue(0);
            }
        }
    }, std::chrono::milliseconds(100));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    spdlog::default_logger()->info("Got IClientEntityList interface -> {}", (void*)interfaces::entityList);
    spdlog::default_logger()->info("Got IBaseClientDLL interface -> {}", (void*)interfaces::client);
    spdlog::default_logger()->info("Got GlobalVars interface -> {}", (void*)interfaces::globalVars);
    spdlog::default_logger()->info("Got ClientMode interface -> {}", (void*)interfaces::clientMode);
    spdlog::default_logger()->info("Got CLuaShared interface -> {}", (void*)interfaces::cluaShared);
    spdlog::default_logger()->info("Got ClientState interface -> {}", interfaces::clientState);
    spdlog::default_logger()->info("Got IPrediction interface -> {}", (void*)interfaces::prediction);
    spdlog::default_logger()->info("Got IEngineTrace interface -> {}", (void*)interfaces::trace);
    spdlog::default_logger()->info("Got ICvar interface -> {}", (void*)interfaces::cvar);
    spdlog::default_logger()->info("Got IGameEventsManager2 interface -> {}", (void*)interfaces::gameevent);
    spdlog::default_logger()->info("Got CGameMovement interface -> {}", (void*)interfaces::gamemovement);
    spdlog::default_logger()->info("Got IMatRenderContext interface -> {}", (void*)interfaces::matrenderctx);
    spdlog::default_logger()->info("Got CModelRender interface -> {}", (void*)interfaces::modelrender);
    spdlog::default_logger()->info("Got CMaterialSystem interface -> {}", (void*)interfaces::matsystem);
    spdlog::default_logger()->info("Got IMoveHelper interface -> {}", (void*)interfaces::movehelper);
    
    interfaces::engine->ClientCmd("play hl1/ambience/particle_suck1.wav");

    dmgevent = new DamageEvent();
    interfaces::gameevent->AddListener(static_cast<IGameEventListener2*>(dmgevent), "player_hurt", false);

    while (!(GetAsyncKeyState(VK_END) & 1) && !config::g_requestEject.load(std::memory_order_acquire)) {
        Sleep(25);
    };

    spdlog::info("[eject] Beginning safe shutdown...");

    config::g_inGame.store(false, std::memory_order_relaxed);
    config::luastate = nullptr;
    Sleep(100);

    if (dmgevent) {
        interfaces::gameevent->RemoveListener(static_cast<IGameEventListener2*>(dmgevent));
        delete dmgevent;
        dmgevent = nullptr;
    }

    scheduler_system->stop();
    hooks::Shutdown();
    Sleep(100);
    d3d9hook::Shutdown();

    MH_Uninitialize();

    spdlog::info("[eject] Shutdown complete, freeing DLL.");
    debug::Shutdown();

    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

int __stdcall DllMain(HMODULE hModule,DWORD reason,void*)
{
    DisableThreadLibraryCalls(hModule);
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)EntryPoint, hModule, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}