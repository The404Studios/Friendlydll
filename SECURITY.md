# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Current `main` branch | Yes |
| Older builds | No |

## Reporting a Vulnerability

If you discover a security vulnerability in Friendlydll, please report it responsibly.

### What qualifies

- Remote code execution via crafted game server responses (malicious net messages, entity data, or Lua payloads that escape the cheat's pcall sandboxing)
- Memory corruption in the hook chain (CreateMove, DrawModelExecute, Present) that could be triggered by a hostile server or another client-side module
- Credential or token leakage (Steam auth tokens, session data, or local file contents exposed through Lua execution, net sniffing, or file I/O)
- DLL hijacking or injection into Friendlydll's own load chain
- Privilege escalation from the cheat's Lua sandbox into arbitrary OS-level code execution
- Unsafe deserialization in config loading (`config_io.hpp`) that allows code execution via crafted config files
- Log injection via `spdlog` sinks (`Friendlydll_error.log`) that could be leveraged if logs are consumed by other tools

### How to report

1. **Do not open a public issue.** Security vulnerabilities must be reported privately.
2. Email **the404studios@gmail.com** with:
   - A description of the vulnerability
   - Steps to reproduce or a proof of concept
   - Affected module(s) and file path(s)
   - Severity assessment (critical / high / medium / low)
3. You will receive an acknowledgment within 72 hours.
4. A fix will be developed privately and released without prior public disclosure of the details.

### What to expect

- **Acknowledgment**: within 72 hours
- **Triage**: within 7 days (confirmation of validity and severity)
- **Fix timeline**: critical issues within 14 days, others within 30 days
- **Disclosure**: coordinated disclosure after the fix is released

## Threat Model

Friendlydll operates in a hostile environment by design. The primary threat actors and attack surfaces are:

### 1. Malicious game servers

The DLL connects to untrusted Garry's Mod servers that control:
- Net messages processed by the Lua engine and duplication exploit systems
- Entity data parsed by the intel pipeline (`intel_lua.hpp`) and entity ESP
- ConVar values read by the scheduler loop
- Game events consumed by the `player_hurt` listener

**Mitigations**:
- All Lua execution is wrapped in `pcall()` to prevent server-delivered errors from crashing the client or leaking stack traces
- The error sanitizer (`exploit_batch_v3.hpp`) filters console output containing cheat-related keywords
- The stack trace spoof rewrites `debug.traceback` / `debug.getinfo` to hide injection points
- Entity data parsing uses fixed-size buffers with bounds checking (512 entity slots, 128 player slots, 64-char labels, 32-char names)

### 2. Anti-cheat systems

Server-side and client-side anti-cheat addons (gAC, CAC, StackAC, custom) attempt to detect the DLL through:
- Hook table enumeration (`hook.GetTable`, `timer.GetTable`)
- File system scanning (`file.Exists`, `file.Size`)
- Integrity checks (`util.CRC`, `util.MD5`)
- Stack inspection (`debug.traceback`, `debug.getinfo`)
- Screenshot capture (`render.Capture`, `render.ReadPixels`)
- Admin tool player list enumeration

**Mitigations**:
- Hook table sanitizer filters `_fdll_` prefixed entries from `hook.GetTable` and `timer.GetTable`
- CRC spoof caches clean values and hides cheat-related file paths
- Stack trace spoof replaces RunString/CompileString sources with legitimate autorun paths
- Screenshot cleaner returns nil from render capture functions
- Anti-AC strips existing detection hooks and detours `hook.Add` to block future registration
- Admin bypass hides from ULX/SAM/FAdmin/ServerGuard player lists
- All Lua globals use `_fdll_` prefix with idempotent installation guards

### 3. Spectating players and admins

Human observers can detect the cheat through:
- Visual artifacts (ESP overlays, chams, skeleton rendering)
- Behavioral anomalies (inhuman aim, anti-aim, bunny hop patterns)
- Admin tools (spectator mode, screen capture)

**Mitigations**:
- Spectator detection scans observer mode/target each frame and counts spectators
- Auto-panic disables all visible features when spectated
- Spectator cloak provides selective per-feature disable (less aggressive than full panic)
- Recording mode disables all overlays for clean footage
- Panic key (END) provides instant manual kill switch via atomic flag

### 4. Local system threats

Other software on the user's machine could:
- Read the cheat's memory or config files
- Hook into the DLL's own functions
- Intercept the debug console output
- Access `Friendlydll_error.log` or `gmod_netvars.txt`

**Mitigations** (limited -- local system security is the user's responsibility):
- Config files contain no credentials (only boolean/int/float toggles)
- Error log is filtered by the error sanitizer to exclude cheat-related keywords when the sanitizer is active
- The DLL does not persist sensitive data to disk beyond config state and netvar dumps

## Secure Development Guidelines

### Memory safety

- All player-indexed arrays are bounded to `[0, 128)`. Entity arrays are bounded to `[0, 512)`.
- String copies use `strncpy_s` / `snprintf` with explicit size limits.
- Backtrack ticks are bounded by `BT_MAX_TICKS` (12) with modular ring indexing.
- Nav graph uses fixed-size arrays (`MAX_NODES=512`, `MAX_CONNECTIONS=8`, `MAX_PATH_NODES=128`) -- no heap allocation in the pathfinding hot path.
- A* iteration is capped at `MAX_ASTAR_ITERS` (100) to prevent infinite loops on malformed graphs.

### Thread safety

- All cross-thread state uses `std::atomic<>` with appropriate memory ordering (`acquire`/`release` for buffer swaps, `relaxed` for flags).
- Double-buffered data (bones, entities, view matrix) is never read and written on the same buffer simultaneously.
- The Lua engine is only accessed from the game thread (CreateMove context). Render-thread code reads only from the atomic-swapped read buffer.

### Lua sandboxing

- Every Lua script execution is wrapped in `pcall()` at the outermost level.
- Embedded scripts use idempotent installation guards (`if _fdll_X then return end`) to prevent double-installation.
- Uninstall functions restore original function references (`_fdll_orig_*`) before clearing state.
- Lua globals are prefixed with `_fdll_` to avoid namespace collisions with game addons.

### Input validation

- Netvar offsets are validated against hardcoded fallbacks on startup; mismatches are logged as warnings.
- Lua query results are parsed with tab-delimited field extraction; malformed lines are silently skipped.
- `sscanf_s` is used for structured parsing of voice exploit data with explicit buffer sizes.
- Entity indices from Lua results are bounds-checked before array access.

## Known Limitations

1. **No ASLR/DEP hardening**: The DLL does not implement its own address space randomization. It relies on the host process's memory layout.
2. **No integrity self-checks**: The DLL does not verify its own code integrity at runtime. A sufficiently privileged local attacker could patch it in memory.
3. **Lua sandbox is cooperative**: The `pcall()` wrapper prevents crashes but does not prevent a malicious server from reading `_fdll_*` globals if they know the prefix. The hook table sanitizer mitigates enumeration but not direct access by name.
4. **Config files are plaintext**: `config_io.hpp` serializes settings in cleartext. No encryption or signing is applied.
5. **Debug console is world-readable**: The allocated console window is visible to any process on the same desktop session.
6. **Error log persistence**: `Friendlydll_error.log` persists across sessions and may contain cheat-related error messages from before the error sanitizer is installed.

## Dependencies and Supply Chain

All dependencies are vendored in the `dependencies/` directory as source code. No package manager or external download is used at build time. This eliminates supply chain risks from package registries but places the burden of updates on the maintainer.

| Dependency | Vendored Version | Update Policy |
|------------|------------------|---------------|
| Dear ImGui | Source in `dependencies/imgui/` | Manual update, review diffs |
| MinHook | Source in `dependencies/minhook/` | Manual update |
| Kiero | Source in `dependencies/kiero/` | Manual update |
| FreeType | Static lib (`freetype.lib`) | Rebuild from source on update |
| spdlog | Source in `dependencies/spdlog/` | Manual update |
| Boost.Regex | Headers in `dependencies/boost/` | Manual update |

## Contact

Security reports: **the404studios@gmail.com**
