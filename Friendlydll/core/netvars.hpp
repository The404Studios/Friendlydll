#pragma once
#include <unordered_map>
#include <string>

// Forward-declare Source SDK RecvTable structures (x64 layout)
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

namespace netvars {
    // Build the offset map by walking all ClientClass RecvTables.
    // Call once after interfaces::Prepare().
    void Init() noexcept;

    // Dump everything to console + gmod_netvars.txt
    void Dump() noexcept;

    // Look up a prop offset by table name and prop name.
    // Returns 0 and logs a warning if not found.
    // Example: netvars::Get("DT_BasePlayer", "m_iHealth")
    int Get(const char* tableName, const char* propName) noexcept;
}
