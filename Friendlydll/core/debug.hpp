#pragma once
#include <Windows.h>
#include <atomic>

// Breadcrumb state — written before risky operations so the VEH crash report
// shows exactly which phase and entity the DLL was processing when it died.
namespace debug {

enum class Phase : uint32_t {
    Idle        = 0,
    CreateMove  = 1,
    EspLoop     = 2,
    Aimbot      = 3,
    DrawModel   = 4,
    BoneCache   = 5,
};

inline std::atomic<Phase>    lastPhase   { Phase::Idle };
inline std::atomic<int32_t>  lastEntity  { -1 };
inline std::atomic<void*>    lastEntityPtr { nullptr };

inline const char* PhaseName(Phase p) noexcept {
    switch (p) {
    case Phase::CreateMove: return "CreateMove";
    case Phase::EspLoop:    return "ESP entity loop";
    case Phase::Aimbot:     return "Aimbot";
    case Phase::DrawModel:  return "DrawModelExecute";
    case Phase::BoneCache:  return "BoneCache";
    default:                return "Idle";
    }
}

// RAII breadcrumb — sets phase on entry, resets to Idle on exit.
struct Breadcrumb {
    explicit Breadcrumb(Phase p) noexcept { lastPhase.store(p, std::memory_order_relaxed); }
    ~Breadcrumb() noexcept {
        lastPhase.store(Phase::Idle, std::memory_order_relaxed);
        lastEntity.store(-1, std::memory_order_relaxed);
        lastEntityPtr.store(nullptr, std::memory_order_relaxed);
    }
};

static LONG WINAPI VEH_Handler(PEXCEPTION_POINTERS ex) noexcept {
    const DWORD code = ex->ExceptionRecord->ExceptionCode;

    // Ignore C++ exceptions, breakpoints, and single-step — let the runtime handle those.
    if (code == 0xE06D7363u)          return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;

    // Build report into a static buffer — no heap allocation during a crash.
    static char buf[2048];
    const auto* ctx = ex->ContextRecord;
    const Phase phase = lastPhase.load(std::memory_order_relaxed);
    const int32_t ent = lastEntity.load(std::memory_order_relaxed);
    const void* entPtr = lastEntityPtr.load(std::memory_order_relaxed);

    int len = 0;

#ifdef _WIN64
    #define FDLL_IP ctx->Rip
    #define FDLL_SP ctx->Rsp
    #define FDLL_AX ctx->Rax
    #define FDLL_CX ctx->Rcx
    #define FDLL_DX ctx->Rdx
    #define FDLL_REG_FMT "0x%016llX"
    #define FDLL_REG_CAST(x) (unsigned long long)(x)
#else
    #define FDLL_IP ctx->Eip
    #define FDLL_SP ctx->Esp
    #define FDLL_AX ctx->Eax
    #define FDLL_CX ctx->Ecx
    #define FDLL_DX ctx->Edx
    #define FDLL_REG_FMT "0x%08X"
    #define FDLL_REG_CAST(x) (unsigned int)(x)
#endif

    if (code == EXCEPTION_ACCESS_VIOLATION && ex->ExceptionRecord->NumberParameters >= 2) {
        len = sprintf_s(buf, sizeof(buf),
            "=== FRIENDLYDLL CRASH ===\r\n"
            "Code:      0x%08X (ACCESS_VIOLATION)\r\n"
            "AV type:   %s\r\n"
            "AV addr:   " FDLL_REG_FMT "\r\n"
            "Fault IP:  " FDLL_REG_FMT "\r\n"
            "Thread:    0x%08X\r\n"
            "Phase:     %s\r\n"
            "Entity:    %d  ptr=0x%p\r\n"
            "IP=" FDLL_REG_FMT "  SP=" FDLL_REG_FMT "\r\n"
            "AX=" FDLL_REG_FMT "  CX=" FDLL_REG_FMT "  DX=" FDLL_REG_FMT "\r\n"
            "\r\n",
            code,
            ex->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "READ",
            FDLL_REG_CAST(ex->ExceptionRecord->ExceptionInformation[1]),
            FDLL_REG_CAST(ex->ExceptionRecord->ExceptionAddress),
            GetCurrentThreadId(),
            PhaseName(phase),
            ent, entPtr,
            FDLL_REG_CAST(FDLL_IP), FDLL_REG_CAST(FDLL_SP),
            FDLL_REG_CAST(FDLL_AX), FDLL_REG_CAST(FDLL_CX), FDLL_REG_CAST(FDLL_DX)
        );
    } else {
        len = sprintf_s(buf, sizeof(buf),
            "=== FRIENDLYDLL CRASH ===\r\n"
            "Code:      0x%08X\r\n"
            "Fault IP:  " FDLL_REG_FMT "\r\n"
            "Thread:    0x%08X\r\n"
            "Phase:     %s\r\n"
            "Entity:    %d  ptr=0x%p\r\n"
            "IP=" FDLL_REG_FMT "  SP=" FDLL_REG_FMT "\r\n"
            "AX=" FDLL_REG_FMT "  CX=" FDLL_REG_FMT "  DX=" FDLL_REG_FMT "\r\n"
            "\r\n",
            code,
            FDLL_REG_CAST(ex->ExceptionRecord->ExceptionAddress),
            GetCurrentThreadId(),
            PhaseName(phase),
            ent, entPtr,
            FDLL_REG_CAST(FDLL_IP), FDLL_REG_CAST(FDLL_SP),
            FDLL_REG_CAST(FDLL_AX), FDLL_REG_CAST(FDLL_CX), FDLL_REG_CAST(FDLL_DX)
        );
    }

    #undef FDLL_IP
    #undef FDLL_SP
    #undef FDLL_AX
    #undef FDLL_CX
    #undef FDLL_DX
    #undef FDLL_REG_FMT
    #undef FDLL_REG_CAST

    if (len > 0) {
        HANDLE hFile = CreateFileA(
            "friendlydll_crash.log",
            GENERIC_WRITE, FILE_SHARE_READ,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            SetFilePointer(hFile, 0, nullptr, FILE_END);
            DWORD written;
            WriteFile(hFile, buf, (DWORD)len, &written, nullptr);
            CloseHandle(hFile);
        }
    }

    return EXCEPTION_CONTINUE_SEARCH; // let the game's own handler run too
}

inline void* g_vehHandle = nullptr;

inline void Init() noexcept {
    // First in chain so we see the crash before any other handler suppresses it.
    g_vehHandle = AddVectoredExceptionHandler(1, VEH_Handler);
}

inline void Shutdown() noexcept {
    if (g_vehHandle) {
        RemoveVectoredExceptionHandler(g_vehHandle);
        g_vehHandle = nullptr;
    }
}

} // namespace debug
