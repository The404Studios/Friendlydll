#include "netvars.hpp"
#include "../includes.hpp"

#include <fstream>
#include <format>
#include <algorithm>
#include <vector>

namespace netvars {

// "DT_TableName::m_propName" -> absolute offset from object base
static std::unordered_map<std::string, int> g_map;

// Walk one RecvTable recursively.
// baseOffset = accumulated offset from the entity base to the current sub-table.
static void WalkTable(RecvTable* table, int baseOffset, std::vector<std::pair<std::string,int>>* dumpList) {
    if (!table || !table->m_pNetTableName) return;

    for (int i = 0; i < table->m_nProps; ++i) {
        RecvProp& prop = table->m_pProps[i];
        if (!prop.m_pVarName) continue;

        // Skip the internal array-element entries ("000", "001", ...)
        if (prop.m_pVarName[0] >= '0' && prop.m_pVarName[0] <= '9') continue;

        int absOffset = baseOffset + prop.m_Offset;

        if (prop.m_pDataTable) {
            // Recurse into nested table at the accumulated offset
            WalkTable(prop.m_pDataTable, absOffset, dumpList);
        }

        // Store with the IMMEDIATE parent table name (standard lookup convention)
        std::string key = std::string(table->m_pNetTableName) + "::" + prop.m_pVarName;
        g_map[key] = absOffset;

        if (dumpList) {
            dumpList->push_back({ key, absOffset });
        }
    }
}

void Init() noexcept {
    g_map.clear();

    // IBaseClientDLL::GetAllClasses() is at vtable index 8
    auto head = mem::Call<ClientClass*>(interfaces::client, 8);
    if (!head) {
        spdlog::default_logger()->error("[netvars] GetAllClasses returned null");
        return;
    }

    int classCount = 0;
    for (auto cc = head; cc; cc = cc->m_pNext) {
        if (cc->m_pRecvTable)
            WalkTable(cc->m_pRecvTable, 0, nullptr);
        ++classCount;
    }

    spdlog::default_logger()->info("[netvars] Initialized {} classes, {} props", classCount, g_map.size());
}

void Dump() noexcept {
    std::ofstream f("gmod_netvars.txt", std::ios::trunc);
    if (!f) {
        spdlog::default_logger()->error("[netvars] Failed to open gmod_netvars.txt for writing");
        return;
    }

    // Rebuild with a list so we can sort + dump
    auto head = mem::Call<ClientClass*>(interfaces::client, 8);
    if (!head) return;

    std::vector<std::pair<std::string, int>> entries;
    entries.reserve(g_map.size());

    for (auto cc = head; cc; cc = cc->m_pNext) {
        if (!cc->m_pRecvTable) continue;

        // Print class header
        auto classLine = std::format("ClientClass: {} (ClassID={}, Table={})\n",
            cc->m_pNetworkName ? cc->m_pNetworkName : "?",
            cc->m_ClassID,
            cc->m_pRecvTable->m_pNetTableName ? cc->m_pRecvTable->m_pNetTableName : "?");

        f << classLine;
        spdlog::default_logger()->info(classLine);

        std::vector<std::pair<std::string,int>> tableEntries;
        WalkTable(cc->m_pRecvTable, 0, &tableEntries);

        // Sort by offset for readability
        std::sort(tableEntries.begin(), tableEntries.end(),
            [](const auto& a, const auto& b){ return a.second < b.second; });

        for (auto& [key, off] : tableEntries) {
            auto line = std::format("  {:<60} 0x{:X} ({})\n", key, off, off);
            f << line;
        }
        f << "\n";
    }

    // Print the specific offsets the cheat uses (correct GMod prop names)
    const char* wantedProps[][2] = {
        { "DT_BaseEntity",            "m_iHealth"           },
        { "DT_BasePlayer",            "m_fFlags"            },
        { "DT_BaseCombatCharacter",   "m_hActiveWeapon"     },
        { "DT_LocalPlayerExclusive",  "m_vecViewOffset[0]"  },
        { "DT_BaseEntity",            "m_vecVelocity[0]"    },
        { "DT_LocalPlayerExclusive",  "m_nTickBase"         },
        { "DT_Local",                 "m_vecPunchAngle"     },
        { "DT_Local",                 "m_vecPunchAngleVel"  },
        { "DT_BaseEntity",            "m_vecOrigin"         },
        { "DT_BaseEntity",            "m_vecOrigin"         },
    };

    f << "\n=== KEY OFFSETS USED BY FRIENDLYDLL ===\n";
    spdlog::default_logger()->info("[netvars] === KEY OFFSETS ===");
    for (auto& pair : wantedProps) {
        int off = Get(pair[0], pair[1]);
        auto line = std::format("  {}::{:<40} 0x{:X} ({})\n", pair[0], pair[1], off, off);
        f << line;
        spdlog::default_logger()->info("[netvars] {}", line);
    }

    // VMT dump helpers for key interfaces
    auto dumpVmt = [&](void* iface, const char* name, int maxEntries) {
        if (!iface) return;
        f << std::format("\n=== VMT: {} @ {:p} ===\n", name, iface);
        auto** vmt = *reinterpret_cast<void***>(iface);
        for (int i = 0; i < maxEntries; ++i) {
            if (!vmt[i]) break;
            f << std::format("  [{:3d}] {:p}\n", i, vmt[i]);
        }
    };

    dumpVmt(interfaces::client,      "IBaseClientDLL",    50);
    dumpVmt(interfaces::modelrender, "CModelRender",      30);
    dumpVmt(interfaces::clientMode,  "IClientModeNormal", 30);
    dumpVmt(interfaces::matsystem,   "CMaterialSystem",   30);

    spdlog::default_logger()->info("[netvars] Dump written to gmod_netvars.txt");
}

int Get(const char* tableName, const char* propName) noexcept {
    auto key = std::string(tableName) + "::" + propName;
    auto it = g_map.find(key);
    if (it != g_map.end()) return it->second;

    spdlog::default_logger()->warn("[netvars] MISS: {}::{}", tableName, propName);
    return 0;
}

} // namespace netvars
