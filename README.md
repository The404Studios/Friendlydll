# Friendlydll

A feature-rich internal DLL for Garry's Mod, built with C++17. Hooks into the Source Engine via DirectX 9 and provides a full ImGui overlay with 60+ modules spanning combat, movement, ESP, DarkRP automation, Lua execution, and anti-detection.

---

## Architecture

```
Friendlydll.dll
  |
  +-- DllMain (dllmain.cpp)
  |     Worker thread -> Interface capture -> Hook init -> Lua state grab -> Scheduler
  |
  +-- D3D9 Hook (d3d9_hook.cpp)
  |     Kiero -> IDirect3DDevice9::Present -> ImGui render loop
  |
  +-- Core Hooks (core/hooks.cpp)
  |     CreateMove, DrawModelExecute, FrameStageNotify, ReadPixels
  |
  +-- Features (core/*.hpp)
  |     62 header-only modules, double-buffered data, atomic state
  |
  +-- Lua Engine (core/lua_loader.cpp, core/lua_scripts.hpp)
  |     Runtime Lua execution via CLuaShared, embedded scripts (~320 KB)
  |
  +-- Scheduler (core/scheduler.cpp)
        Async periodic tasks independent of frame timing
```

## Features

### Combat
| Module | Description |
|--------|-------------|
| **Aimbot** | Silent aim, smooth, FOV-based, bone selection, priority weighting (distance/health/threat) |
| **Triggerbot** | Configurable delay, head-only mode |
| **Backtrack** | 12-tick ring buffer, per-player bone history |
| **Resolver** | Adaptive brute-force (8 angles), jitter detection, velocity-based resolution |
| **Prediction** | Velocity/acceleration tracking, ghost skeleton rendering |
| **Recoil Control** | View punch removal |

### Visuals
| Module | Description |
|--------|-------------|
| **Player ESP** | Box, skeleton, snaplines, health bars, dormant tracking |
| **Entity ESP** | Printers, shipments, drugs, doors, weapons, money, vehicles |
| **Chams** | Model render overrides via DrawModelExecute |
| **Sound ESP** | Positional audio visualization |
| **Threat Radar** | Overhead radar overlay |
| **Heatmap** | Accumulated player position heatmap |
| **X-Ray** | See-through rendering |
| **Aim Lines** | Predicted aim trajectory |

### Movement
| Module | Description |
|--------|-------------|
| **Bunny Hop** | Auto-jump on ground contact |
| **Auto Strafe** | Legit, silent, and directional modes |
| **Edge Jump** | Jump at ledge edges |
| **Fast Stop** | Instant velocity kill |
| **Fake Lag** | Choked packet simulation |
| **Anti-Aim** | Jitter, spin, backwards, down |

### DarkRP Automation
Nine modular batches (`darkrp_lua_batch1-9.hpp`) covering:

- Warrant shield & anti-arrest
- Base alarm system
- Auto-bounty management
- Door exploit & auto-close
- Loot vacuum
- Proximity alerts
- Printer monitoring & tracking
- Auto-disguise

### Advanced Exploits
- HDR stacking, material glow, render override
- Physics prediction, CRC spoofing
- Vehicle boost, animation override, sound masking
- Death cam, net/buy capture, duplication loops
- Keypad cracker, entity steal

### Stealth & Anti-Detection
- **Panic key** (END) -- instant feature kill
- **Spectator detection** with auto-panic
- **Screenshot cleaner** -- hooks `render.Capture` / `render.ReadPixels`
- **Anti-AC bypass** layer
- Admin detection & alerts
- Fake death, name stealing, anti-kick

### Bot System
Fully autonomous bot with navigation, combat, task execution, and visual awareness (`bot_combat.hpp`, `bot_nav.hpp`, `bot_tasks.hpp`, `bot_visuals.hpp`).

### Lua Console
Built-in text editor with syntax highlighting for live Lua execution on the client state. Supports fire-and-forget, queued, and result-returning execution modes.

## Interfaces

The DLL captures and uses these Source Engine interfaces:

| Interface | Version String |
|-----------|---------------|
| IClientEntityList | `VClientEntityList003` |
| IBaseClientDLL | `VClient017` |
| CEngineClient | `VEngineClient015` |
| CLuaShared | `LUASHARED003` |
| IPrediction | `VClientPrediction001` |
| IEngineTrace | `EngineTraceClient003` |
| ICVar | `VEngineCvar007` |
| CModelRender | `VEngineModel016` |
| IGameEventManager2 | -- |
| CMaterialSystem | -- |
| CGameMovement | -- |
| IMoveHelper | -- |

## UI

ImGui-based overlay toggled with **INSERT**. Dark theme with cyan (`#00B4D8`) accents, tab navigation, particle effects, slide-in animations, and UI sound effects. Window state persists to `friendlydll_window.cfg`.

Custom FreeType font rendering with two font faces (default + editor).

## Dependencies

| Library | Purpose |
|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Overlay UI |
| [MinHook](https://github.com/TsudaKageworked/minhook) | x86/x64 API hooking |
| [Kiero](https://github.com/Rebzzel/kiero) | DirectX hook bootstrapping |
| [FreeType](https://freetype.org/) | Font rasterization |
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [Boost.Regex](https://www.boost.org/) | Pattern matching |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Lua editor with syntax highlighting |

## Building

**Requirements**: Visual Studio 2019+, Windows SDK, C++17.

1. Open the `.vcxproj` in Visual Studio.
2. Set configuration to **Release | x86** (Garry's Mod is 32-bit) or **x64** as needed.
3. Build. Output is `Friendlydll.dll`.

## Injection

Inject the compiled DLL into a running `gmod.exe` process using any standard DLL injector. The DLL self-initializes on `DLL_PROCESS_ATTACH`:

1. Opens a debug console (`Friendlydll-GMOD Console`)
2. Hooks D3D9 Present
3. Captures game interfaces
4. Resolves netvars (dumps to `gmod_netvars.txt`)
5. Installs game hooks
6. Grabs Lua state
7. Starts the scheduler

Press **END** to safely eject.

## Controls

| Key | Action |
|-----|--------|
| INSERT | Toggle menu |
| END | Panic / eject DLL |
| F3 | Toggle freecam |

## Config

150+ settings exposed via `config.hpp` as inline atomics for thread-safe access. Configs serialize to disk via `config_io.hpp`.

## Design Notes

- **Header-only features**: All 62 feature modules live in `core/*.hpp` as inline implementations for compile-time optimization and zero link overhead.
- **Double-buffered render data**: Bone cache (128 players x 128 bones), entity ESP records (512 slots), and view matrices use atomic buffer swapping for lock-free reads from the render thread.
- **Thread safety**: `std::atomic<bool>` flags for all cross-thread state (`g_inGame`, `g_panic`, `g_beingSpectated`, etc.).
- **Modular DarkRP exploits**: Nine separate batches keep exploit code isolated and easy to toggle independently.

---

**For educational and research purposes only.**
