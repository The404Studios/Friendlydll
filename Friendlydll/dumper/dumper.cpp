/*
 * GMod Source Engine Dumper
 * Standalone DLL — inject into gmod.exe (x64).
 * BY FOURZEROFOUR
 * Output: %USERPROFILE%\Desktop\gmod_dump.txt
 *
 * Dumps:
 *   1. All ClientClass netvars with absolute offsets
 *   2. Key VMT tables (IBaseClientDLL, CModelRender, IClientModeNormal,
 *                       IClientRenderable, IClientNetworkable)
 *   3. A targeted offset table for every field Friendlydll uses
 *
 * Compile as a separate x64 DLL (no other dependencies needed).
 * Project settings: DLL, /std:c++latest, no PCH, no SDL checks.
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <format>
#include <fstream>
#include <thread>
#include <chrono>

#pragma comment(lib, "Psapi.lib")

// ---------------------------------------------------------------------------
// Source engine SDK structures (x64 layout)
// ---------------------------------------------------------------------------

struct RecvProp;

struct RecvTable {
    RecvProp*  m_pProps;
    int        m_nProps;
    int        _pad0;
    void*      m_pDecoder;
    char*      m_pNetTableName;
    bool       m_bInitialized;
    bool       m_bInMainList;
};

struct RecvProp {
    char*       m_pVarName;
    int         m_RecvType;
    int         m_Flags;
    int         m_StringBufferSize;
    bool        m_bInsideArray;
    char        _pad0[3];
    const void* m_pExtraData;
    RecvProp*   m_pArrayProp;
    void*       m_ArrayLengthProxy;
    void*       m_ProxyFn;
    void*       m_DataTableProxyFn;
    RecvTable*  m_pDataTable;
    int         m_Offset;
    int         m_ElementStride;
    int         m_nElements;
    int         _pad1;
    const char* m_pParentArrayPropName;
};

struct ClientClass {
    void*        m_pCreateFn;
    void*        m_pCreateEventFn;
    char*        m_pNetworkName;
    RecvTable*   m_pRecvTable;
    ClientClass* m_pNext;
    int          m_ClassID;
};

// ---------------------------------------------------------------------------
// Interface capture
// ---------------------------------------------------------------------------

static void* CaptureInterface(const char* module, const char* name) {
    HMODULE hMod = GetModuleHandleA(module);
    if (!hMod) return nullptr;
    using Fn = void*(__cdecl*)(const char*, int*);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(hMod, "CreateInterface"));
    if (!fn) return nullptr;
    return fn(name, nullptr);
}

template<typename R, typename... Args>
static R VCall(void* obj, int idx, Args... args) {
    using Fn = R(__fastcall*)(void*, Args...);
    return (*static_cast<Fn**>(obj))[idx](obj, args...);
}

// ---------------------------------------------------------------------------
// RecvTable walker
// ---------------------------------------------------------------------------

struct Prop { std::string key; int offset; };

static void WalkTable(RecvTable* table, int base, std::vector<Prop>& out) {
    if (!table || !table->m_pNetTableName) return;
    for (int i = 0; i < table->m_nProps; ++i) {
        RecvProp& p = table->m_pProps[i];
        if (!p.m_pVarName) continue;
        if (p.m_pVarName[0] >= '0' && p.m_pVarName[0] <= '9') continue;

        int absOff = base + p.m_Offset;

        if (p.m_pDataTable)
            WalkTable(p.m_pDataTable, absOff, out);

        out.push_back({ std::string(table->m_pNetTableName) + "::" + p.m_pVarName, absOff });
    }
}

// ---------------------------------------------------------------------------
// VMT dumper
// ---------------------------------------------------------------------------

static void DumpVmt(std::ofstream& f, void* iface, const char* name, int maxIdx) {
    if (!iface) { f << std::format("\nVMT {} — null interface\n", name); return; }
    auto** vmt = *reinterpret_cast<void***>(iface);
    f << std::format("\n=== VMT: {} @ {:p} ===\n", name, iface);
    for (int i = 0; i < maxIdx; ++i) {
        void* fn = nullptr;
        __try { fn = vmt[i]; } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
        if (!fn) break;
        f << std::format("  [{:3d}] {:p}\n", i, fn);
    }
}

// Dump VMT of the IClientRenderable sub-object (this+0x8 in x64)
static void DumpRenderableVmt(std::ofstream& f, void* entity, int maxIdx) {
    if (!entity) return;
    // IClientRenderable starts at entity+0x8 in the multiple-inheritance layout
    void* renderable = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entity) + 0x8);
    DumpVmt(f, renderable, "IClientRenderable (entity+0x8)", maxIdx);
}

// Dump VMT of the IClientNetworkable sub-object (this+0x10 in x64)
static void DumpNetworkableVmt(std::ofstream& f, void* entity, int maxIdx) {
    if (!entity) return;
    void* networkable = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entity) + 0x10);
    DumpVmt(f, networkable, "IClientNetworkable (entity+0x10)", maxIdx);
}

// ---------------------------------------------------------------------------
// Main dump routine
// ---------------------------------------------------------------------------

static void DoDump() {
    // Brief delay so game has time to fully load its modules
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Build output path on Desktop
    char desktop[MAX_PATH]{};
    SHGetFolderPathA(nullptr, 0x0010 /*CSIDL_DESKTOPDIRECTORY*/, nullptr, 0, desktop);
    std::string outPath = std::string(desktop) + "\\gmod_dump.txt";

    std::ofstream f(outPath, std::ios::trunc);
    if (!f) {
        MessageBoxA(nullptr, "gmod_dump.txt: failed to create", "Dumper", MB_OK);
        return;
    }

    // Capture interfaces
    void* client     = CaptureInterface("client.dll",         "VClient017");
    void* entityList = CaptureInterface("client.dll",         "VClientEntityList003");
    void* engine     = CaptureInterface("engine.dll",         "VEngineClient015");
    void* modelRender= CaptureInterface("engine.dll",         "VEngineModel016");
    void* matsys     = CaptureInterface("materialsystem.dll", "VMaterialSystem080");

    f << std::format("client.dll         IBaseClientDLL    @ {:p}\n", client);
    f << std::format("client.dll         IClientEntityList @ {:p}\n", entityList);
    f << std::format("engine.dll         IVEngineClient    @ {:p}\n", engine);
    f << std::format("engine.dll         CModelRender      @ {:p}\n", modelRender);
    f << std::format("materialsystem.dll CMaterialSystem   @ {:p}\n\n", matsys);

    // -----------------------------------------------------------------------
    // Netvar dump
    // -----------------------------------------------------------------------
    f << "=== NETVARS (all ClientClass props, sorted by offset) ===\n\n";

    // IBaseClientDLL::GetAllClasses() is at VMT index 8
    ClientClass* head = nullptr;
    if (client)
        head = VCall<ClientClass*>(client, 8);

    std::unordered_map<std::string, int> netvarMap;

    for (auto* cc = head; cc; cc = cc->m_pNext) {
        if (!cc->m_pRecvTable) continue;

        f << std::format("ClientClass: {:40s} (ID={:4d})  table={}\n",
            cc->m_pNetworkName ? cc->m_pNetworkName : "?",
            cc->m_ClassID,
            cc->m_pRecvTable->m_pNetTableName ? cc->m_pRecvTable->m_pNetTableName : "?");

        std::vector<Prop> props;
        WalkTable(cc->m_pRecvTable, 0, props);
        std::sort(props.begin(), props.end(), [](auto& a, auto& b){ return a.offset < b.offset; });

        for (auto& p : props) {
            f << std::format("    {:<60} 0x{:08X}  ({})\n", p.key, p.offset, p.offset);
            netvarMap[p.key] = p.offset;
        }
        f << "\n";
    }

    // -----------------------------------------------------------------------
    // Targeted offsets used by Friendlydll
    // -----------------------------------------------------------------------
    f << "=== FRIENDLYDLL KEY OFFSETS ===\n";

    struct Want { const char* table; const char* prop; const char* usage; };
    static const Want wanted[] = {
        { "DT_BasePlayer",           "m_iHealth",             "GetHealth()"      },
        { "DT_BasePlayer",           "m_fFlags",              "GetFlags()"       },
        { "DT_BasePlayer",           "m_hActiveWeapon",       "GetActiveWeapon" },
        { "DT_LocalPlayerExclusive", "m_vecViewOffset[0]",    "GetViewOffset()"  },
        { "DT_LocalPlayerExclusive", "m_vecAbsVelocity",      "GetVelocity()"    },
        { "DT_LocalPlayerExclusive", "m_nTickBase",           "GetTickBase()"    },
        { "DT_LocalPlayerExclusive", "m_aimPunchAngle",       "GetAimPunch()"    },
        { "DT_LocalPlayerExclusive", "m_viewPunchAngle",      "GetViewPunch()"   },
        { "DT_BaseEntity",           "m_vecOrigin",           "GetAbsOrigin()"   },
    };

    for (auto& w : wanted) {
        std::string key = std::string(w.table) + "::" + w.prop;
        int off = 0;
        auto it = netvarMap.find(key);
        if (it != netvarMap.end()) off = it->second;

        if (off)
            f << std::format("  FOUND   {:<22s} {:<40s} 0x{:X} ({})\n", w.usage, key, off, off);
        else
            f << std::format("  MISSING {:<22s} {:<40s} — search dump above for correct name\n", w.usage, key);
    }

    // -----------------------------------------------------------------------
    // VMT dumps for all key interfaces
    // -----------------------------------------------------------------------
    DumpVmt(f, client,      "IBaseClientDLL (VClient017)",    60);
    DumpVmt(f, modelRender, "CModelRender (VEngineModel016)", 30);
    DumpVmt(f, matsys,      "CMaterialSystem",                30);

    // Get a live entity to dump its sub-object VMTs (IClientRenderable/Networkable)
    if (entityList) {
        // Index 1 = first player slot; use whichever non-null entity we find
        for (int i = 1; i <= 64; ++i) {
            void* ent = VCall<void*>(entityList, 3, i); // GetClientEntity(i) at VMT index 3
            if (!ent) continue;

            f << std::format("\n=== Sub-object VMTs from entity[{}] @ {:p} ===\n", i, ent);
            f << "  (IClientUnknown is at entity+0x0, IClientRenderable at +0x8, IClientNetworkable at +0x10)\n";
            DumpRenderableVmt(f, ent, 25);
            DumpNetworkableVmt(f, ent, 20);

            // Dump raw field bytes at key offsets for local-player sanity check
            f << "\n  Raw bytes at common offsets (first found non-null entity — NOT necessarily local player):\n";
            auto readInt = [&](int offset) -> std::string {
                __try {
                    int v = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(ent) + offset);
                    return std::format("0x{:X} ({})", v, v);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    return "ACCESS_VIOLATION";
                }
            };
            for (int off : { 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xC0, 0xC4, 0xC8, 0xCC, 0xD0 }) {
                f << std::format("    [this+0x{:X}] = {}\n", off, readInt(off));
            }
            break;
        }
    }

    // -----------------------------------------------------------------------
    // IClientModeNormal (need to extract from IBaseClientDLL internal)
    // -----------------------------------------------------------------------
    // clientMode is retrieved from client.dll via a VMT walk in the main cheat.
    // We can't easily replicate that here without the full GetVMT helper,
    // so we log a reminder instead.
    f << "\n=== NOTE ===\n";
    f << "IClientModeNormal address not captured here (requires internal GetVMT scan).\n";
    f << "Run Friendlydll first — it logs 'Got ClientMode interface' with the address.\n";
    f << "Then use x64dbg to inspect that vtable manually if CreateMove index needs verifying.\n";

    f << "\n=== DONE ===\n";
    f.close();

    MessageBoxA(nullptr,
        ("Dump complete!\n\n" + outPath).c_str(),
        "GMod Dumper", MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// DLL entry point
// ---------------------------------------------------------------------------

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0,
            [](void*) -> DWORD { DoDump(); return 0; },
            nullptr, 0, nullptr);
    }
    return TRUE;
}
